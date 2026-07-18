#include "Components/Combat/HeroDefenseComponent.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Engine/World.h"
#include "GameBase/ActionPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

UHeroDefenseComponent::UHeroDefenseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	ParryWindowCombatModifierEffect.StatusEffectTag = ActionGameplayTags::StatusEffect_Combat_ParryWindow;
	ParryWindowCombatModifierEffect.GrantedTags.AddTag(ActionGameplayTags::State_Combat_ParryWindow_Active);

	ParrySuccessCombatModifierEffect.Duration = 0.4f;
	ParrySuccessCombatModifierEffect.StatusEffectTag = ActionGameplayTags::StatusEffect_Combat_ParrySuccess;
	ParrySuccessCombatModifierEffect.GrantedTags.AddTag(ActionGameplayTags::State_Combat_Parry_Success);

	PerfectDodgeSuccessCombatModifierEffect.Duration = 0.4f;
	PerfectDodgeSuccessCombatModifierEffect.StatusEffectTag = ActionGameplayTags::StatusEffect_Combat_PerfectDodgeSuccess;
	PerfectDodgeSuccessCombatModifierEffect.GrantedTags.AddTag(ActionGameplayTags::State_Combat_PerfectDodge_Success);

	DodgeCounterReadyCombatModifierEffect.StatusEffectTag = ActionGameplayTags::StatusEffect_Combat_DodgeCounterReady;
	DodgeCounterReadyCombatModifierEffect.GrantedTags.AddTag(ActionGameplayTags::State_Combat_DodgeCounter_Ready);
}

void UHeroDefenseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(PerfectDodgeWindowTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(DodgeCounterWindowTimerHandle);
	}

	if (UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		CombatComponent->RemoveComponentCombatModifierEffect(ParryWindowEffectHandle);
		CombatComponent->RemoveComponentCombatModifierEffect(ParrySuccessEffectHandle);
		CombatComponent->RemoveComponentCombatModifierEffect(PerfectDodgeSuccessEffectHandle);
		CombatComponent->RemoveComponentCombatModifierEffect(DodgeCounterReadyEffectHandle);
	}

	Super::EndPlay(EndPlayReason);
}

bool UHeroDefenseComponent::CanActivateNonAttackInputNow(const FGameplayTag& InputTag) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = CombatComponent
		? CombatComponent->GetOwningHeroWeaponSwitchComponent()
		: nullptr;
	if (!CombatComponent)
	{
		return false;
	}

	if (CombatComponent->IsNonAttackInputBlockedByCombatReact(InputTag))
	{
		// 受击主段和未放行的恢复阶段里，防御 / 闪避 / 处决这类非攻击输入都不能直接抢回控制权。
		// 只有恢复尾段取消窗口明确对白名单放行后，才允许它们重新接管。
		return false;
	}

	if (CombatComponent->IsNonAttackInputBlockedByAirborneState())
	{
		// 普通空中状态下，当前框架统一禁止这类地面非攻击输入立即激活。
		return false;
	}

	if (!InputTag.IsValid()
		|| (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchTransactionInProgress()))
	{
		// 切武事务态里，不允许防御 / 闪避 / 处决直接抢进来。
		// 否则当前武器上下文、表现期和输入恢复顺序都会被打乱。
		return false;
	}

	if (WeaponSwitchComponent
		&& WeaponSwitchComponent->IsSpecialWeaponSwitchPresentationActive()
		&& !CombatComponent->IsSpecialWeaponSwitchPresentationInterruptInputAllowed(InputTag))
	{
		// 切武表现期默认阻断非攻击主动输入，只有现有主动 GA 抢断窗明确放行时才允许抢入。
		return false;
	}

	if (CombatComponent->IsInputOverrideContextActive())
	{
		// 如果当前动作已经进入输入改写上下文，则是否允许防御或闪避，
		// 要由当前抢断窗或恢复窗白名单来决定，而不是无条件放行。
		return CombatComponent->IsInputAllowedByCurrentOverrideContext(InputTag);
	}

	// 普通情况下，非攻击输入是否允许激活，统一跟随战斗总控当前是否开放主动输入。
	return CombatComponent->IsAttackEnabled();
}

bool UHeroDefenseComponent::CanEnterRelationshipActivationForNonAttackInput(
	const FGameplayTag& InputTag,
	FString* OutFailureReason) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("hero combat component is invalid");
		}
		return false;
	}

	return CombatComponent->PassesSharedNonAttackAbilityHardGate(InputTag, OutFailureReason);
}

UAnimMontage* UHeroDefenseComponent::GetDodgeAnimMontage() const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const AActionHeroCharacter* OwnerCharacter = CombatComponent
		? CombatComponent->GetOwningHeroCharacter()
		: nullptr;
	if (!CombatComponent || !OwnerCharacter)
	{
		return nullptr;
	}

	if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition())
	{
		return CurrentWeaponDefinition->GetDodgeAnimMontage(OwnerCharacter->HasMoveInput());
	}

	return nullptr;
}

UAnimMontage* UHeroDefenseComponent::GetDefenseAnimMontage() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition())
		{
			return CurrentWeaponDefinition->GetDefenseAnimMontage();
		}
	}

	return nullptr;
}

UAnimMontage* UHeroDefenseComponent::GetBlockedHitAnimMontage() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition())
		{
			return CurrentWeaponDefinition->GetBlockedHitAnimMontage();
		}
	}

	return nullptr;
}

int32 UHeroDefenseComponent::GetDodgeReactGuardThreshold() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition())
		{
			const AActionHeroCharacter* OwnerCharacter = CombatComponent->GetOwningHeroCharacter();
			return CurrentWeaponDefinition->GetDodgeReactGuardThreshold(
				OwnerCharacter && OwnerCharacter->HasMoveInput());
		}
	}

	return 0;
}

int32 UHeroDefenseComponent::GetDefenseReactGuardThreshold() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition())
		{
			return CurrentWeaponDefinition->GetDefenseReactGuardThreshold();
		}
	}

	return 0;
}

void UHeroDefenseComponent::ApplyDefenseAbilityStarted(UAnimMontage* DefenseMontage)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !DefenseMontage)
	{
		return;
	}

	// 防御起手时先统一收掉攻击链残留，再把战斗态切入防御。
	// 这样防御 GA 不需要自己逐项改公共运行态。
	CombatComponent->SetAttackEnabled(false);
	CombatComponent->ResetComboIndex();
	CombatComponent->SetCombatMode(EHeroCombatMode::Defense);
	CombatComponent->UpdateRunningAnimMontage(DefenseMontage);
	CombatComponent->SetRunningAnimationReactGuardContext(
		DefenseMontage,
		EActionRunningAnimationSemantic::NonReact,
		GetDefenseReactGuardThreshold());
	CombatComponent->ClearAbilityWindowsForAuthoritativeTakeover();
	ActiveDefenseMontage = DefenseMontage;
	ClearDefenseReleaseRequirement();
	BeginDefenseState();

	if (AActionHeroCharacter* OwnerHeroCharacter = CombatComponent->GetOwningHeroCharacter())
	{
		// 当前防御链仍然采用硬锁移动模式。
		if (UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter->GetCharacterMovement())
		{
			MovementComponent->SetMovementMode(EMovementMode::MOVE_None);
		}
	}
}

void UHeroDefenseComponent::FinalizeDefenseAbilityRuntime(const bool bCombatReactResetInProgress)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || bCombatReactResetInProgress)
	{
		return;
	}

	// 防御结束后恢复公共战斗运行态，并回到普通战斗姿态。
	CombatComponent->SetAttackEnabled(true);
	CombatComponent->SetCombatMode(EHeroCombatMode::Combo);
	CombatComponent->ClearRunningAnimationReactGuardContextIfMatches(
		ActiveDefenseMontage,
		EActionRunningAnimationSemantic::NonReact);
	CombatComponent->ClearRunningAnimMontageReferenceIfMatches(ActiveDefenseMontage);
	ActiveDefenseMontage = nullptr;
	EndDefenseState();
	RestoreWalkingMovementIfLocked();
}

void UHeroDefenseComponent::ApplyDodgeAbilityStarted(UAnimMontage* DodgeMontage)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !DodgeMontage)
	{
		return;
	}

	// 闪避作为高优先级动作接管时，先统一关闭攻击链残留窗口并禁止攻击输入。
	CombatComponent->ResetComboIndex();
	CombatComponent->SetAttackEnabled(false);
	CombatComponent->SetRunningAnimationReactGuardContext(
		DodgeMontage,
		EActionRunningAnimationSemantic::NonReact,
		GetDodgeReactGuardThreshold());
	CombatComponent->ClearAbilityWindowsForAuthoritativeTakeover();
	ActiveDodgeMontage = DodgeMontage;
	BeginDodgeState();
}

void UHeroDefenseComponent::FinalizeDodgeAbilityRuntime(const bool bCombatReactResetInProgress)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || bCombatReactResetInProgress)
	{
		return;
	}

	// 闪避结束时只负责收回公共战斗态，移动状态仍由闪避 GA 根据局部输入语义决定。
	CombatComponent->SetAttackEnabled(true);
	CombatComponent->ClearRunningAnimationReactGuardContextIfMatches(
		ActiveDodgeMontage,
		EActionRunningAnimationSemantic::NonReact);
	CombatComponent->ClearRunningAnimMontageReferenceIfMatches(ActiveDodgeMontage);
	ActiveDodgeMontage = nullptr;
	EndDodgeState();
}

void UHeroDefenseComponent::RequireDefenseReleaseBeforeReactivation()
{
	bRequireDefenseReleaseBeforeReactivation = true;
}

void UHeroDefenseComponent::ClearDefenseReleaseRequirement()
{
	bRequireDefenseReleaseBeforeReactivation = false;
}

void UHeroDefenseComponent::BeginDefenseState()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	ClearDodgeCounterAvailability();
	CombatComponent->SetDefenseStateActive(true);
	CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_Defense_Begin);
	OpenParryWindow();
}

void UHeroDefenseComponent::EndDefenseState()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	CombatComponent->BroadcastCombatEventIf(
		CombatComponent->IsDefenseActive(),
		ActionGameplayTags::Player_Event_Defense_End);
	CloseParryWindow();
	CombatComponent->SetDefenseStateActive(false);
}

void UHeroDefenseComponent::BeginDodgeState()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	ClearDodgeCounterAvailability();
	CombatComponent->SetDodgeStateActive(true);
	CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_Dodge_Begin);
	OpenPerfectDodgeWindow();
}

void UHeroDefenseComponent::EndDodgeState()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	CombatComponent->BroadcastCombatEventIf(
		CombatComponent->IsDodgeActive(),
		ActionGameplayTags::Player_Event_Dodge_End);
	ClosePerfectDodgeWindow();
	CombatComponent->SetDodgeStateActive(false);
}

void UHeroDefenseComponent::ClearDodgeCounterAvailability()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	if (CombatComponent->IsDodgeCounterAvailable())
	{
		DisableDodgeCounterWindow();
	}
}

bool UHeroDefenseComponent::ConsumeDodgeCounterAvailability()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !CombatComponent->IsDodgeCounterAvailable())
	{
		return false;
	}

	DisableDodgeCounterWindow();
	CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_DodgeCounter_Consumed);
	return true;
}

void UHeroDefenseComponent::RequestRecoverCombatInputAfterDodge()
{
	RequestDeferredCombatInputRecovery(
		bDeferredCombatInputRecoveryAfterDodgeRequested,
		&UHeroDefenseComponent::HandleDeferredCombatInputRecoveryAfterDodge);
}

void UHeroDefenseComponent::RequestRecoverCombatInputAfterDefense()
{
	RequestDeferredCombatInputRecovery(
		bDeferredCombatInputRecoveryAfterDefenseRequested,
		&UHeroDefenseComponent::HandleDeferredCombatInputRecoveryAfterDefense);
}

void UHeroDefenseComponent::HandleCombatReactStateReset()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	const bool bWasDefenseStateActive = CombatComponent->HasAnyDefenseStateActive();
	const bool bWasDodgeStateActive = CombatComponent->HasAnyDodgeStateActive();

	if (UActionAbilitySystemComponent* OwnerASC = CombatComponent->GetOwningActionAbilitySystemComponent())
	{
		if (bWasDefenseStateActive)
		{
			OwnerASC->CancelAbilityByAbilityTag(ActionGameplayTags::Player_Ability_CombatModeOrDefense);
		}

		if (bWasDodgeStateActive)
		{
			OwnerASC->CancelAbilityByAbilityTag(ActionGameplayTags::Player_Ability_Dodge);
		}
	}

	if (bWasDefenseStateActive)
	{
		// 受击重置会直接中断防御 GA，因此这里要主动把公共战斗态拉回防御前的基线。
		// 否则后续受击恢复结束后，角色可能仍残留在 Defense 模式或攻击被关闭的坏状态里。
		CombatComponent->SetAttackEnabled(true);
		CombatComponent->SetCombatMode(EHeroCombatMode::Combo);
		CombatComponent->ClearAbilityInterruptWindowForCombatReactHardReset();
		CombatComponent->ClearRunningAnimationReactGuardContextIfMatches(
			ActiveDefenseMontage,
			EActionRunningAnimationSemantic::NonReact);
		CombatComponent->ClearRunningAnimMontageReferenceIfMatches(ActiveDefenseMontage);
		ActiveDefenseMontage = nullptr;
		EndDefenseState();
		RestoreWalkingMovementIfLocked();
	}

	if (bWasDodgeStateActive)
	{
		// 闪避被受击重置打断时，同样要主动恢复攻击开关与主动 GA 抢断窗口。
		// 闪避本身不改 CombatMode，因此这里只回收它接管过的公共输入状态。
		CombatComponent->SetAttackEnabled(true);
		CombatComponent->ClearAbilityInterruptWindowForCombatReactHardReset();
		CombatComponent->ClearRunningAnimationReactGuardContextIfMatches(
			ActiveDodgeMontage,
			EActionRunningAnimationSemantic::NonReact);
		CombatComponent->ClearRunningAnimMontageReferenceIfMatches(ActiveDodgeMontage);
		ActiveDodgeMontage = nullptr;
		EndDodgeState();
		RestoreRunMovementIfFastRun();
	}

	ClearDodgeCounterAvailability();
}

bool UHeroDefenseComponent::TryHandleIncomingDamageDefenseReaction(
	const FActionDamagePayload& InDamagePayload,
	FActionHitResolveResult& OutResult)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		OutResult.ResultType = EActionHitResultType::None;
		return false;
	}

	// 这里的判定顺序是明确固定的：
	// 1. 先看精准防御；
	// 2. 再看普通格挡；
	// 3. 最后看完美闪避。
	// 这样可以保证“防御中的同一击”不会既被当成格挡又被当成完闪。
	if (TryHandleDefenseWindowReaction(
		InDamagePayload.bCanBeParried && CombatComponent->IsParryWindowActive(),
		&UHeroDefenseComponent::HandleSuccessfulParry,
		EActionHitResultType::Parried,
		&OutResult))
	{
		return true;
	}

	if (TryHandleBlockedDefenseReaction(
		InDamagePayload.bCanBeBlocked && CombatComponent->IsDefenseActive(),
		EActionHitResultType::Blocked,
		&OutResult))
	{
		return true;
	}

	if (TryHandleDefenseWindowReaction(
		InDamagePayload.bCanBePerfectDodged && CombatComponent->IsPerfectDodgeWindowActive(),
		&UHeroDefenseComponent::HandleSuccessfulPerfectDodge,
		EActionHitResultType::PerfectDodged,
		&OutResult))
	{
		return true;
	}

	OutResult.ResultType = EActionHitResultType::None;
	return false;
}

void UHeroDefenseComponent::HandleSuccessfulParry()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	CloseParryWindow();
	CombatComponent->AddSpecialWeaponSwitchEnergy(ParrySpecialWeaponSwitchEnergyReward);
	CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_Parry_Success);
	CombatComponent->ApplyComponentCombatModifierEffect(
		ParrySuccessEffectHandle,
		ParrySuccessCombatModifierEffect);
}

void UHeroDefenseComponent::HandleSuccessfulPerfectDodge()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	ClosePerfectDodgeWindow();
	CombatComponent->AddSpecialWeaponSwitchEnergy(PerfectDodgeSpecialWeaponSwitchEnergyReward);
	CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_PerfectDodge_Success);
	CombatComponent->ApplyComponentCombatModifierEffect(
		PerfectDodgeSuccessEffectHandle,
		PerfectDodgeSuccessCombatModifierEffect);
	EnableDodgeCounterWindow();
}

void UHeroDefenseComponent::ResetRuntimeStateForHeroStartup()
{
	bDeferredCombatInputRecoveryAfterDodgeRequested = false;
	bDeferredCombatInputRecoveryAfterDefenseRequested = false;
	bRequireDefenseReleaseBeforeReactivation = false;
}

bool UHeroDefenseComponent::TryHandleDefenseInputByEvent(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag& InputTag,
	EActionInputEvent InputEvent)
{
	if (!IsDefenseInputTag(InputTag))
	{
		return false;
	}

	// 防御组件只负责两类非攻击输入：
	// 1. Dodge
	// 2. CombatModeOrDefense
	// 再按 Pressed / Held / Released 三个阶段分发到各自子处理函数。
	switch (InputEvent)
	{
	case EActionInputEvent::Pressed:
		if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_Dodge)
		{
			return HandleDodgeInput(InActionASC, InputTag, InputEvent);
		}

		if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense)
		{
			return HandleCombatModeOrDefensePressed(InActionASC, InputTag);
		}
		break;

	case EActionInputEvent::Held:
		if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_Dodge)
		{
			return HandleDodgeInput(InActionASC, InputTag, InputEvent);
		}

		if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense)
		{
			return HandleCombatModeOrDefenseHeld(InActionASC, InputTag);
		}
		break;

	case EActionInputEvent::Released:
		return TryForwardReleasedAbilityInput(InActionASC, InputTag);

	default:
		break;
	}

	return false;
}

UHeroCombatComponent* UHeroDefenseComponent::GetOwningHeroCombatComponent() const
{
	if (const AActionHeroCharacter* OwnerCharacter = Cast<AActionHeroCharacter>(GetOwner()))
	{
		return OwnerCharacter->GetHeroCombatComponent();
	}

	return nullptr;
}

UHeroCombatInputComponent* UHeroDefenseComponent::GetOwningHeroCombatInputComponent() const
{
	if (const AActionHeroCharacter* OwnerCharacter = Cast<AActionHeroCharacter>(GetOwner()))
	{
		return OwnerCharacter->GetHeroCombatInputComponent();
	}

	return nullptr;
}

void UHeroDefenseComponent::RequestDeferredCombatInputRecovery(
	bool& bRequestFlag,
	void (UHeroDefenseComponent::*Handler)())
{
	if (bRequestFlag || !GetWorld())
	{
		return;
	}

	// 防御链和闪避链的输入恢复都统一延后一帧触发。
	// 这样可以等 GA 退场、窗口关闭和公共战斗态回收都稳定后，再回接缓冲输入或 Held 输入。
	// 不区分防御恢复还是闪避恢复，登记动作统一只做一件事：
	// 标记“下一帧需要走一次公共恢复入口”。
	bRequestFlag = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, Handler);
}

void UHeroDefenseComponent::HandleDeferredCombatInputRecovery(bool& bRequestFlag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || !bRequestFlag)
	{
		return;
	}

	// 先清恢复标记，再进入总控恢复入口，
	// 避免恢复过程中又触发这里时出现“标记没清导致后续恢复失效”的问题。
	bRequestFlag = false;

	// 防御 / 闪避结束后的恢复当前统一复用受击恢复链的总入口：
	// 先尝试消费缓冲输入，未命中再回放仍有效的 Held 输入。
	CombatInputComponent->RecoverCombatInputAfterCombatReact();
}

bool UHeroDefenseComponent::TryHandleDefenseWindowReaction(
	const bool bShouldTrigger,
	void (UHeroDefenseComponent::*SuccessHandler)(),
	const EActionHitResultType SuccessResultType,
	FActionHitResolveResult* OutResult)
{
	if (!bShouldTrigger)
	{
		return false;
	}

	// 命中窗口后，先让对应成功处理函数负责奖励、状态变化和事件广播，
	// 再把这次命中结果回写给解析输出。
	(this->*SuccessHandler)();
	if (OutResult)
	{
		OutResult->ResultType = SuccessResultType;
	}

	return true;
}

bool UHeroDefenseComponent::TryHandleBlockedDefenseReaction(
	const bool bShouldBlock,
	const EActionHitResultType BlockResultType,
	FActionHitResolveResult* OutResult)
{
	if (!bShouldBlock)
	{
		return false;
	}

	if (UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (CombatComponent->IsParryWindowActive())
		{
			// 这次接触既然已经实际落成普通格挡，就说明本轮起手弹反尝试已经结束。
			// 这里主动关闭弹反窗口，避免同一次按住防御里出现“先挡住一击，再迟到弹反下一击”的窗口残留。
			CloseParryWindow();
		}

		CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_Defense_Blocked);
	}

	PlayBlockedHitReaction();

	if (OutResult)
	{
		OutResult->ResultType = BlockResultType;
	}

	return true;
}

void UHeroDefenseComponent::PlayBlockedHitReaction() const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	AActionHeroCharacter* OwnerHeroCharacter = CombatComponent
		? CombatComponent->GetOwningHeroCharacter()
		: nullptr;
	UAnimMontage* BlockedHitMontage = GetBlockedHitAnimMontage();
	if (!OwnerHeroCharacter || !BlockedHitMontage)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("普通格挡命中成功，但未配置格挡受击蒙太奇。Owner=%s WeaponTag=%s Montage=%s"),
			*GetNameSafe(GetOwner()),
			CombatComponent ? *CombatComponent->GetCurrentEquippedWeaponTag().ToString() : TEXT("None"),
			*GetNameSafe(BlockedHitMontage));
		return;
	}

	if (OwnerHeroCharacter->PlayAnimMontage(BlockedHitMontage) <= 0.f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("普通格挡受击蒙太奇播放失败。Owner=%s WeaponTag=%s Montage=%s"),
			*GetNameSafe(OwnerHeroCharacter),
			CombatComponent ? *CombatComponent->GetCurrentEquippedWeaponTag().ToString() : TEXT("None"),
			*GetNameSafe(BlockedHitMontage));
	}
}

void UHeroDefenseComponent::RestoreWalkingMovementIfLocked() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (AActionHeroCharacter* OwnerHeroCharacter = CombatComponent->GetOwningHeroCharacter())
		{
			if (UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter->GetCharacterMovement())
			{
				if (MovementComponent->MovementMode == EMovementMode::MOVE_None)
				{
					MovementComponent->SetMovementMode(EMovementMode::MOVE_Walking);
				}
			}
		}
	}
}

void UHeroDefenseComponent::RestoreRunMovementIfFastRun() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (AActionPlayerController* HeroController = CombatComponent->GetOwningHeroController())
		{
			if (HeroController->GetMoveState() == EMoveState::FastRun)
			{
				HeroController->SetMoveState(EMoveState::Run);
				HeroController->UpdateMovementData();
			}
		}
	}
}

void UHeroDefenseComponent::OpenTimedDefenseWindow(
	FTimerHandle& TimerHandle,
	void (UHeroDefenseComponent::*ExpireCallback)(),
	const float Duration,
	const FGameplayTag& OpenEventTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !GetWorld())
	{
		return;
	}

	// 所有“限时窗口”的公共开启动作都收口在这里：
	// 广播开窗事件、重置旧计时器、登记新的超时回调。
	CombatComponent->BroadcastCombatEvent(OpenEventTag);
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, ExpireCallback, Duration, false);
}

void UHeroDefenseComponent::CloseTimedDefenseWindow(
	FTimerHandle& TimerHandle,
	const bool bWasActive,
	const FGameplayTag& CloseEventTag)
{
	if (UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		// 只在窗口之前确实处于打开状态时，才广播对应的关闭事件。
		CombatComponent->BroadcastCombatEventIf(bWasActive, CloseEventTag);
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	}
}

void UHeroDefenseComponent::OpenParryWindow()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !GetWorld())
	{
		return;
	}

	// 精准防御窗口除了打开 bool 状态外，还会附带一层临时战斗修正效果，
	// 供后续标签查询、表现层或状态判断统一复用。
	CombatComponent->SetParryWindowStateActive(true);
	OpenTimedDefenseWindow(
		ParryWindowTimerHandle,
		&UHeroDefenseComponent::CloseParryWindow,
		ParryWindowDuration,
		ActionGameplayTags::Player_Event_ParryWindow_Open);
	CombatComponent->ApplyComponentCombatModifierEffect(
		ParryWindowEffectHandle,
		ParryWindowCombatModifierEffect,
		ParryWindowDuration);
}

void UHeroDefenseComponent::CloseParryWindow()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	CloseTimedDefenseWindow(
		ParryWindowTimerHandle,
		CombatComponent->IsParryWindowActive(),
		ActionGameplayTags::Player_Event_ParryWindow_Close);
	CombatComponent->SetParryWindowStateActive(false);
	CombatComponent->RemoveComponentCombatModifierEffect(ParryWindowEffectHandle);
}

void UHeroDefenseComponent::OpenPerfectDodgeWindow()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !GetWorld())
	{
		return;
	}

	// 完美闪避窗口本身只负责“这一小段时间能否把来袭命中转成 PerfectDodged”，
	// 真正的闪反资格是在成功命中完闪后另外开启。
	CombatComponent->SetPerfectDodgeWindowStateActive(true);
	OpenTimedDefenseWindow(
		PerfectDodgeWindowTimerHandle,
		&UHeroDefenseComponent::ClosePerfectDodgeWindow,
		PerfectDodgeWindowDuration,
		ActionGameplayTags::Player_Event_PerfectDodgeWindow_Open);
}

void UHeroDefenseComponent::ClosePerfectDodgeWindow()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	CloseTimedDefenseWindow(
		PerfectDodgeWindowTimerHandle,
		CombatComponent->IsPerfectDodgeWindowActive(),
		ActionGameplayTags::Player_Event_PerfectDodgeWindow_Close);
	CombatComponent->SetPerfectDodgeWindowStateActive(false);
}

void UHeroDefenseComponent::EnableDodgeCounterWindow()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !GetWorld())
	{
		return;
	}

	// 闪反资格不是简单 bool，它同时承担三层职责：
	// 1. 标记当前允许普通攻击升级成闪避反击；
	// 2. 驱动一个限时窗口；
	// 3. 附带一层“闪反就绪”战斗修正效果。
	CombatComponent->SetDodgeCounterStateAvailable(true);
	OpenTimedDefenseWindow(
		DodgeCounterWindowTimerHandle,
		&UHeroDefenseComponent::DisableDodgeCounterWindow,
		DodgeCounterWindowDuration,
		ActionGameplayTags::Player_Event_DodgeCounter_Available);
	CombatComponent->ApplyComponentCombatModifierEffect(
		DodgeCounterReadyEffectHandle,
		DodgeCounterReadyCombatModifierEffect,
		DodgeCounterWindowDuration);
}

void UHeroDefenseComponent::DisableDodgeCounterWindow()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DodgeCounterWindowTimerHandle);
	}
	// 关闭闪反资格时，必须把计时器、可用状态和就绪效果一起收掉，
	// 避免攻击链仍从旧资格里错误升级出 DodgeCounter 请求。
	CombatComponent->SetDodgeCounterStateAvailable(false);
	CombatComponent->RemoveComponentCombatModifierEffect(DodgeCounterReadyEffectHandle);
}

void UHeroDefenseComponent::HandleDeferredCombatInputRecoveryAfterDodge()
{
	HandleDeferredCombatInputRecovery(bDeferredCombatInputRecoveryAfterDodgeRequested);
}

void UHeroDefenseComponent::HandleDeferredCombatInputRecoveryAfterDefense()
{
	HandleDeferredCombatInputRecovery(bDeferredCombatInputRecoveryAfterDefenseRequested);
}

bool UHeroDefenseComponent::HandleDodgeInput(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag& InputTag,
	EActionInputEvent InputEvent)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || !InActionASC)
	{
		return false;
	}

	if (InputEvent == EActionInputEvent::Held)
	{
		// 闪避虽然本质是一次性动作，但输入层仍记录“当前键还按着”这件事，
		// 方便总控在需要时统一理解这次输入处于什么阶段。
		CombatInputComponent->MarkInputAsHeldByTag(InputTag);
	}

	if (CombatInputComponent->IsInputConsumedByTag(InputTag))
	{
		// 只要这次闪避输入已经被成功受理过，后续同一轮输入阶段直接吞掉，
		// 防止 Pressed / Held 两个阶段重复触发一次闪避。
		return true;
	}

	if (!CanEnterRelationshipActivationForNonAttackInput(InputTag))
	{
		return false;
	}

	// 闪避本质上是一次性动作，因此真正向 ASC 提交时统一走 Pressed 入口。
	if (CombatComponent->IsSpecialWeaponSwitchPresentationInterruptInputAllowed(InputTag))
	{
		// 切武表现期里允许通过主动 GA 抢断窗口切到闪避，
		// 旧 Ability 的正式取消统一交给 ASC 关系裁决入口处理。
	}

	const bool bDodgeTriggered = InActionASC->OnAbilityInputPressed(
		CombatComponent->ResolveLoadoutScopedCombatInputTag(InputTag));
	if (!bDodgeTriggered)
	{
		return false;
	}

	// 只有 ASC 明确接受了这次闪避输入，才把它标记为已消费。
	CombatInputComponent->MarkInputConsumedByTag(InputTag);
	return true;
}

bool UHeroDefenseComponent::HandleCombatModeOrDefensePressed(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag& InputTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !InActionASC)
	{
		return false;
	}

	if (bRequireDefenseReleaseBeforeReactivation)
	{
		// 防御属于“按住维持、松手结束”的状态输入。
		// 如果上一轮防御还没经历过一次完整释放，就不允许再次靠 Pressed 重新点燃。
		return false;
	}

	if (!CanEnterRelationshipActivationForNonAttackInput(InputTag))
	{
		return false;
	}

	// 防御键的 Pressed 阶段当前就按“开始进入 Held 维持语义”处理，
	// 这样角色在第一次按下时就能立刻尝试进入防御，而不是必须等到下一帧 Held。
	// 防御不是一次性动作，而是“按住维持”的状态能力。
	// 所以即使是 Pressed 首帧，也直接按 Held 语义交给 ASC。
	if (CombatComponent->IsSpecialWeaponSwitchPresentationInterruptInputAllowed(InputTag))
	{
		// 主动 GA 抢断统一由 ASC 关系裁决入口执行，这里不再业务层手工先取消切武 Ability。
	}

	return InActionASC->OnAbilityInputHeld(CombatComponent->ResolveLoadoutScopedCombatInputTag(InputTag));
}

bool UHeroDefenseComponent::HandleCombatModeOrDefenseHeld(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag& InputTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || !InActionASC)
	{
		return false;
	}

	if (bRequireDefenseReleaseBeforeReactivation)
	{
		return false;
	}

	if (!CanEnterRelationshipActivationForNonAttackInput(InputTag))
	{
		return false;
	}

	// 防御是真正依赖“持续按住”的状态输入，因此 Held 阶段既要记录按住状态，
	// 也要持续把这次 Held 语义转发给 ASC，保证恢复链回放时仍能按同一语义重新接回防御。
	CombatInputComponent->MarkInputAsHeldByTag(InputTag);
	if (CombatComponent->IsSpecialWeaponSwitchPresentationInterruptInputAllowed(InputTag))
	{
		// 主动 GA 抢断统一由 ASC 关系裁决入口执行，这里不再业务层手工先取消切武 Ability。
	}

	return InActionASC->OnAbilityInputHeld(CombatComponent->ResolveLoadoutScopedCombatInputTag(InputTag));
}

bool UHeroDefenseComponent::TryForwardReleasedAbilityInput(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag& InputTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !InActionASC || !ShouldForwardReleaseToAbilitySystem(InputTag))
	{
		return false;
	}

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense)
	{
		// 防御释放是解除“必须先松手才能再次激活”这条保护的唯一时机。
		ClearDefenseReleaseRequirement();
	}

	// Released 阶段只负责把“这次键已经松开”同步给 ASC，
	// 不在这里额外做状态推进，真正的防御/闪避收尾仍交给各自 GA 生命周期处理。
	// Released 这里只负责同步“键已松开”。
	// 真正是否结束防御、结束闪避或回收窗口，由对应 Ability 生命周期自己决定。
	InActionASC->OnAbilityInputReleased(CombatComponent->ResolveLoadoutScopedCombatInputTag(InputTag));
	return true;
}

bool UHeroDefenseComponent::IsDefenseInputTag(const FGameplayTag& InputTag) const
{
	return InputTag == ActionGameplayTags::InputTag_GameplayAbility_Dodge
		|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense;
}

bool UHeroDefenseComponent::ShouldForwardReleaseToAbilitySystem(const FGameplayTag& InputTag) const
{
	return IsDefenseInputTag(InputTag);
}
