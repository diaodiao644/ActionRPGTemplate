#include "Components/Combat/ActionCombatReactComponent.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "Components/Execution/ExecutionWindowComponent.h"
#include "AnimNotify/AnimNotify_CombatReactRecoveryUnlock.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/ActionDebugHelper.h"
#include "Engine/World.h"
#include "GameBase/ActionPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/SoftObjectPath.h"

namespace
{
    constexpr float ExecutionLockedCombatReactRetryDelay = 0.05f;

    bool ShouldCloseUnconsumedExecutionWindowAfterPoiseBreak(
        const UActionCombatReactComponent* CombatReactComponent,
        const FGameplayTag& ReactEventTag)
    {
        // 这条判断只服务“PoiseBreak 前半段受击表现没有顺利走到 unlock frame”的失败链。
        // 一旦 unlock 已到，后续处决窗口是否继续保留就必须交回 ExecutionWindowComponent，
        // 不能再由 CombatReact 把“受击表现收尾”直接等价成“关闭窗口并恢复 Poise”。
        if (!CombatReactComponent
            || ReactEventTag != ActionGameplayTags::Combat_Event_PoiseBreak
            || CombatReactComponent->HasCombatReactUnlockFrameReached())
        {
            return false;
        }

        const AActor* OwnerActor = CombatReactComponent->GetOwner();
        if (!OwnerActor)
        {
            return false;
        }

        const UExecutionWindowComponent* ExecutionWindowComponent =
            OwnerActor->FindComponentByClass<UExecutionWindowComponent>();
        return ExecutionWindowComponent && ExecutionWindowComponent->IsExecutionWindowOpen();
    }

	void CloseUnconsumedExecutionWindowAfterPoiseBreak(const UActionCombatReactComponent* CombatReactComponent)
	{
		const AActor* OwnerActor = CombatReactComponent ? CombatReactComponent->GetOwner() : nullptr;
		UExecutionWindowComponent* ExecutionWindowComponent =
			OwnerActor ? OwnerActor->FindComponentByClass<UExecutionWindowComponent>() : nullptr;
		if (ExecutionWindowComponent)
		{
			ExecutionWindowComponent->HandlePoiseBreakPresentationEndedWithoutExecution();
		}
	}

	bool HasExecutionExclusiveLock(const UActionCombatReactComponent* CombatReactComponent)
	{
		const AActionCharacterBase* OwnerCharacter =
			CombatReactComponent ? Cast<AActionCharacterBase>(CombatReactComponent->GetOwner()) : nullptr;
		return OwnerCharacter
			&& (OwnerCharacter->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionVictimLocked)
				|| OwnerCharacter->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionVictim_HardLock));
	}

	bool HasExecutionVictimHardLock(const UActionCombatReactComponent* CombatReactComponent)
	{
		const AActionCharacterBase* OwnerCharacter =
			CombatReactComponent ? Cast<AActionCharacterBase>(CombatReactComponent->GetOwner()) : nullptr;
		return OwnerCharacter
			&& OwnerCharacter->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionVictim_HardLock);
	}

    FString CombatReactBoolToDebugText(const bool bValue)
    {
        return bValue ? TEXT("\u662f") : TEXT("\u5426");
    }

    FString TagToDebugText(const FGameplayTag& Tag)
    {
        return Tag.IsValid() ? Tag.ToString() : TEXT("\u65e0");
    }

    FString ReactDirectionToDebugText(const EActionCombatReactDirection Direction)
    {
        switch (Direction)
        {
        case EActionCombatReactDirection::Front:
            return TEXT("\u524d");

        case EActionCombatReactDirection::Left:
            return TEXT("\u5de6");

        case EActionCombatReactDirection::Right:
            return TEXT("\u53f3");

        case EActionCombatReactDirection::Back:
            return TEXT("\u540e");

        default:
            break;
        }

        return TEXT("\u65e0");
    }

    FString ReactPhaseToDebugText(const EActionCombatReactPhase Phase)
    {
        switch (Phase)
        {
        case EActionCombatReactPhase::Reacting:
            return TEXT("\u53d7\u51fb\u4e2d");

        case EActionCombatReactPhase::AirborneUncontrolled:
            return TEXT("\u7a7a\u4e2d\u5931\u63a7\u4e2d");

        case EActionCombatReactPhase::Recovery:
            return TEXT("\u6062\u590d\u4e2d");

        default:
            break;
        }

        return TEXT("\u65e0");
    }

	FString RunningAnimationSemanticToDebugText(const EActionRunningAnimationSemantic Semantic)
	{
		switch (Semantic)
		{
		case EActionRunningAnimationSemantic::NonReact:
			return TEXT("NonReact");

		case EActionRunningAnimationSemantic::CombatReact:
			return TEXT("CombatReact");

		case EActionRunningAnimationSemantic::Execution:
			return TEXT("Execution");

		default:
			break;
		}

		return TEXT("None");
	}

}

UActionCombatReactComponent::UActionCombatReactComponent()
	: Super()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UActionCombatReactComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureCombatReactAssetsLoaded();
}

void UActionCombatReactComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bCombatReactActive || !bWaitingForLanding)
	{
		// 只有“当前仍在受击链里，并且这条链正在等待落地收尾”时才继续 Tick。
		SetComponentTickEnabled(false);
		return;
	}

	if (const UCharacterMovementComponent* MovementComponent = GetOwningMovementComponent())
	{
		if (!MovementComponent->IsFalling())
		{
			if (HasExecutionExclusiveLock(this))
			{
				// 目标已进入处决保护链时，落地后的普通受击收尾继续挂起，避免和处决主链抢状态。
				return;
			}

			HandleCombatReactLanded();
		}
	}
}

FString UActionCombatReactComponent::DescribeCurrentCombatReactState() const
{
    if (!bCombatReactActive)
    {
        return TEXT("\u5f53\u524d\u672a\u5904\u4e8e\u53d7\u51fb\u72b6\u6001\u3002");
    }

    return FString::Printf(
        TEXT("\u53d7\u51fb\u72b6\u6001\uff1a\u6fc0\u6d3b=%s\uff0c\u4e8b\u4ef6=%s\uff0c\u4f18\u5148\u7ea7=%d\uff0c\u65b9\u5411=%s\uff0c\u9636\u6bb5=%s\uff0c\u4e3b\u53d7\u51fb=%s\uff0c\u7a7a\u4e2d\u5931\u63a7=%s\uff0c\u5c3e\u6bb5\u5f00\u7a97=%s\uff0c\u5df2\u6536\u5230\u5f00\u7a97Notify=%s\uff0c\u72b6\u6001Tag=%s\uff0c\u6548\u679cTag=%s\uff0c\u7b49\u5f85\u843d\u5730=%s\uff0c\u7b49\u5f85\u8ba1\u65f6\u6536\u5c3e=%s\u3002"),
        *CombatReactBoolToDebugText(bCombatReactActive),
        *TagToDebugText(CurrentReactEventTag),
        ResolveCombatReactPriority(CurrentReactEventTag),
        *ReactDirectionToDebugText(CurrentReactDirection),
        *ReactPhaseToDebugText(CurrentReactPhase),
        *CombatReactBoolToDebugText(IsPrimaryReactPhaseActive()),
        *CombatReactBoolToDebugText(IsAirborneUncontrolledActive()),
        *CombatReactBoolToDebugText(IsRecoveryPhaseActive()),
        *CombatReactBoolToDebugText(bCombatReactUnlockFrameReached),
        *TagToDebugText(CurrentReactStateTag),
        *TagToDebugText(CurrentReactStatusEffectTag),
        *CombatReactBoolToDebugText(bWaitingForLanding),
        *CombatReactBoolToDebugText(bHoldReactUntilTimerExpires));
}

int32 UActionCombatReactComponent::ResolveCombatReactPriority(const FGameplayTag EventTag) const
{
	if (EventTag == ActionGameplayTags::Combat_Event_Knockdown)
	{
		return 40;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Launch)
	{
		return 40;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_GuardBreak)
	{
		return 20;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_HeavyHitReact)
	{
		return 15;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		return 30;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_HitReact)
	{
		return 10;
	}

	return 0;
}

bool UActionCombatReactComponent::IsRepeatableCombatReactEvent(const FGameplayTag EventTag) const
{
	return EventTag == ActionGameplayTags::Combat_Event_HitReact
		|| EventTag == ActionGameplayTags::Combat_Event_HeavyHitReact
		|| EventTag == ActionGameplayTags::Combat_Event_GuardBreak
		|| EventTag == ActionGameplayTags::Combat_Event_PoiseBreak
		|| EventTag == ActionGameplayTags::Combat_Event_Launch
		|| EventTag == ActionGameplayTags::Combat_Event_Knockdown;
}

bool UActionCombatReactComponent::IsSamePriorityRepeatableCombatReact(const FGameplayTag EventTag) const
{
	if (!bCombatReactActive || !CurrentReactEventTag.IsValid() || !EventTag.IsValid())
	{
		return false;
	}

	return EventTag == CurrentReactEventTag
		&& IsRepeatableCombatReactEvent(EventTag)
		&& ResolveCombatReactPriority(EventTag) == ResolveCombatReactPriority(CurrentReactEventTag);
}

bool UActionCombatReactComponent::CanIncomingCombatReactOverrideCurrent(const FGameplayTag EventTag) const
{
	if (!EventTag.IsValid())
	{
		return false;
	}

	if (!bCombatReactActive || !CurrentReactEventTag.IsValid())
	{
		if (const UPawnCombatComponent* CombatComponent = GetOwningCombatComponent())
		{
			const FActionRunningAnimationReactGuardContext& RunningContext =
				CombatComponent->GetRunningAnimationReactGuardContext();
			if (RunningContext.Semantic == EActionRunningAnimationSemantic::NonReact)
			{
				return !CombatComponent->IsCurrentRunningAnimationProtectedFromIncomingReact(
					ResolveCombatReactPriority(EventTag));
			}
		}

		return true;
	}

	if (IsRecoveryPhaseActive())
	{
		return true;
	}

	if (CurrentReactPhase == EActionCombatReactPhase::Reacting
		|| CurrentReactPhase == EActionCombatReactPhase::AirborneUncontrolled)
	{
		const int32 IncomingPriority = ResolveCombatReactPriority(EventTag);
		const int32 CurrentPriority = ResolveCombatReactPriority(CurrentReactEventTag);
		return IncomingPriority > CurrentPriority
			|| IsSamePriorityRepeatableCombatReact(EventTag);
	}

	return true;
}

FString UActionCombatReactComponent::DescribeIncomingCombatReactOverrideDecision(const FGameplayTag EventTag) const
{
	if (!EventTag.IsValid())
	{
		return TEXT("Combat react override decision: block, reason=invalid_incoming_event.");
	}

	if (!bCombatReactActive || !CurrentReactEventTag.IsValid())
	{
		if (const UPawnCombatComponent* CombatComponent = GetOwningCombatComponent())
		{
			const FActionRunningAnimationReactGuardContext& RunningContext =
				CombatComponent->GetRunningAnimationReactGuardContext();
			if (RunningContext.Semantic == EActionRunningAnimationSemantic::NonReact)
			{
				const int32 IncomingPriority = ResolveCombatReactPriority(EventTag);
				if (CombatComponent->IsCurrentRunningAnimationProtectedFromIncomingReact(IncomingPriority))
				{
					return FString::Printf(
						TEXT("Combat react override decision: block, reason=blocked_by_running_animation_threshold, incoming=%s priority=%d, semantic=%s, threshold=%d, montage=%s."),
						*TagToDebugText(EventTag),
						IncomingPriority,
						*RunningAnimationSemanticToDebugText(RunningContext.Semantic),
						RunningContext.MinIncomingReactPriorityToInterrupt,
						*GetNameSafe(RunningContext.Montage));
				}
			}
		}

		return FString::Printf(
			TEXT("Combat react override decision: allow, reason=no_active_react, incoming=%s priority=%d."),
			*TagToDebugText(EventTag),
			ResolveCombatReactPriority(EventTag));
	}

	if (IsRecoveryPhaseActive())
	{
		return FString::Printf(
			TEXT("Combat react override decision: allow, reason=recovery_phase, current=%s priority=%d, incoming=%s priority=%d."),
			*TagToDebugText(CurrentReactEventTag),
			ResolveCombatReactPriority(CurrentReactEventTag),
			*TagToDebugText(EventTag),
			ResolveCombatReactPriority(EventTag));
	}

	if (CurrentReactPhase == EActionCombatReactPhase::Reacting
		|| CurrentReactPhase == EActionCombatReactPhase::AirborneUncontrolled)
	{
		const int32 CurrentPriority = ResolveCombatReactPriority(CurrentReactEventTag);
		const int32 IncomingPriority = ResolveCombatReactPriority(EventTag);
		const bool bAllowHigherPriorityOverride = IncomingPriority > CurrentPriority;
		const bool bAllowSamePriorityReplay = IsSamePriorityRepeatableCombatReact(EventTag);
		return FString::Printf(
			TEXT("Combat react override decision: %s, reason=%s, phase=%s, current=%s priority=%d, incoming=%s priority=%d."),
			(bAllowHigherPriorityOverride || bAllowSamePriorityReplay) ? TEXT("allow") : TEXT("block"),
			bAllowHigherPriorityOverride
				? TEXT("higher_priority_override")
				: (bAllowSamePriorityReplay ? TEXT("same_priority_same_event_replay") : TEXT("same_or_lower_priority_rejected")),
			*ReactPhaseToDebugText(CurrentReactPhase),
			*TagToDebugText(CurrentReactEventTag),
			CurrentPriority,
			*TagToDebugText(EventTag),
			IncomingPriority);
	}

	return FString::Printf(
		TEXT("Combat react override decision: allow, reason=phase_fallback, phase=%s, current=%s, incoming=%s."),
		*ReactPhaseToDebugText(CurrentReactPhase),
		*TagToDebugText(CurrentReactEventTag),
		*TagToDebugText(EventTag));
}

bool UActionCombatReactComponent::CanActivateAbilityDuringRecoveryCancelWindow(const FGameplayTag& InputTag) const
{
	if (!IsRecoveryPhaseActive() || !InputTag.IsValid())
	{
		return false;
	}

	const UHeroCombatComponent* HeroCombatComponent = Cast<UHeroCombatComponent>(GetOwningCombatComponent());
	return HeroCombatComponent && HeroCombatComponent->IsCombatReactRecoveryCancelInputAllowed(InputTag);
}

void UActionCombatReactComponent::PrintCurrentCombatReactStateDebug() const
{
	Debug::Print(DescribeCurrentCombatReactState(), FColor::Cyan, 4.0f);
}

bool UActionCombatReactComponent::HandleCombatReactEvent(
	const FGameplayTag EventTag,
	const FActionDamagePayload& DamagePayload)
{
	EnsureCombatReactAssetsLoaded();

	if (!EventTag.IsValid())
	{
		return false;
	}

	const FGameplayTag ReactStateTag = ResolveCombatReactStateTag(EventTag);
	if (!ReactStateTag.IsValid())
	{
		return false;
	}

	if (HasExecutionVictimHardLock(this))
	{
		UE_LOG(
			ActionRPG,
			Log,
			TEXT("Combat react rejected: owner=%s event=%s reason=execution_recovery_hard_lock."),
			*GetNameSafe(GetOwner()),
			*EventTag.ToString());
		return false;
	}

	EActionCombatReactDirection ResolvedDirection = EActionCombatReactDirection::None;
	UAnimMontage* ReactMontage = ResolveCombatReactMontage(EventTag, DamagePayload, ResolvedDirection);
	UAnimInstance* OwnerAnimInstance = GetOwnerAnimInstance();
	if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		const FActionCombatReactMontageSet* PoiseBreakMontageSet = GetCombatReactMontageSetForEvent(EventTag);
		UE_LOG(
			ActionRPG,
			Log,
			TEXT("poise_break_react_diagnostic_begin owner=%s montage_set=%s assets_loaded=%s resolved_montage=%s direction=%d has_unlock_notify=%s anim_instance=%s"),
			*GetNameSafe(GetOwner()),
			(PoiseBreakMontageSet && PoiseBreakMontageSet->HasAnyMontage()) ? TEXT("configured") : TEXT("missing"),
			bCombatReactAssetsLoaded ? TEXT("true") : TEXT("false"),
			*GetNameSafe(ReactMontage),
			static_cast<int32>(ResolvedDirection),
			(ReactMontage && HasCombatReactUnlockNotify(ReactMontage)) ? TEXT("true") : TEXT("false"),
			OwnerAnimInstance ? TEXT("true") : TEXT("false"));
	}

	if (ReactMontage && !HasCombatReactUnlockNotify(ReactMontage))
	{
		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("Combat react montage rejected: owner=%s event=%s montage=%s reason=missing CombatReactRecoveryUnlock notify."),
			*GetNameSafe(GetOwner()),
			*EventTag.ToString(),
			*GetNameSafe(ReactMontage));
		return false;
	}

	if (!ReactMontage)
	{
		if (const FActionCombatReactMontageSet* ReactMontageSet = GetCombatReactMontageSetForEvent(EventTag))
		{
			if (!ReactMontageSet->HasAnyMontage())
			{
				UE_LOG(
					ActionRPG,
					Warning,
					TEXT("Combat react montage rejected: owner=%s event=%s reason=missing montage set in CombatReact config."),
					*GetNameSafe(GetOwner()),
					*EventTag.ToString());
			}
			else if (!bCombatReactAssetsLoaded)
			{
				UE_LOG(
					ActionRPG,
					Warning,
					TEXT("Combat react montage rejected: owner=%s event=%s reason=soft asset not loaded."),
					*GetNameSafe(GetOwner()),
					*EventTag.ToString());
			}
			else
			{
				UE_LOG(
					ActionRPG,
					Warning,
					TEXT("Combat react montage rejected: owner=%s event=%s reason=direction montage unresolved or asset load failed. direction=%d."),
					*GetNameSafe(GetOwner()),
					*EventTag.ToString(),
					static_cast<int32>(ResolvedDirection));
			}
		}
	}

	if (ReactMontage && !OwnerAnimInstance)
	{
		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("Combat react montage rejected: owner=%s event=%s montage=%s reason=AnimInstance is invalid."),
			*GetNameSafe(GetOwner()),
			*EventTag.ToString(),
			*GetNameSafe(ReactMontage));
		return false;
	}

	if (!CanIncomingCombatReactOverrideCurrent(EventTag))
	{
		UE_LOG(
			ActionRPG,
			Log,
			TEXT("%s"),
			*DescribeIncomingCombatReactOverrideDecision(EventTag));
		return false;
	}

	// 新受击主链开始前，先把上一轮正式受击状态完整收掉。
	FinishCombatReact(true, true);
	PrepareOwnerCombatStateForReact();

	CurrentCombatReactMontage = ReactMontage;
	CurrentReactDirection = ResolvedDirection;
	CurrentReactPhase = EActionCombatReactPhase::None;
	CurrentReactEventTag = EventTag;
	CurrentReactStateTag = ReactStateTag;
	CurrentReactStatusEffectTag = ResolveCombatReactStatusEffectTag(EventTag);
	bWaitingForLanding = false;
	bHoldReactUntilTimerExpires = false;
	bCombatReactUnlockFrameReached = false;
	bHandedOffToExecutionVictim = false;
	PendingLandingRecoveryDuration = 0.f;
	bCombatReactActive = true;

	if (ReactMontage)
	{
		if (UPawnCombatComponent* CombatComponent = GetOwningCombatComponent())
		{
			CombatComponent->SetRunningAnimationReactGuardContext(
				ReactMontage,
				EActionRunningAnimationSemantic::CombatReact,
				0);
		}
	}

	ApplyCombatReactState(ReactStateTag, DamagePayload);
	ApplyKnockbackIfNeeded(DamagePayload);
	K2_OnCombatReactStarted(EventTag, ResolvedDirection, DamagePayload.InstigatorActor);

	if (!ReactMontage)
	{
		if (!bWaitingForLanding
			&& ResolveCombatReactDuration(nullptr, DamagePayload) <= 0.f)
		{
			FinishCombatReact(false, true);
		}

		return true;
	}

	if (OwnerAnimInstance->Montage_Play(ReactMontage) <= 0.f)
	{
		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("Combat react montage rejected: owner=%s event=%s montage=%s reason=Montage_Play failed."),
			*GetNameSafe(GetOwner()),
			*EventTag.ToString(),
			*GetNameSafe(ReactMontage));
		if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
		{
			UE_LOG(
				ActionRPG,
				Warning,
				TEXT("poise_break_react_diagnostic_play_failed owner=%s montage=%s direction=%d"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(ReactMontage),
				static_cast<int32>(ResolvedDirection));
		}
		FinishCombatReact(true, false);
		return false;
	}

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &ThisClass::HandleCombatReactMontageEnded);
	OwnerAnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, ReactMontage);
	return true;
}

bool UActionCombatReactComponent::HandOffActiveCombatReactToExecutionVictim(FString* OutFailureReason)
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	UE_LOG(
		ActionRPG,
		Log,
		TEXT("execution_victim_combat_react_handoff_begin owner=%s current_react_event=%s current_react_montage=%s combat_react_active=%s movement_restricted=%s waiting_for_landing=%s hold_until_timer_expires=%s handed_off=%s"),
		*GetNameSafe(GetOwner()),
		*CurrentReactEventTag.ToString(),
		*GetNameSafe(CurrentCombatReactMontage),
		bCombatReactActive ? TEXT("true") : TEXT("false"),
		bCombatReactMovementRestricted ? TEXT("true") : TEXT("false"),
		bWaitingForLanding ? TEXT("true") : TEXT("false"),
		bHoldReactUntilTimerExpires ? TEXT("true") : TEXT("false"),
		bHandedOffToExecutionVictim ? TEXT("true") : TEXT("false"));

	if (!bCombatReactActive)
	{
		UE_LOG(
			ActionRPG,
			Log,
			TEXT("execution_victim_combat_react_handoff_success owner=%s current_react_event=%s current_react_montage=%s combat_react_active=false movement_restricted=%s waiting_for_landing=%s hold_until_timer_expires=%s handed_off=false reason=no_active_combat_react"),
			*GetNameSafe(GetOwner()),
			*CurrentReactEventTag.ToString(),
			*GetNameSafe(CurrentCombatReactMontage),
			bCombatReactMovementRestricted ? TEXT("true") : TEXT("false"),
			bWaitingForLanding ? TEXT("true") : TEXT("false"),
			bHoldReactUntilTimerExpires ? TEXT("true") : TEXT("false"));
		return true;
	}

	if (CurrentReactEventTag != ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("当前受击运行态 %s 不是 PoiseBreak，不能直接交接给处决 victim 演出。"),
				*CurrentReactEventTag.ToString());
		}

		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("execution_victim_combat_react_handoff_failed owner=%s current_react_event=%s current_react_montage=%s combat_react_active=true movement_restricted=%s waiting_for_landing=%s hold_until_timer_expires=%s handed_off=%s reason=non_poise_break_react"),
			*GetNameSafe(GetOwner()),
			*CurrentReactEventTag.ToString(),
			*GetNameSafe(CurrentCombatReactMontage),
			bCombatReactMovementRestricted ? TEXT("true") : TEXT("false"),
			bWaitingForLanding ? TEXT("true") : TEXT("false"),
			bHoldReactUntilTimerExpires ? TEXT("true") : TEXT("false"),
			bHandedOffToExecutionVictim ? TEXT("true") : TEXT("false"));
		return false;
	}

	ResetActiveCombatReactRuntimeForExecutionVictimHandoff();
	bHandedOffToExecutionVictim = true;

	UE_LOG(
		ActionRPG,
		Log,
		TEXT("execution_victim_combat_react_handoff_success owner=%s current_react_event=%s current_react_montage=%s combat_react_active=%s movement_restricted=%s waiting_for_landing=%s hold_until_timer_expires=%s handed_off=%s reason=poise_break_handed_off_to_execution_victim"),
		*GetNameSafe(GetOwner()),
		*CurrentReactEventTag.ToString(),
		*GetNameSafe(CurrentCombatReactMontage),
		bCombatReactActive ? TEXT("true") : TEXT("false"),
		bCombatReactMovementRestricted ? TEXT("true") : TEXT("false"),
		bWaitingForLanding ? TEXT("true") : TEXT("false"),
		bHoldReactUntilTimerExpires ? TEXT("true") : TEXT("false"),
		bHandedOffToExecutionVictim ? TEXT("true") : TEXT("false"));
	return true;
}

bool UActionCombatReactComponent::EnsureCombatReactAssetsLoaded()
{
	if (bCombatReactAssetsLoaded)
	{
		return true;
	}

	LoadedCombatReactMontages.Reset();

	TArray<FSoftObjectPath> CombatReactAssetPaths;
	CombatReactConfig.CollectSoftObjectPaths(CombatReactAssetPaths);
	if (CombatReactAssetPaths.Num() <= 0)
	{
		bCombatReactAssetsLoaded = true;
		return true;
	}

	bool bAllAssetsLoaded = true;
	for (const FSoftObjectPath& AssetPath : CombatReactAssetPaths)
	{
		if (AssetPath.IsNull())
		{
			continue;
		}

		UObject* LoadedObject = AssetPath.ResolveObject();
		if (!LoadedObject)
		{
			LoadedObject = AssetPath.TryLoad();
		}

		UAnimMontage* LoadedMontage = Cast<UAnimMontage>(LoadedObject);
		if (!LoadedMontage)
		{
			bAllAssetsLoaded = false;
			UE_LOG(
				ActionRPG,
				Warning,
			TEXT("Combat react asset load failed: owner=%s asset=%s."),
				*GetNameSafe(GetOwner()),
				*AssetPath.ToString());
			continue;
		}

		LoadedCombatReactMontages.AddUnique(LoadedMontage);
	}

	bCombatReactAssetsLoaded = bAllAssetsLoaded;
	return bAllAssetsLoaded;
}

bool UActionCombatReactComponent::HasCombatReactUnlockNotify(const UAnimMontage* ReactMontage) const
{
	if (!ReactMontage)
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : ReactMontage->Notifies)
	{
		if (NotifyEvent.Notify
			&& NotifyEvent.Notify->IsA(UAnimNotify_CombatReactRecoveryUnlock::StaticClass()))
		{
			return true;
		}
	}

	return false;
}

float UActionCombatReactComponent::ResolveCombatReactUnlockNotifyTriggerTime(const UAnimMontage* ReactMontage) const
{
	if (!ReactMontage)
	{
		return 0.f;
	}

	float UnlockTriggerTime = 0.f;
	for (const FAnimNotifyEvent& NotifyEvent : ReactMontage->Notifies)
	{
		if (NotifyEvent.Notify
			&& NotifyEvent.Notify->IsA(UAnimNotify_CombatReactRecoveryUnlock::StaticClass()))
		{
			UnlockTriggerTime = FMath::Max(UnlockTriggerTime, NotifyEvent.GetTriggerTime());
		}
	}

	return UnlockTriggerTime;
}

float UActionCombatReactComponent::ResolveMinimumCombatReactDurationForUnlockFrame(
	UAnimMontage* ReactMontage,
	const float BaseReactDuration) const
{
	if (CurrentReactEventTag != ActionGameplayTags::Combat_Event_PoiseBreak
		|| !ReactMontage
		|| !HasCombatReactUnlockNotify(ReactMontage))
	{
		return BaseReactDuration;
	}

	const float UnlockTriggerTime = ResolveCombatReactUnlockNotifyTriggerTime(ReactMontage);
	if (UnlockTriggerTime <= 0.f)
	{
		return BaseReactDuration;
	}

	// PoiseBreak 受击链至少要保到 unlock frame 之后一点点，避免时长覆盖值比解锁帧更短时提前过期。
	return FMath::Max(BaseReactDuration, UnlockTriggerTime + KINDA_SMALL_NUMBER);
}

void UActionCombatReactComponent::OpenCombatReactRecoveryCancelWindow()
{
	UHeroCombatComponent* HeroCombatComponent = Cast<UHeroCombatComponent>(GetOwningCombatComponent());
	if (!HeroCombatComponent)
	{
		return;
	}

	HeroCombatComponent->OpenAbilityCancelWindow(CombatReactConfig.RecoveryCancelInputTags);
}

void UActionCombatReactComponent::CloseCombatReactRecoveryCancelWindow()
{
	UHeroCombatComponent* HeroCombatComponent = Cast<UHeroCombatComponent>(GetOwningCombatComponent());
	if (!HeroCombatComponent)
	{
		return;
	}

	HeroCombatComponent->CloseAbilityCancelWindow();
}

void UActionCombatReactComponent::ResetActiveCombatReactRuntimeForExecutionVictimHandoff()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CombatReactTimerHandle);
	}

	bWaitingForLanding = false;
	bHoldReactUntilTimerExpires = false;
	PendingLandingRecoveryDuration = 0.f;

	if (UPawnCombatComponent* CombatComponent = GetOwningCombatComponent())
	{
		const FActionRunningAnimationReactGuardContext& RunningContext =
			CombatComponent->GetRunningAnimationReactGuardContext();
		if (RunningContext.Semantic == EActionRunningAnimationSemantic::CombatReact
			&& RunningContext.Montage == CurrentCombatReactMontage)
		{
			CombatComponent->ClearRunningAnimationReactGuardContext();
		}
	}

	StopCurrentCombatReactMontage();
	RemoveCombatReactStateEffect();
	ClearCombatReactMovementRestriction();
	CloseCombatReactRecoveryCancelWindow();

	bCombatReactActive = false;
	CurrentCombatReactMontage = nullptr;
	CurrentReactDirection = EActionCombatReactDirection::None;
	CurrentReactPhase = EActionCombatReactPhase::None;
	CurrentReactEventTag = FGameplayTag();
	CurrentReactStateTag = FGameplayTag();
	CurrentReactStatusEffectTag = FGameplayTag();
	bCombatReactUnlockFrameReached = false;
}

void UActionCombatReactComponent::NotifyCombatReactUnlockFrame()
{
	if (!bCombatReactActive || bCombatReactUnlockFrameReached)
	{
		return;
	}

	// 非处决受击的正式恢复开窗只认这一帧 Notify。
	bCombatReactUnlockFrameReached = true;
	CurrentReactPhase = EActionCombatReactPhase::Recovery;
	ClearCombatReactMovementRestriction();
	OpenCombatReactRecoveryCancelWindow();

	if (UPawnCombatComponent* CombatComponent = GetOwningCombatComponent())
	{
		CombatComponent->SetAttackEnabled(true);
	}

	if (const AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(GetOwningCharacter()))
	{
		if (UHeroCombatInputComponent* HeroCombatInputComponent = HeroCharacter->GetHeroCombatInputComponent())
		{
			HeroCombatInputComponent->RequestReplayHeldInputsAfterCombatReact();
		}
	}
}

UAnimMontage* UActionCombatReactComponent::ResolveCombatReactMontage(
	const FGameplayTag EventTag,
	const FActionDamagePayload& DamagePayload,
	EActionCombatReactDirection& OutReactDirection) const
{
	OutReactDirection = ResolveCombatReactDirection(DamagePayload);

	if (EventTag == ActionGameplayTags::Combat_Event_HitReact)
	{
		return CombatReactConfig.HitReactMontages.ResolveMontage(OutReactDirection);
	}

	if (EventTag == ActionGameplayTags::Combat_Event_HeavyHitReact)
	{
		return CombatReactConfig.HeavyHitReactMontages.ResolveMontage(OutReactDirection);
	}

	if (EventTag == ActionGameplayTags::Combat_Event_GuardBreak)
	{
		return CombatReactConfig.GuardBreakMontages.ResolveMontage(OutReactDirection);
	}

	if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		return CombatReactConfig.PoiseBreakMontages.ResolveMontage(OutReactDirection);
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Launch)
	{
		return CombatReactConfig.LaunchMontages.ResolveMontage(OutReactDirection);
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Knockdown)
	{
		return CombatReactConfig.KnockdownMontages.ResolveMontage(OutReactDirection);
	}

	return nullptr;
}

const FActionCombatReactMontageSet* UActionCombatReactComponent::GetCombatReactMontageSetForEvent(
	const FGameplayTag EventTag) const
{
	if (EventTag == ActionGameplayTags::Combat_Event_HitReact)
	{
		return &CombatReactConfig.HitReactMontages;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_HeavyHitReact)
	{
		return &CombatReactConfig.HeavyHitReactMontages;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_GuardBreak)
	{
		return &CombatReactConfig.GuardBreakMontages;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		return &CombatReactConfig.PoiseBreakMontages;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Launch)
	{
		return &CombatReactConfig.LaunchMontages;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Knockdown)
	{
		return &CombatReactConfig.KnockdownMontages;
	}

	return nullptr;
}

FGameplayTag UActionCombatReactComponent::ResolveCombatReactStateTag(const FGameplayTag EventTag) const
{
	if (EventTag == ActionGameplayTags::Combat_Event_HitReact)
	{
		return ActionGameplayTags::State_Combat_HitStun_Active;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_HeavyHitReact)
	{
		return ActionGameplayTags::State_Combat_HeavyHitReact_Active;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_GuardBreak)
	{
		return ActionGameplayTags::State_Combat_GuardBreak_Active;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		return ActionGameplayTags::State_Combat_PoiseBreak_Active;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Launch)
	{
		return ActionGameplayTags::State_Combat_Launch_Active;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Knockdown)
	{
		return ActionGameplayTags::State_Combat_Knockdown_Active;
	}

	return FGameplayTag();
}

FGameplayTag UActionCombatReactComponent::ResolveCombatReactStatusEffectTag(const FGameplayTag EventTag) const
{
	if (EventTag == ActionGameplayTags::Combat_Event_HitReact)
	{
		return ActionGameplayTags::StatusEffect_Combat_HitStun;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_HeavyHitReact)
	{
		return ActionGameplayTags::StatusEffect_Combat_HeavyHitReact;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_GuardBreak)
	{
		return ActionGameplayTags::StatusEffect_Combat_GuardBreak;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		return ActionGameplayTags::StatusEffect_Combat_PoiseBreak;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Launch)
	{
		return ActionGameplayTags::StatusEffect_Combat_Launch;
	}

	if (EventTag == ActionGameplayTags::Combat_Event_Knockdown)
	{
		return ActionGameplayTags::StatusEffect_Combat_Knockdown;
	}

	return FGameplayTag();
}

EActionCombatReactDirection UActionCombatReactComponent::ResolveCombatReactDirection(
	const FActionDamagePayload& DamagePayload) const
{
	const AActionCharacterBase* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter)
	{
		return EActionCombatReactDirection::None;
	}

	FVector SourceDirection = FVector::ZeroVector;

	if (IsValid(DamagePayload.SourceActor))
	{
		SourceDirection = (DamagePayload.SourceActor->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
	}
	else if (IsValid(DamagePayload.InstigatorActor))
	{
		SourceDirection = (DamagePayload.InstigatorActor->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
	}
	else if (!DamagePayload.ImpactDirection.IsNearlyZero())
	{
		SourceDirection = (-DamagePayload.ImpactDirection).GetSafeNormal2D();
	}

	if (SourceDirection.IsNearlyZero())
	{
		return EActionCombatReactDirection::Front;
	}

	const FVector OwnerForward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector OwnerRight = OwnerCharacter->GetActorRightVector().GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(OwnerForward, SourceDirection);
	const float RightDot = FVector::DotProduct(OwnerRight, SourceDirection);

	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		return ForwardDot >= 0.f
			? EActionCombatReactDirection::Front
			: EActionCombatReactDirection::Back;
	}

	return RightDot >= 0.f
		? EActionCombatReactDirection::Right
		: EActionCombatReactDirection::Left;
}

void UActionCombatReactComponent::PrepareOwnerCombatStateForReact() const
{
	UPawnCombatComponent* CombatComponent = GetOwningCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	if (CombatReactConfig.bStopCurrentCombatMontageOnReact)
	{
		CombatComponent->StopCurrentAttackMontage();
	}

	if (CombatReactConfig.bResetComboIndexOnReact)
	{
		CombatComponent->ResetComboIndex();
	}

	if (CombatReactConfig.bResetCombatModeToIdleOnReact)
	{
		CombatComponent->SetCombatMode(EHeroCombatMode::Idle);
	}

	// 受击正式开始后，主动攻击入口一律先关闭。
	CombatComponent->SetAttackEnabled(false);
}

void UActionCombatReactComponent::ApplyCombatReactState(
	const FGameplayTag& StateTag,
	const FActionDamagePayload& DamagePayload)
{
	// 这里是“受击正式开始后”的核心状态写入点。
	CloseCombatReactRecoveryCancelWindow();
	ApplyCombatReactMovementRestriction();
	CurrentReactPhase = EActionCombatReactPhase::Reacting;
	ApplyCombatReactStateEffect(
		StateTag,
		CurrentReactStatusEffectTag,
		ResolveCombatReactEffectDuration(DamagePayload));

	const float ReactDuration = ResolveCombatReactDuration(CurrentCombatReactMontage, DamagePayload);
	const float EffectiveReactDuration =
		ResolveMinimumCombatReactDurationForUnlockFrame(CurrentCombatReactMontage, ReactDuration);
	PendingLandingRecoveryDuration = EffectiveReactDuration;
	bCombatReactUnlockFrameReached = false;

	if (ShouldWaitForLandingToFinishReact())
	{
		// 需要等落地的受击链先进入空中失控阶段，最终恢复或收尾交给落地回调。
		CurrentReactPhase = EActionCombatReactPhase::AirborneUncontrolled;
		bWaitingForLanding = true;
		SetComponentTickEnabled(true);
	}
	else if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CombatReactTimerHandle);
		if (EffectiveReactDuration > 0.f)
		{
			const float MontageLength = CurrentCombatReactMontage ? CurrentCombatReactMontage->GetPlayLength() : 0.f;
			// 持续时间长于演出时，保留一段“演出后状态尾巴”。
			bHoldReactUntilTimerExpires = MontageLength > 0.f && EffectiveReactDuration > MontageLength;
			GetWorld()->GetTimerManager().SetTimer(
				CombatReactTimerHandle,
				this,
				&ThisClass::OnCombatReactExpired,
				EffectiveReactDuration,
				false);
		}
	}
}

void UActionCombatReactComponent::ApplyKnockbackIfNeeded(const FActionDamagePayload& DamagePayload) const
{
	AActionCharacterBase* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter || DamagePayload.KnockbackStrength <= 0.f)
	{
		return;
	}

	FVector KnockbackDirection = DamagePayload.ImpactDirection.GetSafeNormal2D();
	if (KnockbackDirection.IsNearlyZero())
	{
		const AActor* ImpactSourceActor = IsValid(DamagePayload.SourceActor)
			? DamagePayload.SourceActor.Get()
			: DamagePayload.InstigatorActor.Get();
		if (IsValid(ImpactSourceActor))
		{
			KnockbackDirection =
				(OwnerCharacter->GetActorLocation() - ImpactSourceActor->GetActorLocation()).GetSafeNormal2D();
			if (KnockbackDirection.IsNearlyZero())
			{
				KnockbackDirection = ImpactSourceActor->GetActorForwardVector().GetSafeNormal2D();
			}
		}
	}

	if (KnockbackDirection.IsNearlyZero())
	{
		return;
	}

	OwnerCharacter->LaunchCharacter(
		KnockbackDirection * DamagePayload.KnockbackStrength,
		true,
		true);
}

void UActionCombatReactComponent::ApplyCombatReactStateEffect(
	const FGameplayTag& StateTag,
	const FGameplayTag& StatusEffectTag,
	const float Duration)
{
	RemoveCombatReactStateEffect();

	if (!StateTag.IsValid() || Duration <= 0.f)
	{
		return;
	}

	UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent();
	if (!ActionAbilitySystemComponent)
	{
		return;
	}

	FActionCombatModifierEffectSpec EffectSpec;
	EffectSpec.Duration = Duration;
	EffectSpec.StatusEffectTag = StatusEffectTag;
	EffectSpec.GrantedTags.AddTag(StateTag);

	CurrentCombatReactEffectHandle = ActionAbilitySystemComponent->ApplyCombatModifierEffect(EffectSpec);
}

void UActionCombatReactComponent::RemoveCombatReactStateEffect()
{
	if (!CurrentCombatReactEffectHandle.IsValid())
	{
		return;
	}

	if (UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent())
	{
		ActionAbilitySystemComponent->RemoveActiveGameplayEffect(CurrentCombatReactEffectHandle);
	}

	CurrentCombatReactEffectHandle.Invalidate();
}

void UActionCombatReactComponent::ApplyCombatReactMovementRestriction()
{
	UCharacterMovementComponent* MovementComponent = GetOwningMovementComponent();
	if (!MovementComponent || bCombatReactMovementRestricted)
	{
		return;
	}

	CachedMaxWalkSpeed = MovementComponent->MaxWalkSpeed;
	CachedMaxAcceleration = MovementComponent->MaxAcceleration;
	MovementComponent->MaxWalkSpeed = 0.f;
	MovementComponent->MaxAcceleration = 0.f;

	if (CurrentReactEventTag != ActionGameplayTags::Combat_Event_Launch
		&& CurrentReactEventTag != ActionGameplayTags::Combat_Event_Knockdown
		&& !MovementComponent->IsFalling())
	{
		MovementComponent->StopMovementImmediately();
	}

	bCombatReactMovementRestricted = true;
}

void UActionCombatReactComponent::ClearCombatReactMovementRestriction()
{
	UCharacterMovementComponent* MovementComponent = GetOwningMovementComponent();
	if (!MovementComponent || !bCombatReactMovementRestricted)
	{
		return;
	}

	MovementComponent->MaxWalkSpeed = CachedMaxWalkSpeed;
	MovementComponent->MaxAcceleration = CachedMaxAcceleration;
	bCombatReactMovementRestricted = false;
}

bool UActionCombatReactComponent::ShouldWaitForLandingToFinishReact() const
{
	if (CurrentReactEventTag != ActionGameplayTags::Combat_Event_Launch
		&& CurrentReactEventTag != ActionGameplayTags::Combat_Event_Knockdown)
	{
		return false;
	}

	if (const UCharacterMovementComponent* MovementComponent = GetOwningMovementComponent())
	{
		return MovementComponent->IsFalling();
	}

	return false;
}

void UActionCombatReactComponent::HandleCombatReactLanded()
{
	bWaitingForLanding = false;
	SetComponentTickEnabled(false);

	if (!bCombatReactActive)
	{
		return;
	}

	if (CurrentReactEventTag == ActionGameplayTags::Combat_Event_Launch)
	{
		// 纯击飞在落地后没有额外恢复链时，落地本身就是这条受击链的最终推进点。
		FinishCombatReact(false, true);
		return;
	}

	if (CurrentReactEventTag == ActionGameplayTags::Combat_Event_Knockdown)
	{
		// 击倒不再切到单独起身恢复链，落地后继续走当前受击主链的统一收尾。
		FinishCombatReact(false, true);
	}
}

void UActionCombatReactComponent::FinishCombatReact(bool bWasInterrupted, bool bBroadcastEvent)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CombatReactTimerHandle);
	}

	// 这里是整条受击链最终收尾的唯一正式出口。
	bWaitingForLanding = false;
	bHoldReactUntilTimerExpires = false;
	PendingLandingRecoveryDuration = 0.f;
	SetComponentTickEnabled(false);

	if (UPawnCombatComponent* CombatComponent = GetOwningCombatComponent())
	{
		const FActionRunningAnimationReactGuardContext& RunningContext =
			CombatComponent->GetRunningAnimationReactGuardContext();
		if (RunningContext.Semantic == EActionRunningAnimationSemantic::CombatReact
			&& RunningContext.Montage == CurrentCombatReactMontage)
		{
			CombatComponent->ClearRunningAnimationReactGuardContext();
		}
	}

	StopCurrentCombatReactMontage();
	RemoveCombatReactStateEffect();
	ClearCombatReactMovementRestriction();
	CloseCombatReactRecoveryCancelWindow();
	const bool bControlAlreadyReleasedByUnlockFrame = bCombatReactUnlockFrameReached;

	if (const AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(GetOwningCharacter()))
	{
		if (AActionPlayerController* HeroController = Cast<AActionPlayerController>(HeroCharacter->GetController()))
		{
			// 受击链结束后，让控制器按当前移动状态重新应用正式移动配置。
			HeroController->UpdateMovementData();
		}
	}

	if (UPawnCombatComponent* CombatComponent = GetOwningCombatComponent())
	{
		CombatComponent->SetAttackEnabled(true);

		if (!bControlAlreadyReleasedByUnlockFrame)
		{
			if (const AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(GetOwningCharacter()))
			{
				// 解锁帧之前没放行过输入时，这里统一回放 held input。
				if (UHeroCombatInputComponent* HeroCombatInputComponent = HeroCharacter->GetHeroCombatInputComponent())
				{
					HeroCombatInputComponent->RequestReplayHeldInputsAfterCombatReact();
				}
			}
		}
	}

	const bool bWasActive = bCombatReactActive;
	const FGameplayTag FinishedReactEventTag = CurrentReactEventTag;

	bCombatReactActive = false;
	bHandedOffToExecutionVictim = false;
	CurrentCombatReactMontage = nullptr;
	CurrentReactDirection = EActionCombatReactDirection::None;
	CurrentReactPhase = EActionCombatReactPhase::None;
	CurrentReactEventTag = FGameplayTag();
	CurrentReactStateTag = FGameplayTag();
	CurrentReactStatusEffectTag = FGameplayTag();
	bCombatReactUnlockFrameReached = false;

	if (bWasActive && bBroadcastEvent)
	{
		K2_OnCombatReactFinished(FinishedReactEventTag, bWasInterrupted);
	}
}

void UActionCombatReactComponent::StopCurrentCombatReactMontage()
{
	UAnimInstance* OwnerAnimInstance = GetOwnerAnimInstance();
	if (!OwnerAnimInstance || !CurrentCombatReactMontage)
	{
		return;
	}

	if (!OwnerAnimInstance->Montage_IsPlaying(CurrentCombatReactMontage))
	{
		return;
	}

	FOnMontageEnded EmptyMontageEndedDelegate;
	OwnerAnimInstance->Montage_SetEndDelegate(EmptyMontageEndedDelegate, CurrentCombatReactMontage);
	OwnerAnimInstance->Montage_Stop(0.1f, CurrentCombatReactMontage);
}

UAnimInstance* UActionCombatReactComponent::GetOwnerAnimInstance() const
{
	const AActionCharacterBase* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter || !OwnerCharacter->GetMesh())
	{
		return nullptr;
	}

	return OwnerCharacter->GetMesh()->GetAnimInstance();
}

AActionCharacterBase* UActionCombatReactComponent::GetOwningCharacter() const
{
	return Cast<AActionCharacterBase>(GetOwner());
}

UPawnCombatComponent* UActionCombatReactComponent::GetOwningCombatComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UPawnCombatComponent>() : nullptr;
}

UActionAbilitySystemComponent* UActionCombatReactComponent::GetOwningActionAbilitySystemComponent() const
{
	const AActionCharacterBase* OwnerCharacter = GetOwningCharacter();
	return OwnerCharacter ? OwnerCharacter->GetActionAbilitySystemComponent() : nullptr;
}

UCharacterMovementComponent* UActionCombatReactComponent::GetOwningMovementComponent() const
{
	const AActionCharacterBase* OwnerCharacter = GetOwningCharacter();
	return OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
}

bool UActionCombatReactComponent::HasOwningGameplayTag(const FGameplayTag& TagToCheck) const
{
	if (!TagToCheck.IsValid())
	{
		return false;
	}

	if (const UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent())
	{
		return ActionAbilitySystemComponent->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

float UActionCombatReactComponent::ResolveCombatReactDuration(
	UAnimMontage* ReactMontage,
	const FActionDamagePayload& DamagePayload) const
{
	if (DamagePayload.HitStateDuration > 0.f)
	{
		return DamagePayload.HitStateDuration;
	}

	return ReactMontage ? ReactMontage->GetPlayLength() : 0.f;
}

float UActionCombatReactComponent::ResolveCombatReactEffectDuration(const FActionDamagePayload& DamagePayload) const
{
	const float ReactStageDuration = ResolveCombatReactDuration(CurrentCombatReactMontage, DamagePayload);
	const float ReactMontageDuration = CurrentCombatReactMontage ? CurrentCombatReactMontage->GetPlayLength() : 0.f;
	float TotalEffectDuration = FMath::Max(ReactStageDuration, ReactMontageDuration);

	if (CurrentReactEventTag == ActionGameplayTags::Combat_Event_Launch)
	{
		// 击飞链额外保留一小段尾巴，覆盖落地后状态收尾的过渡区间。
		return TotalEffectDuration + 3.f;
	}

	return TotalEffectDuration;
}

void UActionCombatReactComponent::OnCombatReactExpired()
{
	if (HasExecutionExclusiveLock(this))
	{
		// 处决锁还在时，普通受击链不能先收尾；这里短延迟重试，让处决链先完成最终裁决。
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				CombatReactTimerHandle,
				this,
				&ThisClass::OnCombatReactExpired,
				ExecutionLockedCombatReactRetryDelay,
				false);
		}

		return;
	}

	if (CurrentReactEventTag == ActionGameplayTags::Combat_Event_PoiseBreak
		&& bCombatReactUnlockFrameReached)
	{
		const UExecutionWindowComponent* ExecutionWindowComponent =
			GetOwner() ? GetOwner()->FindComponentByClass<UExecutionWindowComponent>() : nullptr;
		UE_LOG(
			ActionRPG,
			Log,
			TEXT("poise_break_react_diagnostic_window_retained_after_unlock owner=%s current_montage=%s execution_window_open=%s combat_react_expired=true"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(CurrentCombatReactMontage),
			(ExecutionWindowComponent && ExecutionWindowComponent->IsExecutionWindowOpen()) ? TEXT("true") : TEXT("false"));
	}

	if (ShouldCloseUnconsumedExecutionWindowAfterPoiseBreak(this, CurrentReactEventTag))
	{
		// 走进这里代表这次 PoiseBreak 表现还停留在 unlock 之前就收尾了。
		// 因此本次“可处决”机会视为没有真正落地，需要立刻关窗并恢复 Poise，
		// 避免目标在没有完成破韧表现的情况下长期停留在可处决状态。
		const bool bExpectedUnlockNotify =
			CurrentCombatReactMontage && HasCombatReactUnlockNotify(CurrentCombatReactMontage);
		if (bExpectedUnlockNotify && !bCombatReactUnlockFrameReached)
		{
			const float ReactMontageLength =
				CurrentCombatReactMontage ? CurrentCombatReactMontage->GetPlayLength() : 0.f;
			UE_LOG(
				ActionRPG,
				Warning,
				TEXT("poise_break_react_diagnostic_unlock_notify_missing_before_expire owner=%s current_montage=%s montage_length=%.3f unlock_time=%.3f"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(CurrentCombatReactMontage),
				ReactMontageLength,
				ResolveCombatReactUnlockNotifyTriggerTime(CurrentCombatReactMontage));
		}

		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("poise_break_react_diagnostic_expired_close_window owner=%s current_montage=%s waiting_for_landing=%s hold_until_timer=%s unlock_frame_reached=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(CurrentCombatReactMontage),
			bWaitingForLanding ? TEXT("true") : TEXT("false"),
			bHoldReactUntilTimerExpires ? TEXT("true") : TEXT("false"),
			bCombatReactUnlockFrameReached ? TEXT("true") : TEXT("false"));
		CloseUnconsumedExecutionWindowAfterPoiseBreak(this);
		FinishCombatReact(false, true);
		return;
	}

	// 走到这里说明当前受击链没有被处决挂起，可以直接正式收尾。
	FinishCombatReact(false, true);
}

void UActionCombatReactComponent::HandleCombatReactMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != CurrentCombatReactMontage)
	{
		// 只处理当前正式受击链正在使用的那段蒙太奇。
		return;
	}

	if (HasExecutionExclusiveLock(this) && !bInterrupted)
	{
		// 蒙太奇播完但处决锁还在时，正式收尾权仍然属于处决主链。
		// 这里先把受击表现转成“等待状态到时重试收尾”，避免普通受击先把 victim 运行态裁掉。
		CurrentCombatReactMontage = nullptr;
		bHoldReactUntilTimerExpires = true;

		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				CombatReactTimerHandle,
				this,
				&ThisClass::OnCombatReactExpired,
				ExecutionLockedCombatReactRetryDelay,
				false);
		}

		return;
	}

	if (CurrentReactEventTag == ActionGameplayTags::Combat_Event_PoiseBreak
		&& bCombatReactUnlockFrameReached
		&& !bInterrupted)
	{
		UExecutionWindowComponent* ExecutionWindowComponent =
			GetOwner() ? GetOwner()->FindComponentByClass<UExecutionWindowComponent>() : nullptr;
		if (ExecutionWindowComponent)
		{
			// unlock frame 已到且本次破韧表现正常播完时，若还没进入正式处决消费链，
			// 这里要补一条“只恢复韧性、不关窗口”的正式桥接，避免窗口保留但韧性一直不回满。
			ExecutionWindowComponent->RestorePoiseAfterPoiseBreakPresentationEnd();
		}

		UE_LOG(
			ActionRPG,
			Log,
			TEXT("poise_break_react_diagnostic_window_retained_after_unlock owner=%s montage=%s execution_window_open=%s poise_restore_requested=%s combat_react_montage_ended=true"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Montage),
			(ExecutionWindowComponent && ExecutionWindowComponent->IsExecutionWindowOpen()) ? TEXT("true") : TEXT("false"),
			ExecutionWindowComponent ? TEXT("true") : TEXT("false"));
	}

	if (ShouldCloseUnconsumedExecutionWindowAfterPoiseBreak(this, CurrentReactEventTag) && !bInterrupted)
	{
		// 蒙太奇正常结束但仍未到 unlock frame，语义上仍然属于“破韧表现失败”。
		// 这时必须立即撤销未被消费的处决窗口，不能把 unlock 前失败误留给 ExecutionWindow 持有。
		const bool bExpectedUnlockNotify = Montage && HasCombatReactUnlockNotify(Montage);
		if (bExpectedUnlockNotify && !bCombatReactUnlockFrameReached)
		{
			UE_LOG(
				ActionRPG,
				Warning,
				TEXT("poise_break_react_diagnostic_unlock_notify_missing_at_montage_end owner=%s montage=%s unlock_time=%.3f"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Montage),
				ResolveCombatReactUnlockNotifyTriggerTime(Montage));
		}

		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("poise_break_react_diagnostic_montage_end_close_window owner=%s montage=%s unlock_frame_reached=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Montage),
			bCombatReactUnlockFrameReached ? TEXT("true") : TEXT("false"));
		CloseUnconsumedExecutionWindowAfterPoiseBreak(this);
		FinishCombatReact(false, true);
		return;
	}

	if ((bWaitingForLanding || bHoldReactUntilTimerExpires) && !bInterrupted)
	{
		// 还在等落地或等延后计时器时，正式收尾权继续留给对应回调。
		return;
	}

	FinishCombatReact(bInterrupted, true);
}
