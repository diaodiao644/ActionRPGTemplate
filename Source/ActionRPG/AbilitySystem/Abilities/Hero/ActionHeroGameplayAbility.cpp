// 文件说明：实现英雄专用 GameplayAbility 基类的共享工具逻辑。

#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "Debug/ActionDebugHelper.h"
#include "GameBase/ActionPlayerController.h"

namespace
{
	FGameplayTag ResolveHeroAbilityCombatReactInputTag(const UActionHeroGameplayAbility* Ability)
	{
		if (!Ability)
		{
			return FGameplayTag();
		}

		const FGameplayTagContainer& AbilityTags = Ability->GetAbilityIdentityTags();
		if (AbilityTags.HasTagExact(ActionGameplayTags::Player_Ability_CombatModeOrDefense))
		{
			return ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense;
		}

		if (AbilityTags.HasTagExact(ActionGameplayTags::Player_Ability_Dodge))
		{
			return ActionGameplayTags::InputTag_GameplayAbility_Dodge;
		}

		if (AbilityTags.HasTagExact(ActionGameplayTags::Player_Ability_Execution))
		{
			return ActionGameplayTags::InputTag_GameplayAbility_Execution;
		}

		if (AbilityTags.HasTagExact(ActionGameplayTags::Player_Ability_WeaponSwitch))
		{
			return ActionGameplayTags::InputTag_GameplayAbility_WeaponSwitch;
		}

		if (AbilityTags.HasTagExact(ActionGameplayTags::Player_Ability_ProjectileSwitch))
		{
			return ActionGameplayTags::InputTag_GameplayAbility_ProjectileSwitch;
		}

		if (AbilityTags.HasTagExact(ActionGameplayTags::Player_Ability_Attack))
		{
			return ActionGameplayTags::InputTag_GameplayAbility_Attack;
		}

		return FGameplayTag();
	}
}

UActionHeroGameplayAbility::UActionHeroGameplayAbility()
	: Super()
{
	// 英雄战斗 GA 默认都参与统一的关系裁决，后续由各自构造函数补优先级与打断规则。
	// 同时默认启用受击规则，让大多数战斗 Ability 天然接入受击阶段门槛判定。
	bUseAbilityRelationshipSystem = true;
	CombatReactAbilityRule.bUseCombatReactRule = true;
}

FGameplayTag UActionHeroGameplayAbility::GetPrimaryInputTagForCombatReact() const
{
	return ResolveHeroAbilityCombatReactInputTag(this);
}

bool UActionHeroGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// 先沿用 GameplayAbilityBase 和 GAS 自带的基础激活条件，
	// 只有这些都通过后，英雄专用层才继续补“必须是英雄角色 + 受击规则放行”两项共性限制。
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActionHeroCharacter* HeroCharacter =
		ActorInfo ? Cast<AActionHeroCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!HeroCharacter)
	{
		// 英雄专用基类只服务英雄角色。
		// Avatar 不是英雄角色时，后续所有角色侧辅助访问都会失去意义，因此直接拒绝激活。
		return false;
	}

	// 共享基类在最外层统一补一轮“当前受击状态是否允许激活”判断。
	// 这样攻击、闪避、处决、恢复类 GA 都不需要再各自重复接这一层基础受击规则。
	const UActionCombatReactComponent* CombatReactComponent = HeroCharacter->GetActionCombatReactComponent();
	return IsActivationAllowedByCombatReact(CombatReactComponent);
}

void UActionHeroGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 英雄 GA 统一在基类结束时清理临时战斗修正效果，
	// 防止子类提前 return 或中断时遗留易伤、霸体、无敌之类的运行时状态。
	// 这样子类就可以专注于“什么时候挂效果”，而不必每条 GA 都重复写一套移除逻辑。
	ClearHeroCombatModifierEffects();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

AActionHeroCharacter* UActionHeroGameplayAbility::GetHeroCharacterFromActorInfo()
{
	if (CachedHeroCharacter.IsValid())
	{
		// 同一份 Ability 实例在一次激活期间可能多次访问角色对象，
		// 命中缓存后直接返回，避免重复从 ActorInfo 解引用和 Cast。
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetAvatarActorFromActorInfo());
	// 这里缓存的是弱引用，因此不会人为延长角色生命周期。
	// 一旦角色侧对象被销毁，后续再次访问时仍会自然失效并重新判空。
	return CachedHeroCharacter.Get();
}

AActionPlayerController* UActionHeroGameplayAbility::GetHeroControllerFromActorInfo()
{
	if (CachedHeroController.IsValid())
	{
		// 控制器生命周期通常比单次 Ability 更稳定，缓存后可复用整段技能链。
		return CachedHeroController.Get();
	}

	if (AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo())
	{
		CachedHeroController = Cast<AActionPlayerController>(HeroCharacter->GetController());
	}

	// 控制器查询统一收在这里后，子类就不需要各自决定
	// “是从 Pawn 拿 Controller，还是从 ActorInfo 里自己再找一遍”。
	return CachedHeroController.Get();
}

UHeroCombatComponent* UActionHeroGameplayAbility::GetHeroCombatComponentFromActorInfo()
{
	if (CachedHeroCombatComponent.IsValid())
	{
		// 战斗组件会被攻击、闪避、处决等大量能力反复访问，
		// 因此这里也统一走缓存，避免每个子类都自己再做一次局部缓存。
		return CachedHeroCombatComponent.Get();
	}

	if (AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo())
	{
		CachedHeroCombatComponent = HeroCharacter->FindComponentByClass<UHeroCombatComponent>();
	}

	// 这层返回的不是“战斗系统的全局单例”，而是当前英雄身上的那一份战斗组件。
	// 因此所有输入恢复、窗口读写和事件广播都会天然绑定到当前执行者上下文。
	return CachedHeroCombatComponent.Get();
}

UHeroCombatInputComponent* UActionHeroGameplayAbility::GetHeroCombatInputComponentFromActorInfo()
{
	if (CachedHeroCombatInputComponent.IsValid())
	{
		return CachedHeroCombatInputComponent.Get();
	}

	if (AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo())
	{
		CachedHeroCombatInputComponent = HeroCharacter->GetHeroCombatInputComponent();
	}

	return CachedHeroCombatInputComponent.Get();
}

UHeroLoadoutRuntimeComponent* UActionHeroGameplayAbility::GetHeroLoadoutRuntimeComponentFromActorInfo()
{
	if (CachedHeroLoadoutRuntimeComponent.IsValid())
	{
		return CachedHeroLoadoutRuntimeComponent.Get();
	}

	if (AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo())
	{
		CachedHeroLoadoutRuntimeComponent = HeroCharacter->FindComponentByClass<UHeroLoadoutRuntimeComponent>();
	}

	return CachedHeroLoadoutRuntimeComponent.Get();
}

UHeroLoadoutContextComponent* UActionHeroGameplayAbility::GetHeroLoadoutContextComponentFromActorInfo()
{
	if (CachedHeroLoadoutContextComponent.IsValid())
	{
		return CachedHeroLoadoutContextComponent.Get();
	}

	if (AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo())
	{
		CachedHeroLoadoutContextComponent = HeroCharacter->FindComponentByClass<UHeroLoadoutContextComponent>();
	}

	return CachedHeroLoadoutContextComponent.Get();
}

UHeroAttackComponent* UActionHeroGameplayAbility::GetHeroAttackComponentFromActorInfo()
{
	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		return HeroCombatComponent->GetOwningHeroAttackComponent();
	}

	return nullptr;
}

UHeroDefenseComponent* UActionHeroGameplayAbility::GetHeroDefenseComponentFromActorInfo()
{
	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		return HeroCombatComponent->GetOwningHeroDefenseComponent();
	}

	return nullptr;
}

UHeroWeaponSwitchComponent* UActionHeroGameplayAbility::GetHeroWeaponSwitchComponentFromActorInfo()
{
	if (AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo())
	{
		return HeroCharacter->GetHeroWeaponSwitchComponent();
	}

	return nullptr;
}

FString UActionHeroGameplayAbility::DescribeCurrentCombatReactDebug() const
{
	const AActionHeroCharacter* HeroCharacter =
		CurrentActorInfo ? Cast<AActionHeroCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const UActionCombatReactComponent* CombatReactComponent =
		HeroCharacter ? HeroCharacter->GetActionCombatReactComponent() : nullptr;
	const UHeroCombatComponent* HeroCombatComponent =
		HeroCharacter ? HeroCharacter->GetHeroCombatComponent() : nullptr;

	TArray<FString> DebugLines;
	// 这里输出的是“当前这次联调时，人该怎么理解整条判定链”的解释文本，
	// 不是新的规则来源；真正的激活与打断规则仍以基类判定和组件运行态为准。
	// 调试文本统一按“能力自身配置 -> 当前受击组件状态 -> 激活判定 -> 打断判定”的顺序输出，
	// 这样屏幕打印时阅读顺序会和排查思路保持一致。
	DebugLines.Add(FString::Printf(TEXT("Ability=%s"), *GetAbilityDebugName()));
	DebugLines.Add(DescribeCombatReactRule());
	DebugLines.Add(CombatReactComponent
		? CombatReactComponent->DescribeCurrentCombatReactState()
		: TEXT("受击状态：未找到受击组件。"));
	DebugLines.Add(DescribeCombatReactActivationDecision(CombatReactComponent));
	DebugLines.Add(DescribeCombatReactInterruptDecision(CombatReactComponent));

	const FGameplayTag DebugInputTag = GetPrimaryInputTagForCombatReact();
	if (HeroCombatComponent && DebugInputTag.IsValid())
	{
		DebugLines.Add(HeroCombatComponent->DescribeNonAttackInputGateForDebug(DebugInputTag));
	}

	return FString::Join(DebugLines, TEXT("\n"));
}

void UActionHeroGameplayAbility::PrintCurrentCombatReactDebug() const
{
	Debug::Print(DescribeCurrentCombatReactDebug(), FColor::Green, 5.0f);
}

bool UActionHeroGameplayAbility::PlayHeroMontage(UAnimMontage* InMontage, const FName TaskName, const FName MontageContext)
{
	if (!InMontage)
	{
		// 这里直接把“没有蒙太奇资源”视为失败，由调用方决定后续是回退、取消还是跳过演出。
		return false;
	}

	// 统一通过共享任务入口创建蒙太奇播放节点，减少各个 Ability 的重复绑定代码。
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TaskName, InMontage);
	if (!MontageTask)
	{
		// 任务对象都没创建出来时，后续就没有任何可靠的 Completed / Interrupted 回调来源。
		return false;
	}

	// 缓存当前蒙太奇上下文，后续在共享回调里再分发给子类处理。
	// 子类只关心“这是攻击蒙太奇还是切武蒙太奇”，不需要再自己维护一套任务绑定。
	ActiveMontageContext = MontageContext;
	ActiveMontageTask = MontageTask;
	ActiveHeroMontageAsset = InMontage;
	// 共享基类把任务和上下文都记录下来，
	// 这样子类只需要实现“这个上下文完成后我该做什么”，不用自己重复维护任务句柄。
	// 这也是为什么攻击、闪避、防御、处决都能复用同一套蒙太奇回调签名，
	// 但仍然能区分“当前到底是哪一段蒙太奇”。

	// 共享基类统一接住 AbilityTask 的四类正式回调，再按 ActiveMontageContext 分发给子类。
	// 这里绑定的是“这次 AT 返回了什么结果”，不是新的长期业务状态源。
	MontageTask->OnCompleted.AddDynamic(this, &UActionHeroGameplayAbility::HandleSharedMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UActionHeroGameplayAbility::HandleSharedMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &UActionHeroGameplayAbility::HandleSharedMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UActionHeroGameplayAbility::HandleSharedMontageCancelled);
	MontageTask->Activate();
	return true;
}

bool UActionHeroGameplayAbility::StopActiveHeroMontageTaskAndCurrentMontage(
	const bool bClearRunningMontageReference)
{
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	UAnimMontage* CurrentRunningMontage =
		HeroCombatComponent ? HeroCombatComponent->GetCurrentRunningAnimMontage() : nullptr;
	const bool bHadActiveMontageTask = ActiveMontageTask.IsValid();

	if (UAbilityTask_PlayMontageAndWait* MontageTask = ActiveMontageTask.Get())
	{
		// 立即切段前先拆掉旧段共享任务的回调，避免旧蒙太奇停播时把回调继续分发回当前 Ability。
		// 这一步只做共享 AbilityTask 解绑与旧回调清理，不重建第二套蒙太奇运行态。
		MontageTask->OnCompleted.Clear();
		MontageTask->OnBlendOut.Clear();
		MontageTask->OnInterrupted.Clear();
		MontageTask->OnCancelled.Clear();
		MontageTask->EndTask();
		ActiveMontageTask = nullptr;
	}

	ActiveHeroMontageAsset = nullptr;

	bool bStoppedCurrentMontage = false;
	if (CurrentRunningMontage)
	{
		if (UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetActionAbilitySystemComponentFromActorInfo())
		{
			ActionAbilitySystemComponent->StopMontageIfCurrent(*CurrentRunningMontage);
			bStoppedCurrentMontage = true;
		}

		if (bClearRunningMontageReference && HeroCombatComponent)
		{
			// 清掉旧段蒙太奇引用后，旧 notify 的 End 回调就不会再命中当前正式运行态。
			HeroCombatComponent->ClearRunningAnimationReactGuardContextIfMatches(CurrentRunningMontage);
			HeroCombatComponent->ClearRunningAnimMontageReferenceIfMatches(CurrentRunningMontage);
		}
	}

	return bHadActiveMontageTask || bStoppedCurrentMontage;
}

void UActionHeroGameplayAbility::BroadcastHeroCombatEvent(FGameplayTag EventTag)
{
	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		// 事件仍由战斗组件统一发出，目的是保证：
		// 1. 所有战斗事件出口一致；
		// 2. UI、调试和其它系统只需要监听战斗组件这一条总线；
		// 3. GA 不直接和外部系统形成过多耦合。
		// 统一把战斗事件广播职责留给战斗组件，避免 Ability 自己拼装事件数据。
		// 这样所有战斗事件最终都会走到同一条 HeroCombatComponent 事件链里，来源保持一致。
		HeroCombatComponent->BroadcastCombatEvent(EventTag);
	}
}

bool UActionHeroGameplayAbility::ConsumeHeroBufferedInput()
{
	if (UHeroCombatInputComponent* HeroCombatInputComponent = GetHeroCombatInputComponentFromActorInfo())
	{
		// 能力结束前尝试消费一次缓冲输入，让玩家在硬直尾帧提前输入也能顺畅衔接。
		// 这里不自己决定是否真的能放，而是把最终判断继续交给正式输入运行态组件统一裁决。
		return HeroCombatInputComponent->ConsumeBufferedInput();
	}

	return false;
}

void UActionHeroGameplayAbility::RequestHeroBufferedInputConsumeNextTick()
{
	if (UHeroCombatInputComponent* HeroCombatInputComponent = GetHeroCombatInputComponentFromActorInfo())
	{
		// 某些恢复/演出能力会在当前帧末尾才真正释放输入，
		// 因此这里允许把“消费缓冲输入”延后一帧，再交给正式输入组件统一处理。
		HeroCombatInputComponent->RequestConsumeBufferedInputOnNextTick();
	}
}

UAnimMontage* UActionHeroGameplayAbility::GetCurrentHeroMontageAsset() const
{
	return ActiveHeroMontageAsset.Get();
}

FGameplayAbilitySpecHandle UActionHeroGameplayAbility::GetCurrentHeroAbilitySpecHandle() const
{
	return CurrentSpecHandle;
}

bool UActionHeroGameplayAbility::OwnsHeroMontage(const UAnimMontage* InMontage) const
{
	return InMontage != nullptr && ActiveHeroMontageAsset.Get() == InMontage;
}

void UActionHeroGameplayAbility::GetPredictedOwnedTagsToReleaseOnRelationshipCancel(
	FGameplayTagContainer& OutOwnedTags) const
{
	Super::GetPredictedOwnedTagsToReleaseOnRelationshipCancel(OutOwnedTags);
	OutOwnedTags.AppendTags(ActiveHeroCombatModifierGrantedTags);
}

bool UActionHeroGameplayAbility::ApplyHeroCombatModifierEffect(const FActionCombatModifierEffectSpec& EffectSpec)
{
	UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetActionAbilitySystemComponentFromActorInfo();
	if (!ActionAbilitySystemComponent)
	{
		// 没有 ASC 时，临时战斗修正无法通过 GAS 安全挂载。
		return false;
	}

	const FActiveGameplayEffectHandle EffectHandle =
		ActionAbilitySystemComponent->ApplyCombatModifierEffect(EffectSpec);
	if (!EffectHandle.IsValid())
	{
		// 效果施加失败时不记录句柄，避免后续清理阶段去移除一份根本没成功加上的效果。
		return false;
	}

	// 基类记录句柄，后续在 EndAbility 统一移除。
	// 这样子类只需要声明“要加什么效果”，不需要各自维护一套清理代码。
	// 这里记录的是“这次 Ability 生命周期里自己挂上去的效果”，
	// 不是按标签搜全局效果，因此清理范围始终可控。
	// 这也意味着子类如果想做“跨 Ability 持续保留”的效果，就不该走这个入口，
	// 否则会在当前 Ability 结束时被基类自动回收。
	ActiveHeroCombatModifierEffectHandles.Add(EffectHandle);
	ActiveHeroCombatModifierGrantedTags.AppendTags(EffectSpec.GrantedTags);
	return true;
}

void UActionHeroGameplayAbility::OnHeroMontageCompleted(FName MontageContext)
{
}

void UActionHeroGameplayAbility::OnHeroMontageBlendOut(FName MontageContext)
{
}

void UActionHeroGameplayAbility::OnHeroMontageInterrupted(FName MontageContext)
{
}

void UActionHeroGameplayAbility::OnHeroMontageCancelled(FName MontageContext)
{
}

bool UActionHeroGameplayAbility::ValidateHeroRuntimeObjects(
	FString& OutFailureReason,
	const bool bRequireController,
	const bool bRequireCombatComponent)
{
	// 这里专门做“英雄战斗 Ability 运行时依赖是否齐全”的共用检查，
	// 避免每个子类一开头都自己重复写角色/控制器/战斗组件判空。
	OutFailureReason.Reset();

	if (!GetHeroCharacterFromActorInfo())
	{
		// 这里统一返回文本失败原因，目的是让子类在日志里能直接打印人能读懂的缺失项，
		// 而不是每个 GA 都各自拼一套“缺了谁”的报错字符串。
		OutFailureReason = TEXT("hero character is invalid");
		return false;
	}

	if (bRequireController && !GetHeroControllerFromActorInfo())
	{
		// 某些 Ability 明确依赖控制器，例如需要读移动输入、移动状态或本地玩家控制语义。
		OutFailureReason = TEXT("hero controller is invalid");
		return false;
	}

	if (bRequireCombatComponent && !GetHeroCombatComponentFromActorInfo())
	{
		// 绝大多数战斗 GA 都依赖战斗组件做事件广播、窗口读写和输入恢复，因此这里允许调用方显式要求它必须存在。
		OutFailureReason = TEXT("hero combat component is invalid");
		return false;
	}

	// 这层只检查“对象是否存在”，不检查“当前是否允许施放”。
	// 激活资格、优先级、窗口和冷却仍然应该放在更上层的关系系统或具体 GA 中处理。
	return true;
}

void UActionHeroGameplayAbility::ClearHeroCombatModifierEffects()
{
	if (ActiveHeroCombatModifierEffectHandles.Num() == 0)
	{
		return;
	}

	if (UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetActionAbilitySystemComponentFromActorInfo())
	{
		// 基类只负责“移除这次 Ability 自己挂上的临时效果”，
		// 不会动其它系统或其它 Ability 挂上的持续效果，避免清理范围越界。
		for (const FActiveGameplayEffectHandle& EffectHandle : ActiveHeroCombatModifierEffectHandles)
		{
			if (EffectHandle.IsValid())
			{
				// 这里只精确移除这条 Ability 自己记录下来的句柄。
				// 这样不会误删同标签、但来源于其它 Ability 或其它系统的运行时效果。
				ActionAbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
			}
		}
	}

	// 无论 ASC 当前是否还存在，本地句柄数组都应清空。
	// 否则下一次激活同一实例时，会把上一轮生命周期的效果句柄误认为仍然归自己管理。
	ActiveHeroCombatModifierEffectHandles.Reset();
	ActiveHeroCombatModifierGrantedTags.Reset();
}

void UActionHeroGameplayAbility::HandleSharedMontageCompleted()
{
	// 共享任务完成后，把具体上下文继续转发给子类。
	// 基类不关心这是攻击、闪避还是切武蒙太奇，只负责把统一事件翻译成带上下文的回调。
	// 这种“基类做任务绑定，子类做业务分支”的结构，能明显减少各条 GA 链的重复样板代码。
	OnHeroMontageCompleted(ActiveMontageContext);
	ActiveHeroMontageAsset = nullptr;
}

void UActionHeroGameplayAbility::HandleSharedMontageBlendOut()
{
	// BlendOut 也走同一套上下文分发，避免不同 Ability 各自维护绑定表。
	OnHeroMontageBlendOut(ActiveMontageContext);
	ActiveHeroMontageAsset = nullptr;
}

void UActionHeroGameplayAbility::HandleSharedMontageInterrupted()
{
	// Interrupted 与 Cancelled 分开发，是为了保留子类区分“被外部打断”和“主动取消”的机会。
	OnHeroMontageInterrupted(ActiveMontageContext);
	ActiveHeroMontageAsset = nullptr;
}

void UActionHeroGameplayAbility::HandleSharedMontageCancelled()
{
	// 这里同样继续转发上下文，保证子类即使共用同一套回调入口也能知道是哪段蒙太奇被取消。
	OnHeroMontageCancelled(ActiveMontageContext);
	ActiveHeroMontageAsset = nullptr;
}
