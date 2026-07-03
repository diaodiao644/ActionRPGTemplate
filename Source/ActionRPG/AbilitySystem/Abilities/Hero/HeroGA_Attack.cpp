// 文件说明：实现攻击 GA 基类的通用解析、执行、姿态切换与收尾流程。

#include "AbilitySystem/Abilities/Hero/HeroGA_Attack.h"

#include "ActionGameplayTags.h"

#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "GameBase/ActionPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroGA_Attack, Log, All);

UHeroGA_Attack::UHeroGA_Attack()
	: Super()
{
	ActivationPolicy = EActionAbilityActivationPolicy::OnInput;

	// 基类本身不直接代表某一种具体攻击。
	// 具体请求标签、能力标签和固定武器槽，由各个独立派生类在构造函数里写入。
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_Attack);
	ActivationOwnedTags.AddTag(ActionGameplayTags::State_Ability_Attack_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Defense_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Dodge_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Execution_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_SpiritSkill_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_WeaponSwitch_Active);

	// 攻击属于基础战斗行为：默认不主动打断别人，但会被更高优先级行为压住。
	// 当前基线优先级下调到 18，让 SpiritSkill 在同属主动链时具备更明确的压制权。
	AbilityPriority = 18;
	bCanInterruptLowerPriorityAbilities = false;
	bCanInterruptSamePriorityAbilities = false;
	bCanBeInterruptedByHigherPriority = true;
	bCanBeInterruptedBySamePriority = false;
	CombatReactAbilityRule.bAllowActivationDuringRecoveryCancelWindow = true;
}

bool UHeroGA_Attack::ValidateRelationshipActivationPreconditions(FString& OutFailureReason)
{
	// 关系预检阶段只做“现在这条攻击 GA 是否有资格被放行”的判断，
	// 不在这里真正改战斗状态，避免预检失败时留下半套运行时副作用。
	// 这里的职责是补充攻击主链自己的前置条件，不替代 GAS 提交检查，
	// 也不替代真正激活后那条“请求 -> 解析 -> 执行”的正式流程。
	if (!ValidateHeroRuntimeObjects(OutFailureReason, true, true))
	{
		return false;
	}

	AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo();
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	UHeroAttackComponent* HeroAttackComponent = GetHeroAttackComponentFromActorInfo();
	const UHeroEquipmentComponent* HeroEquipmentComponent = HeroCharacter
		? HeroCharacter->FindComponentByClass<UHeroEquipmentComponent>()
		: nullptr;
	if (!HeroCombatComponent || !HeroAttackComponent || !AttackRequestTag.IsValid())
	{
		// 没有战斗组件或当前这条攻击 GA 没有绑定请求标签时，
		// 说明这次激活连“我要执行哪类攻击”都还没建立完成，必须在最前面拦掉。
		OutFailureReason = TEXT("attack request tag, combat component or attack component is invalid");
		return false;
	}

	if (ExpectedLoadoutSlot != EHeroWeaponLoadoutSlot::Invalid
		&& HeroEquipmentComponent
		&& HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot() != ExpectedLoadoutSlot)
	{
		// 攻击 GA 已经按固定武器槽拆开后，
		// 每一条 GA 都只应该在自己的目标槽位下被激活，避免不同槽位的攻击 GA 串用同一把武器配置。
		OutFailureReason = TEXT("attack loadout slot does not match expected slot");
		return false;
	}

	UDataAsset_WeaponDefinition* CurrentWeaponDefinition = HeroCombatComponent->GetCurrentWeaponDefinition();
	if (!CurrentWeaponDefinition)
	{
		// 没有当前武器定义，就无法继续向下解析攻击分支、动画和命中配置。
		OutFailureReason = TEXT("current weapon definition is invalid");
		return false;
	}

	FActionResolvedAttackExecutionConfig ResolvedConfig;
	if (!HeroAttackComponent->TryResolveAttackExecutionConfigByRequestTag(AttackRequestTag, ResolvedConfig))
	{
		// 这里提前做一次“当前连段索引下是否真有可执行配置”的校验，
		// 目的是在关系系统放行前就把空分支、空蒙太奇、空命中配置这类问题拦掉。
		OutFailureReason = TEXT("attack execution config cannot be resolved");
		return false;
	}

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_DodgeCounter
		&& !HeroCombatComponent->IsDodgeCounterAvailable()
		&& !HeroAttackComponent->HasPendingDodgeCounterExecutionAuthorization())
	{
		// 闪避反击必须来自一次真实存在的闪反资格。
		// 这里允许“实时资格”与“已锁存资格”二选一成立，覆盖激活与正式播放之间的短时序。
		OutFailureReason = TEXT("dodge counter is not available");
		return false;
	}

	return true;
}

void UHeroGA_Attack::ActivateAbility(
	FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 这批字段都只描述“本次激活走到了哪一步”。
	// 正式攻击状态仍在组件侧；GA 本地只缓存短生命周期时序，方便把各种失败和结束路径统一收口。
	// 每次进入攻击前都要把本次运行时状态清零，
	// 避免上一段攻击遗留的连段收尾标记或闪避反击授权串到下一次激活里。
	// 这些状态都只服务“本次激活”的时序，不应该跨越多次 ActivateAbility 持续保留。
	bAttackAbilityFinished = false;
	bShouldResetComboIndexOnFinalizeIfNotChained = false;
	bAttackExecutionStarted = false;
	bSimpleTurnAssistAttempted = false;
	bDodgeCounterExecutionAuthorized = false;

	AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo();
	AActionPlayerController* HeroController = GetHeroControllerFromActorInfo();
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	UHeroAttackComponent* HeroAttackComponent = GetHeroAttackComponentFromActorInfo();
	if (!HeroCharacter || !HeroController || !HeroCombatComponent)
	{
		Debug::Print(TEXT("[GA][Attack] Begin Failed: Hero/Controller/CombatComponent invalid"), FColor::Red, 2.0f);
		UE_LOG(LogHeroGA_Attack, Warning, TEXT("Attack activate failed: hero/controller/combat component is null."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const UHeroEquipmentComponent* HeroEquipmentComponent = HeroCharacter->FindComponentByClass<UHeroEquipmentComponent>();
	if (ExpectedLoadoutSlot != EHeroWeaponLoadoutSlot::Invalid
		&& HeroEquipmentComponent
		&& HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot() != ExpectedLoadoutSlot)
	{
		// 这里再次做槽位保护，是为了防止运行时标签、输入映射或授予关系被误配后激活到错误槽位 GA。
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Attack] Begin Failed: CurrentSlot=%d ExpectedSlot=%d Request=%s"),
				static_cast<int32>(HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot()),
				static_cast<int32>(ExpectedLoadoutSlot),
				*AttackRequestTag.ToString()),
			FColor::Red,
			2.0f);
		UE_LOG(
			LogHeroGA_Attack,
			Warning,
			TEXT("Attack activate failed: current slot does not match expected slot. CurrentSlot=%d ExpectedSlot=%d RequestTag=%s"),
			static_cast<int32>(HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot()),
			static_cast<int32>(ExpectedLoadoutSlot),
			*AttackRequestTag.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][Attack] Begin Slot=%d Request=%s Input=%s"),
			HeroEquipmentComponent ? static_cast<int32>(HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot()) : static_cast<int32>(EHeroWeaponLoadoutSlot::Invalid),
			*AttackRequestTag.ToString(),
			*GetCurrentAttackInputTag().ToString()),
		FColor::Cyan,
		2.0f);

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_DodgeCounter)
	{
		// 闪避反击资格只在真正开始攻击前消费一次。
		// 这里的“锁住”本质上是在 GA 本地缓存一份本次可以出闪反的授权结果，
		// 后续真正执行攻击时再决定是否正式消耗外部资格。
		bDodgeCounterExecutionAuthorized =
			HeroAttackComponent->ConsumePendingDodgeCounterExecutionAuthorization()
			|| HeroCombatComponent->IsDodgeCounterAvailable();
	}

	if (!AttackRequestTag.IsValid() || !TryResolveAndExecuteAttack())
	{
		// 进入这里说明这次激活没能成功启动正式攻击执行：
		// 可能是请求失效、姿态切换失败、分支解析失败，或最终执行资格被撤销。
		// 这些失败统一走 EndAbility / FinalizeAttackAbility，避免各分支各收一套残局。
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Attack] Execute Failed: Request=%s Input=%s"),
				*AttackRequestTag.ToString(),
				*GetCurrentAttackInputTag().ToString()),
			FColor::Red,
			2.0f);
		UE_LOG(
			LogHeroGA_Attack,
			Warning,
			TEXT("Attack execute failed: invalid request tag or attack resolve failed. RequestTag=%s InputTag=%s"),
			*AttackRequestTag.ToString(),
			*GetCurrentAttackInputTag().ToString());

		if (HeroAttackComponent)
		{
			// 这次激活如果在真正出招前失败，就把上一段攻击临时保留下来的连段索引回滚掉。
			HeroAttackComponent->RestoreComboIndexAfterFailedAttackStart();
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UHeroGA_Attack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!bAttackAbilityFinished)
	{
		// 攻击结束可能来自正常播完、中断、取消或主动 EndAbility。
		// 因此这里统一先补一次正式收尾，确保所有出口都能落到同一套状态恢复逻辑。
		FinalizeAttackAbility();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHeroGA_Attack::OnHeroMontageCompleted(const FName MontageContext)
{
	FinishAttackAbility();
}

void UHeroGA_Attack::OnHeroMontageBlendOut(const FName MontageContext)
{
	FinishAttackAbility();
}

void UHeroGA_Attack::OnHeroMontageInterrupted(FName MontageContext)
{
	FinishAttackAbility();
}

void UHeroGA_Attack::OnHeroMontageCancelled(FName MontageContext)
{
	FinishAttackAbility();
}

void UHeroGA_Attack::FinishAttackAbility()
{
	if (bAttackAbilityFinished)
	{
		// 攻击蒙太奇的 Completed / BlendOut / Interrupted / Cancelled 可能在边界情况下连续到来。
		// 这里用单一布尔锁死收尾，避免重复重置连段、重复恢复输入或重复关闭武器检测。
		return;
	}

	// 这里是所有蒙太奇回调最终汇合到的单一结束入口。
	// 先做正式收尾，再交给蓝图层的 EndAbility，避免回调次序不同导致状态残留。
	FinalizeAttackAbility();
	K2_EndAbility();
}

void UHeroGA_Attack::FinalizeAttackAbility()
{
	if (bAttackAbilityFinished)
	{
		return;
	}

	bAttackAbilityFinished = true;

	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		// ActiveDamageContext 只应该覆盖“正式攻击已经起手并正在承担伤害语义”的阶段。
		// 无论本次结束是自然播完、取消还是中断，只要收尾落地，都要把这层上下文统一清掉。
		HeroCombatComponent->ClearActiveDamageContext();
	}

	if (UHeroAttackComponent* HeroAttackComponent = GetHeroAttackComponentFromActorInfo())
	{
		// 具体的攻击运行态清理已经下沉到攻击组件：
		// 包括关闭命中检测、恢复攻击开关、必要时回退连段、恢复移动状态与延迟恢复输入。
		// 这里把“是否需要在未连上时回退 ComboIndex”的最终决策一并交给组件收口，
		// 避免起手阶段就过早改写连段结果。
		HeroAttackComponent->FinalizeAttackExecutionRuntime(bShouldResetComboIndexOnFinalizeIfNotChained);
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][Attack] End Request=%s MoveState=%d"),
			*AttackRequestTag.ToString(),
			GetHeroControllerFromActorInfo() ? static_cast<int32>(GetHeroControllerFromActorInfo()->GetMoveState()) : -1),
		FColor::Green,
		2.0f);
}

FGameplayTag UHeroGA_Attack::GetCurrentAttackInputTag() const
{
	// 当前输入标签来自本次激活 Spec 的动态标签。
	// 它主要用于调试显示，让日志能看出“这次攻击是被哪条输入请求触发的”。
	return GetCurrentAbilitySpec() ? GetCurrentAbilitySpec()->DynamicAbilityTags.First() : FGameplayTag();
}

bool UHeroGA_Attack::TryResolveAndExecuteAttack()
{
	// 这条链统一负责把“本次攻击请求”落成一次真正的执行：
	// 1. 先做一次朝向修正；
	// 2. 再把请求标签解析成当前武器和当前 ComboIndex 下的一份正式执行配置；
	// 3. 在真正播放前做最后一次资格闸门；
	// 4. 通过后才进入 ExecuteResolvedAttack 写正式运行态。
	AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo();
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	UHeroAttackComponent* HeroAttackComponent = GetHeroAttackComponentFromActorInfo();
	UHeroTargetingComponent* HeroTargetingComponent = HeroCharacter ? HeroCharacter->GetHeroTargetingComponent() : nullptr;
	UDataAsset_WeaponDefinition* CurrentWeaponDefinition =
		HeroCombatComponent ? HeroCombatComponent->GetCurrentWeaponDefinition() : nullptr;
	if (!HeroCombatComponent || !HeroAttackComponent || !CurrentWeaponDefinition || !AttackRequestTag.IsValid())
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Attack] Resolve Failed: Request=%s WeaponValid=%d"),
				*AttackRequestTag.ToString(),
				CurrentWeaponDefinition != nullptr ? 1 : 0),
			FColor::Red,
			2.0f);
		UE_LOG(
			LogHeroGA_Attack,
			Warning,
			TEXT("Attack resolve failed: combat component, weapon definition or request tag invalid. RequestTag=%s WeaponValid=%d"),
			*AttackRequestTag.ToString(),
			CurrentWeaponDefinition != nullptr);
		return false;
	}

	if (!bSimpleTurnAssistAttempted)
	{
		// 简单转向辅助只服务这次起手朝向修正，不应该在同次激活里被重复叠加。
		// 因此无论最终是否真的打中目标，都只尝试一次。
		bSimpleTurnAssistAttempted = true;
		if (HeroTargetingComponent)
		{
			HeroTargetingComponent->TryApplyOffensiveFacing(EActionSimpleTurnAssistTriggerSource::Attack);
		}
	}

	// 这里直接进入“请求标签 -> 当前武器分支 -> 当前连段执行配置”的主解析链。
	FActionResolvedAttackExecutionConfig ResolvedConfig;
	if (!HeroAttackComponent->TryResolveAttackExecutionConfigByRequestTag(AttackRequestTag, ResolvedConfig))
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Attack] Branch Failed: Request=%s Branch=%s Combo=%d"),
				*AttackRequestTag.ToString(),
				*CurrentWeaponDefinition->ResolveAttackBranchTag(AttackRequestTag).ToString(),
				HeroCombatComponent->GetComboIndex()),
			FColor::Red,
			2.0f);
		const FActionWeaponAnimationConfig& AnimationConfig = CurrentWeaponDefinition->GetAnimationConfig();
		UE_LOG(
			LogHeroGA_Attack,
			Warning,
			TEXT("Attack branch resolve failed: no playable attack config. WeaponTag=%s RequestTag=%s BranchTag=%s ComboIndex=%d AttackEntryConfigCount=%d"),
			*CurrentWeaponDefinition->WeaponTag.ToString(),
			*AttackRequestTag.ToString(),
			*CurrentWeaponDefinition->ResolveAttackBranchTag(AttackRequestTag).ToString(),
			HeroCombatComponent->GetComboIndex(),
			AnimationConfig.AttackEntryConfigs.Num());
		return false;
	}
	// 进入这里说明“这次输入请求”已经被翻译成一份可执行配置：
	// 它已经包含当前要播哪段蒙太奇、是否推进连段、是否重置连段、命中配置和临时战斗修正效果。

	// 真正开始播放攻击前，再做一次“这条请求此刻是否仍然允许执行”的校验。
	// 这样闪避反击这类受窗口影响的请求，不会因为中间插入姿态切换或其它时序而错误放行。
	if (!HeroAttackComponent->IsAttackExecutionStillAuthorized(AttackRequestTag, bDodgeCounterExecutionAuthorized))
	{
		// 这一步主要兜住“请求建立后、真正播放前资格失效”的情况，
		// 尤其是闪避反击这类强依赖瞬时资格的攻击，不能只在激活开头检查一次。
		Debug::Print(TEXT("[GA][Attack] Resolve Failed: dodge counter not available"), FColor::Red, 2.0f);
		return false;
	}

	return ExecuteResolvedAttack(ResolvedConfig);
}

bool UHeroGA_Attack::ExecuteResolvedAttack(const FActionResolvedAttackExecutionConfig& InResolvedConfig)
{
	// 进入这里说明“请求标签 -> 武器分支 -> 具体段配置”的解析已经成功。
	// 从这一刻开始，代码会正式写入组件状态源、开启伤害上下文并把蒙太奇执行权交给当前段攻击。
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	UHeroAttackComponent* HeroAttackComponent = GetHeroAttackComponentFromActorInfo();
	if (!HeroCombatComponent || !HeroAttackComponent || !InResolvedConfig.IsValid())
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Attack] Execute Failed: Branch=%s MontageValid=%d"),
				*InResolvedConfig.BranchTag.ToString(),
				InResolvedConfig.ResolvedMontage != nullptr ? 1 : 0),
			FColor::Red,
			2.0f);
		UE_LOG(
			LogHeroGA_Attack,
			Warning,
			TEXT("Attack execute failed: combat component is null or resolved config invalid. BranchTag=%s MontageValid=%d"),
			*InResolvedConfig.BranchTag.ToString(),
			InResolvedConfig.ResolvedMontage != nullptr);
		return false;
	}

	if (InResolvedConfig.BranchTag == ActionGameplayTags::Attack_Branch_Sprint)
	{
		TryApplySprintFacingFromMoveInput();
	}

	if (!PlayHeroMontage(
		InResolvedConfig.ResolvedMontage,
		TEXT("PlayAttackMontageAndWait"),
		UHeroAttackComponent::GetMontageContextNameForAttackBranch(InResolvedConfig.BranchTag)))
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Attack] Montage Failed: Branch=%s Montage=%s"),
				*InResolvedConfig.BranchTag.ToString(),
				*GetNameSafe(InResolvedConfig.ResolvedMontage)),
			FColor::Red,
			2.0f);
		UE_LOG(
			LogHeroGA_Attack,
			Warning,
			TEXT("Attack montage play failed. BranchTag=%s Montage=%s"),
			*InResolvedConfig.BranchTag.ToString(),
			*GetNameSafe(InResolvedConfig.ResolvedMontage));
		return false;
	}
	// 从这里开始，解析阶段正式切换到执行阶段。
	// 后续所有状态写入都以“蒙太奇已经成功开始播放”为前提，避免未播放成功时污染战斗组件。

	// 新一段攻击真正开始前，先把上一段残留的衔接窗口和取消窗口收掉。
	// 这样同一时刻只会存在当前这一段攻击自己的窗口状态，避免旧窗口跨段残留。
	HeroAttackComponent->ApplyAttackExecutionStartedForRequest(AttackRequestTag);
	// 一旦已经正式切入执行，本地缓存的闪反授权就没有继续保留的必要了；
	// 后续如果还要判断闪反资格，应当以这次攻击已经启动的事实为准，而不是旧授权标记。
	bDodgeCounterExecutionAuthorized = false;

	bShouldResetComboIndexOnFinalizeIfNotChained =
		InResolvedConfig.bUseComboIndex
		&& InResolvedConfig.bAdvanceComboIndexOnPlay
		&& !InResolvedConfig.bResetComboIndexOnPlay;
	// 这个标记只描述“如果本段推进了连段，但后面没有继续接上，就在最终收尾时回退索引”。
	// 它不是立即执行的重置命令，而是交给 FinalizeAttackAbility 在整段动作真正结束后统一判断。
	// 这样 Reset/Advance 的执行时机就能和攻击实际是否连上下一段保持一致，而不是在起手瞬间提前定死最终结果。

	HeroAttackComponent->ApplyResolvedAttackExecutionState(InResolvedConfig);
	bAttackExecutionStarted = true;
	// DamageContext 代表“当前是谁在造成这段攻击伤害”。
	// 它只在正式攻击起手成功后建立，并在 FinalizeAttackAbility 统一清理。
	HeroCombatComponent->BeginActiveDamageContext(
		GetCurrentAbilitySpec() ? GetCurrentAbilitySpec()->Level : 1,
		SpecificAttackAbilityTag.IsValid() ? SpecificAttackAbilityTag : AttackRequestTag);

	Debug::Print(
		FString::Printf(
			TEXT("[GA][Attack] Play Branch=%s ComboIndex=%d Reset=%d Advance=%d"),
			*InResolvedConfig.BranchTag.ToString(),
			HeroCombatComponent->GetComboIndex(),
			InResolvedConfig.bResetComboIndexOnPlay ? 1 : 0,
			InResolvedConfig.bAdvanceComboIndexOnPlay ? 1 : 0),
		FColor::Cyan,
		2.0f);

	return true;
}

bool UHeroGA_Attack::TryApplySprintFacingFromMoveInput()
{
	AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo();
	AActionPlayerController* HeroController = GetHeroControllerFromActorInfo();
	if (!HeroCharacter)
	{
		return false;
	}

	FRotator DesiredYawRotation = HeroCharacter->GetActorRotation();
	if (HeroController && HeroController->HasMoveInput())
	{
		const FRotator InputDirection = HeroController->GetInputDirection();
		const FRotator InputYawRotation(0.f, InputDirection.Yaw, 0.f);
		if (!InputYawRotation.Vector().GetSafeNormal2D().IsNearlyZero())
		{
			DesiredYawRotation = InputYawRotation;
		}
	}

	DesiredYawRotation.Pitch = 0.f;
	DesiredYawRotation.Roll = 0.f;
	HeroCharacter->SetActorRotation(DesiredYawRotation);
	return true;
}

void UHeroGA_Attack::SetAttackRequestTag(const FGameplayTag& InRequestTag)
{
	// 派生类在构造阶段用它声明“这条 GA 天生负责哪一种攻击请求”。
	AttackRequestTag = InRequestTag;
}

void UHeroGA_Attack::SetSpecificAttackAbilityTag(const FGameplayTag& InAbilityTag)
{
	if (SpecificAttackAbilityTag.IsValid())
	{
		// 派生类若重复初始化，先移除旧专属标签，避免 AbilityTags 中残留旧身份。
		// 这一步主要保护构造期的重复写入，不让同一条攻击 GA 同时挂着多份彼此冲突的专属身份。
		AbilityTags.RemoveTag(SpecificAttackAbilityTag);
	}

	SpecificAttackAbilityTag = InAbilityTag;
	if (SpecificAttackAbilityTag.IsValid())
	{
		AbilityTags.AddTag(SpecificAttackAbilityTag);
	}
}

void UHeroGA_Attack::SetExpectedLoadoutSlot(const EHeroWeaponLoadoutSlot InExpectedLoadoutSlot)
{
	// 固定四槽架构下，每条攻击 GA 只服务自己的目标槽位。
	ExpectedLoadoutSlot = InExpectedLoadoutSlot;
}

void UHeroGA_Attack::InitializeAttackAbility(
	const FGameplayTag& InRequestTag,
	const FGameplayTag& InAbilityTag,
	const EHeroWeaponLoadoutSlot InExpectedLoadoutSlot)
{
	// 派生类构造函数统一走这个入口，避免遗漏请求标签、专属能力标签或目标槽位三者之一。
	SetAttackRequestTag(InRequestTag);
	SetSpecificAttackAbilityTag(InAbilityTag);
	SetExpectedLoadoutSlot(InExpectedLoadoutSlot);
}
