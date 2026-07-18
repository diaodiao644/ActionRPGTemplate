// 文件说明：实现闪避 Gameplay Ability 的运行时逻辑。

#include "AbilitySystem/Abilities/Hero/HeroGA_Dodge.h"

#include "ActionGameplayTags.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "GameBase/ActionPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroGA_Dodge, Log, All);

UHeroGA_Dodge::UHeroGA_Dodge()
	: Super()
{
	ActivationPolicy = EActionAbilityActivationPolicy::OnInput;

	// 在原生构造函数里固定声明闪避 AbilityTag，避免蓝图侧遗漏配置。
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_Dodge);
	ActivationOwnedTags.AddTag(ActionGameplayTags::State_Ability_Dodge_Active);
	CombatReactAbilityRule.bAllowActivationDuringRecoveryCancelWindow = true;

	// 闪避期间的临时战斗状态统一收口到这份分支级修正效果里。
	// 这样“闪避进行中”这件事既能被受击过滤、状态查询和后续联动统一识别，
	// 又不需要在 GA 内部手写一套额外的 loose tag 开关。
	DodgeCombatModifierEffect.Duration = 10.f;
	DodgeCombatModifierEffect.StatusEffectTag = ActionGameplayTags::StatusEffect_Combat_Dodge;
	DodgeCombatModifierEffect.GrantedTags.AddTag(ActionGameplayTags::State_Combat_Dodge_Active);
}

bool UHeroGA_Dodge::ValidateRelationshipActivationPreconditions(FString& OutFailureReason)
{
	// 关系预检阶段只补充“这条闪避主链现在有没有资格被放行”的判断。
	// 这里不真正写闪避运行态，避免预检失败时留下半套战斗或移动副作用。
	if (!ValidateHeroRuntimeObjects(OutFailureReason, true, true))
	{
		return false;
	}

	UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo();
	if (!HeroDefenseComponent)
	{
		OutFailureReason = TEXT("hero defense component is invalid");
		return false;
	}

	if (!HeroDefenseComponent->CanEnterRelationshipActivationForNonAttackInput(
		ActionGameplayTags::InputTag_GameplayAbility_Dodge,
		&OutFailureReason))
	{
		// 闪避当前只在关系裁决前保留共享硬门禁。
		// 是否能抢入活跃主动 GA，统一交回 ASC 关系矩阵和 interrupt-window 裁决。
		// 也就是说，这里拦下的是“宿主当前绝对不能起闪避”，不是“Attack 没给白名单所以闪避不许进 ASC”。
		return false;
	}

	// 闪避资格的资源校验当前保持极简：
	// 这里只确认“当前武器已经给出了可播放的闪避蒙太奇”，
	// 而不在这里提前判断完美闪避是否可触发，因为那属于闪避开始后的窗口期结果。
	if (!HeroDefenseComponent->GetDodgeAnimMontage())
	{
		// 闪避当前不再区分复杂的资源前置，只要求“当前武器确实能给出一段可播的闪避蒙太奇”。
		// 如果连这份最基础的表现资源都没有，就不该继续把闪避链推到运行时。
		OutFailureReason = TEXT("dodge montage is missing");
		return false;
	}

	return true;
}

void UHeroGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// 这批字段都只服务“本次闪避实例”。
	// 正式闪避运行态在组件侧；GA 本地只缓存短生命周期时序，方便把各种结束路径统一收口。
	// 每次新开一条闪避链时，先把这次闪避自己的运行时标记清零。
	// 这些标记都只属于“本次闪避实例”，绝不能沿用上一段闪避留下来的结果。
	bDodgeAbilityFinished = false;
	bRequestedFastRunFromDodge = false;

	AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo();
	AActionPlayerController* HeroController = GetHeroControllerFromActorInfo();
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCharacter || !HeroController || !HeroCombatComponent)
	{
		UE_LOG(LogHeroGA_Dodge, Warning, TEXT("闪避能力激活失败：角色、控制器或战斗组件为空。"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo();
	UAnimMontage* DodgeMontage = HeroDefenseComponent ? HeroDefenseComponent->GetDodgeAnimMontage() : nullptr;
	if (!DodgeMontage)
	{
		const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = HeroCombatComponent->GetCurrentWeaponDefinition();
		UE_LOG(
			LogHeroGA_Dodge,
			Warning,
			TEXT("闪避动画为空：请检查当前武器的 StandingDodgeMontage / MovingDodgeMontage 是否已配置并成功加载。WeaponTag=%s HasMoveInput=%d"),
			CurrentWeaponDefinition ? *CurrentWeaponDefinition->WeaponTag.ToString() : TEXT("None"),
			HeroCharacter->HasMoveInput());
		K2_EndAbility();
		return;
	}

	HeroCombatComponent->UpdateRunningAnimMontage(DodgeMontage);
	if (HeroDefenseComponent)
	{
		// 闪避链公共运行态现在统一交给防御组件写入。
		HeroDefenseComponent->ApplyDodgeAbilityStarted(DodgeMontage);
	}
	// 闪避的临时状态效果统一在这里附加。
	// 这样无敌、受击过滤、后续窗口标签等“闪避期间成立”的语义都能跟 Ability 生命周期自动对齐。
	// 先写闪避运行态、再施加临时修正效果，可以保证后续状态查询看到的是一套已经正式起手的闪避语义。
	ApplyHeroCombatModifierEffect(DodgeCombatModifierEffect);

	// 闪避位移期间临时切到 FastRun，沿用现有移动参数驱动闪避过程。
	// 只有“移动中的闪避”才会申请 FastRun，原地闪避不会强行改写移动状态。
	bRequestedFastRunFromDodge = HeroController->HasMoveInput();
	if (bRequestedFastRunFromDodge)
	{
		// 这里记录的是“本次闪避起手时是否确实需要移动闪避”。
		// 后面收尾阶段会结合这个标记与结束时是否仍有移动输入，判断要不要保留 FastRun。
		HeroController->SetMoveState(EMoveState::FastRun);
		HeroController->UpdateMovementData();
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][Dodge] Begin MoveInput=%d KeepFastRun=%d MoveState=%d"),
			HeroController->HasMoveInput() ? 1 : 0,
			bRequestedFastRunFromDodge ? 1 : 0,
			static_cast<int32>(HeroController->GetMoveState())),
		FColor::Cyan,
		2.0f);

	if (!PlayHeroMontage(DodgeMontage, TEXT("PlayDodgeMontageAndWait"), TEXT("Dodge")))
	{
		UE_LOG(LogHeroGA_Dodge, Warning, TEXT("闪避蒙太奇播放失败。Montage=%s"), *GetNameSafe(DodgeMontage));
		// 闪避状态已经部分写入运行时后，如果蒙太奇播放失败，不能直接 return，
		// 必须立即走统一收尾，把已写入的战斗态和移动态完整回收。
		FinishDodgeAbility();
	}
}

void UHeroGA_Dodge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 闪避可能被外部系统、蓝图或更高优先级链条直接结束。
	// 因此这里要做兜底收尾，不能假设蒙太奇一定会按预期把所有结束回调都走完。
	// 闪避 GA 可能被外部系统直接结束，收尾不能只依赖蒙太奇回调。
	if (!bDodgeAbilityFinished)
	{
		FinalizeDodgeAbility();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHeroGA_Dodge::FinishDodgeAbility()
{
	// 蒙太奇可能同时触发 Completed / BlendOut / Cancelled 等多个回调，这里统一做一次防重入保护。
	if (bDodgeAbilityFinished)
	{
		return;
	}

	// 闪避所有“准备结束”的路径都先经过这里，再由这里决定正式收尾并结束 Ability。
	// 先正式收尾，再 K2_EndAbility，可以避免蓝图结束顺序把局部状态留成半套。
	FinalizeDodgeAbility();
	K2_EndAbility();
}

void UHeroGA_Dodge::FinalizeDodgeAbility()
{
	if (bDodgeAbilityFinished)
	{
		return;
	}

	bDodgeAbilityFinished = true;
	// 这里是闪避所有结束路径最终汇合到的正式收口点。
	// 无论是正常播完、取消、中断还是外部强制结束，都必须在这里把组件状态和移动状态统一回收。
	bool bCombatReactResetInProgress = false;

	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		bCombatReactResetInProgress = HeroCombatComponent->IsCombatReactStateResetInProgress();
		if (UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo())
		{
			// 闪避链公共运行态收尾交回防御组件统一处理。
			// 这一步负责收掉闪避成立期间的正式战斗语义，本身不等于“输入现在立刻重新开放”。
			HeroDefenseComponent->FinalizeDodgeAbilityRuntime(bCombatReactResetInProgress);
		}
	}

	if (!bCombatReactResetInProgress)
	{
		if (AActionPlayerController* HeroController = GetHeroControllerFromActorInfo())
		{
			// 闪避期间会临时把移动状态切到 FastRun。
			// 闪避结束时必须把这个临时状态收口掉，避免角色在仍有移动输入时错误残留在冲刺态。
			const bool bShouldKeepFastRun = ShouldKeepFastRunAfterDodge();
			if (bShouldKeepFastRun)
			{
				// 只有“闪避开始时就带移动输入，且结束时仍在持续移动”时，
				// 才把这次闪避自然衔接回 FastRun，形成移动闪避后的顺滑续跑。
				HeroController->SetMoveState(EMoveState::FastRun);
			}
			else if (HeroController->GetMoveState() == EMoveState::FastRun)
			{
				// 其它情况一律回收到普通 Run，防止把闪避用过的高速状态错误留在角色身上。
				HeroController->SetMoveState(EMoveState::Run);
			}

			HeroController->UpdateMovementData();

			Debug::Print(
				FString::Printf(
					TEXT("[GA][Dodge] End MoveInput=%d KeepFastRun=%d MoveState=%d"),
					HeroController->HasMoveInput() ? 1 : 0,
					bShouldKeepFastRun ? 1 : 0,
					static_cast<int32>(HeroController->GetMoveState())),
				FColor::Green,
				2.0f);
		}
	}

	if (!bCombatReactResetInProgress)
	{
		if (UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo())
		{
			// 闪避结束后延后一帧恢复战斗输入，确保闪避窗口收口与后续输入衔接顺序稳定。
			// 这一步和上面的“立即恢复攻击开关”不是一回事：
			// 前者是收掉闪避运行态，后者是安排输入系统在正确时序下重新接管。
			// 因此 FinalizeDodgeAbilityRuntime(...) 与 RequestRecoverCombatInputAfterDodge() 分别负责“状态收口”和“输入恢复时序”。
			HeroDefenseComponent->RequestRecoverCombatInputAfterDodge();
		}
	}
	else
	{
		// 当受击系统正在执行更高优先级的整链重置时，
		// 闪避 GA 在这里主动让出最终状态控制权，避免闪避收尾和受击收尾互相覆盖。
		// 这类场景下最终状态以受击接管链为准，闪避不再争抢最后一拍的移动或输入恢复。
	}
}

bool UHeroGA_Dodge::ShouldKeepFastRunAfterDodge()
{
	if (const AActionPlayerController* HeroController = GetHeroControllerFromActorInfo())
	{
		// 必须同时满足“闪避开始时带有移动输入”和“闪避结束时仍然在移动”，
		// 才说明这次闪避应该自然衔接回移动中的 FastRun。
		// 这不是新的移动状态源，只是闪避收尾阶段的一次性恢复判定。
		return bRequestedFastRunFromDodge && HeroController->HasMoveInput();
	}

	// 拿不到控制器时，默认按“不保留 FastRun”处理，避免保留一份无法正确验证的高速移动状态。
	return false;
}

void UHeroGA_Dodge::OnHeroMontageCompleted(FName MontageContext)
{
	(void)MontageContext;
	// 闪避没有 Attack 那种“姿态过渡”和“正式攻击”双层蒙太奇语义。
	// 因此四类结束回调都只承担一件事：把不同动画结束路径收束到同一个收尾入口。
	// 闪避没有“播完”和“混出”上的业务差异，所以统一收口。
	FinishDodgeAbility();
}

void UHeroGA_Dodge::OnHeroMontageBlendOut(FName MontageContext)
{
	(void)MontageContext;
	// 闪避没有依赖 BlendOut 与 Completed 的行为差异，
	// 因此只要进入任意结束回调，就统一收口到同一条结束链。
	FinishDodgeAbility();
}

void UHeroGA_Dodge::OnHeroMontageInterrupted(FName MontageContext)
{
	(void)MontageContext;
	FinishDodgeAbility();
}

void UHeroGA_Dodge::OnHeroMontageCancelled(FName MontageContext)
{
	(void)MontageContext;
	FinishDodgeAbility();
}
