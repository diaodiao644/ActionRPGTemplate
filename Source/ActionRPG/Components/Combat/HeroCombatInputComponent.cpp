// 英雄战斗输入运行态组件实现。

#include "Components/Combat/HeroCombatInputComponent.h"

#include "ActionGameplayTags.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "Engine/World.h"

UHeroCombatInputComponent::UHeroCombatInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHeroCombatInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRuntimeStateForHeroStartup();
	Super::EndPlay(EndPlayReason);
}

void UHeroCombatInputComponent::HandleCombatInputPressed(const FGameplayTag& InputTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !CombatComponent->CanProcessCombatInput())
	{
		return;
	}

	InputRuntimeState.BeginPressedInputByTag(InputTag);
	CombatComponent->HandlePressedInputState(InputTag);
}

void UHeroCombatInputComponent::HandleCombatInputHeld(const FGameplayTag& InputTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !CombatComponent->CanProcessCombatInput() || !GetWorld())
	{
		return;
	}

	if (InputRuntimeState.GetInputButtonStateByTag(InputTag) == EActionInputButtonState::Held)
	{
		return;
	}

	const float PressedTime = InputRuntimeState.AccumulatePressedTimeByTag(InputTag, GetWorld()->GetDeltaSeconds());
	if (PressedTime < CombatComponent->ShortPressThreshold)
	{
		return;
	}

	InputRuntimeState.MarkInputAsHeldByTag(InputTag);
	CombatComponent->HandleHeldInputState(InputTag);
}

void UHeroCombatInputComponent::HandleCombatInputReleased(const FGameplayTag& InputTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !CombatComponent->CanProcessCombatInput())
	{
		return;
	}

	bool bSkipReleasedDispatch = false;
	if (UHeroAttackComponent* AttackComponent = CombatComponent->GetOwningHeroAttackComponent())
	{
		bSkipReleasedDispatch = AttackComponent->HandleAttackInputReleased(InputTag);
	}

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense)
	{
		FActionBufferedInput BufferedInputSnapshot;
		if (PeekBufferedInputSnapshot(BufferedInputSnapshot)
			&& BufferedInputSnapshot.InputTag == InputTag)
		{
			ClearBufferedInput();
		}
	}

	if (!bSkipReleasedDispatch)
	{
		CombatComponent->HandleReleasedInputState(InputTag);
	}

	InputRuntimeState.ClearPressedTimeByTag(InputTag);
	InputRuntimeState.RemoveInputStateByTag(InputTag);
}

void UHeroCombatInputComponent::QueueBufferedInput(
	const FGameplayTag& InputTag,
	const EActionInputEvent InputEvent,
	const FGameplayTag& ResolvedAttackRequestTag,
	const int32 BufferedInputOrder)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !InputTag.IsValid() || !GetWorld())
	{
		return;
	}

	const int32 EffectiveBufferedInputOrder = BufferedInputOrder > 0
		? BufferedInputOrder
		: GenerateNextCombatIntentOrder();

	InputRuntimeState.SetBufferedInput(
		InputTag,
		InputEvent,
		GetWorld()->GetTimeSeconds(),
		GetWorld()->GetTimeSeconds() + CombatComponent->InputBufferDuration,
		ResolvedAttackRequestTag,
		EffectiveBufferedInputOrder);

	GetWorld()->GetTimerManager().ClearTimer(InputRuntimeState.BufferedInputTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		InputRuntimeState.BufferedInputTimerHandle,
		this,
		&ThisClass::OnBufferedInputExpired,
		CombatComponent->InputBufferDuration,
		false);

	CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_InputBuffered);
}

void UHeroCombatInputComponent::ClearBufferedInput()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(InputRuntimeState.BufferedInputTimerHandle);
	}

	InputRuntimeState.ClearBufferedInput();
}

void UHeroCombatInputComponent::ClearBufferedInputIfMatchesTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid() || !GetWorld())
	{
		return;
	}

	FActionBufferedInput BufferedInputSnapshot;
	if (!InputRuntimeState.PeekBufferedInput(GetWorld()->GetTimeSeconds(), BufferedInputSnapshot)
		|| BufferedInputSnapshot.InputTag != InputTag)
	{
		return;
	}

	ClearBufferedInput();
}

void UHeroCombatInputComponent::ClearBufferedWeaponSwitchInputIfAny()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !GetWorld())
	{
		return;
	}

	FActionBufferedInput BufferedInputSnapshot;
	if (!InputRuntimeState.PeekBufferedInput(GetWorld()->GetTimeSeconds(), BufferedInputSnapshot))
	{
		return;
	}

	if (CombatComponent->IsWeaponSwitchInputTag(BufferedInputSnapshot.InputTag))
	{
		ClearBufferedInput();
	}
}

bool UHeroCombatInputComponent::PeekBufferedInputSnapshot(FActionBufferedInput& OutBufferedInput) const
{
	return GetWorld() && InputRuntimeState.PeekBufferedInput(GetWorld()->GetTimeSeconds(), OutBufferedInput);
}

bool UHeroCombatInputComponent::ConsumeBufferedInputSnapshot(FActionBufferedInput& OutBufferedInput)
{
	if (!GetWorld())
	{
		return false;
	}

	if (!InputRuntimeState.ConsumeBufferedInput(GetWorld()->GetTimeSeconds(), OutBufferedInput))
	{
		return false;
	}

	GetWorld()->GetTimerManager().ClearTimer(InputRuntimeState.BufferedInputTimerHandle);
	return true;
}

bool UHeroCombatInputComponent::ConsumeBufferedInput()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !GetWorld())
	{
		return false;
	}

	FActionBufferedInput LocalBufferedInput;
	if (!InputRuntimeState.PeekBufferedInput(GetWorld()->GetTimeSeconds(), LocalBufferedInput))
	{
		return false;
	}

	if (CombatComponent->IsAttackInputTag(LocalBufferedInput.InputTag))
	{
		if (UHeroAttackComponent* AttackComponent = CombatComponent->GetOwningHeroAttackComponent())
		{
			return AttackComponent->TryConsumeBufferedAttackInput();
		}

		return false;
	}

	if (LocalBufferedInput.InputTag == ActionGameplayTags::InputTag_GameplayAbility_Execution)
	{
		ClearBufferedInput();
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[BufferedInput] Execution buffered replay discarded because execution is immediate-only. Input=%s Event=%d"),
			*LocalBufferedInput.InputTag.ToString(),
			static_cast<int32>(LocalBufferedInput.TriggerEvent));
		return false;
	}

	if (!CombatComponent->CanConsumeBufferedInputNow(LocalBufferedInput))
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("%s"),
			*CombatComponent->DescribeBufferedNonAttackReplayGateForDebug(LocalBufferedInput));
		return false;
	}

	if (!ConsumeBufferedInputSnapshot(LocalBufferedInput))
	{
		return false;
	}

	CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_InputBuffer_Consumed);
	const bool bProcessed = CombatComponent->ProcessAbilityInput(
		LocalBufferedInput.InputTag,
		LocalBufferedInput.TriggerEvent,
		false,
		LocalBufferedInput.ResolvedAttackRequestTag);
	if (!bProcessed)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[BufferedInput] Non-attack buffered replay reached formal activation but did not start. Input=%s Reason=check ASC relationship diagnostics or ability runtime precheck logs."),
			*LocalBufferedInput.InputTag.ToString());
	}

	return bProcessed;
}

bool UHeroCombatInputComponent::HasValidBufferedInput() const
{
	return GetWorld() && InputRuntimeState.HasValidBufferedInput(GetWorld()->GetTimeSeconds());
}

void UHeroCombatInputComponent::ClearDeferredCombatInputRecoveryRequests()
{
	bDeferredBufferedInputConsumeRequested = false;
	bDeferredReplayHeldInputsAfterCombatReactRequested = false;

	if (UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (UHeroAttackComponent* AttackComponent = CombatComponent->GetOwningHeroAttackComponent())
		{
			AttackComponent->ClearDeferredAttackRequests();
		}
	}
}

void UHeroCombatInputComponent::RequestConsumeBufferedInputOnNextTick()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !GetWorld())
	{
		return;
	}

	FActionBufferedInput BufferedInputSnapshot;
	if (InputRuntimeState.PeekBufferedInput(GetWorld()->GetTimeSeconds(), BufferedInputSnapshot)
		&& CombatComponent->IsAttackInputTag(BufferedInputSnapshot.InputTag))
	{
		if (UHeroAttackComponent* AttackComponent = CombatComponent->GetOwningHeroAttackComponent())
		{
			AttackComponent->RequestConsumeBufferedAttackInputOnNextTick();
		}
		return;
	}

	if (bDeferredBufferedInputConsumeRequested)
	{
		return;
	}

	bDeferredBufferedInputConsumeRequested = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleDeferredBufferedInputConsume);
}

void UHeroCombatInputComponent::RequestReplayHeldInputsAfterCombatReact()
{
	if (bDeferredReplayHeldInputsAfterCombatReactRequested || !GetWorld())
	{
		return;
	}

	bDeferredReplayHeldInputsAfterCombatReactRequested = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleDeferredReplayHeldInputsAfterCombatReact);
}

bool UHeroCombatInputComponent::RecoverCombatInputAfterCombatReact()
{
	ClearDeferredCombatInputRecoveryRequests();
	if (ConsumeBufferedInput())
	{
		return true;
	}

	ReplayHeldInputsAfterCombatReact();
	return false;
}

bool UHeroCombatInputComponent::RecoverCombatInputAfterWeaponSwitch()
{
	ClearDeferredCombatInputRecoveryRequests();
	if (ConsumeBufferedInput())
	{
		return true;
	}

	ReplayHeldInputsAfterWeaponSwitch();
	return false;
}

int32 UHeroCombatInputComponent::GenerateNextCombatIntentOrder()
{
	if (NextCombatIntentOrder <= 0)
	{
		NextCombatIntentOrder = 1;
	}

	return NextCombatIntentOrder++;
}

bool UHeroCombatInputComponent::IsCombatInputPressedOrHeld(const FGameplayTag& InputTag) const
{
	const EActionInputButtonState InputState = InputRuntimeState.GetInputButtonStateByTag(InputTag);
	return InputState == EActionInputButtonState::Pressed
		|| InputState == EActionInputButtonState::Held;
}

EActionInputButtonState UHeroCombatInputComponent::GetInputButtonStateByTag(const FGameplayTag& InputTag) const
{
	return InputRuntimeState.GetInputButtonStateByTag(InputTag);
}

bool UHeroCombatInputComponent::TryGetInputRuntimeStateEntry(
	const FGameplayTag& InputTag,
	FActionInputRuntimeStateEntry& OutInputState) const
{
	if (const FActionInputRuntimeStateEntry* InputState = InputRuntimeState.InputRuntimeStateEntries.Find(InputTag))
	{
		OutInputState = *InputState;
		return true;
	}

	return false;
}

void UHeroCombatInputComponent::MarkInputAsHeldByTag(const FGameplayTag& InputTag)
{
	InputRuntimeState.MarkInputAsHeldByTag(InputTag);
}

void UHeroCombatInputComponent::MarkInputConsumedByTag(const FGameplayTag& InputTag)
{
	InputRuntimeState.MarkInputConsumedByTag(InputTag);
}

bool UHeroCombatInputComponent::IsInputConsumedByTag(const FGameplayTag& InputTag) const
{
	return InputRuntimeState.IsInputConsumedByTag(InputTag);
}

void UHeroCombatInputComponent::SetLatchedAttackRequestTagByInputTag(
	const FGameplayTag& InputTag,
	const FGameplayTag& InAttackRequestTag)
{
	InputRuntimeState.SetLatchedAttackRequestTagByInputTag(InputTag, InAttackRequestTag);
}

FGameplayTag UHeroCombatInputComponent::GetLatchedAttackRequestTagByInputTag(const FGameplayTag& InputTag) const
{
	return InputRuntimeState.GetLatchedAttackRequestTagByInputTag(InputTag);
}

void UHeroCombatInputComponent::ClearInputStateByTag(const FGameplayTag& InputTag)
{
	InputRuntimeState.ClearPressedTimeByTag(InputTag);
	InputRuntimeState.RemoveInputStateByTag(InputTag);
}

void UHeroCombatInputComponent::ResetCombatInputRecoveryRuntime()
{
	ClearBufferedInput();
	bDeferredBufferedInputConsumeRequested = false;
	bDeferredReplayHeldInputsAfterCombatReactRequested = false;
}

void UHeroCombatInputComponent::ResetRuntimeStateForHeroStartup()
{
	ResetCombatInputRecoveryRuntime();
	InputRuntimeState.InputRuntimeStateEntries.Reset();
	NextCombatIntentOrder = 1;
}

void UHeroCombatInputComponent::OnBufferedInputExpired()
{
	ClearBufferedInput();

	if (UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (UHeroWeaponSwitchComponent* WeaponSwitchComponent = CombatComponent->GetOwningHeroWeaponSwitchComponent())
		{
			WeaponSwitchComponent->HandleBufferedInputExpired();
		}
	}
}

void UHeroCombatInputComponent::HandleDeferredBufferedInputConsume()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !bDeferredBufferedInputConsumeRequested)
	{
		return;
	}

	bDeferredBufferedInputConsumeRequested = false;

	if (ConsumeBufferedInput())
	{
		return;
	}

	if (!GetWorld())
	{
		return;
	}

	FActionBufferedInput BufferedInputSnapshot;
	if (!InputRuntimeState.PeekBufferedInput(GetWorld()->GetTimeSeconds(), BufferedInputSnapshot))
	{
		return;
	}

	if (CombatComponent->IsWeaponSwitchInputTag(BufferedInputSnapshot.InputTag)
		&& CombatComponent->IsWeaponSwitchBlockedByCooldown())
	{
		RequestConsumeBufferedInputOnNextTick();
	}
}

void UHeroCombatInputComponent::HandleDeferredReplayHeldInputsAfterCombatReact()
{
	if (!bDeferredReplayHeldInputsAfterCombatReactRequested)
	{
		return;
	}

	RecoverCombatInputAfterCombatReact();
}

void UHeroCombatInputComponent::ReplayHeldInputsAfterCombatReact()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !CombatComponent->CanProcessCombatInput())
	{
		return;
	}

	// Execution 当前只认“本次 Pressed + 当前可处决”。
	// 它不进入 Held 回放优先级，也不在恢复帧里自动补起手。
	const TArray<FGameplayTag> HeldReplayPriority =
	{
		ActionGameplayTags::InputTag_GameplayAbility_Dodge,
		ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense,
		ActionGameplayTags::InputTag_GameplayAbility_Attack
	};
	ReplayHeldInputsByPriority(HeldReplayPriority);
}

void UHeroCombatInputComponent::ReplayHeldInputsAfterWeaponSwitch()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !CombatComponent->CanProcessCombatInput())
	{
		return;
	}

	// Execution 不参与切武收尾后的 Held 回放；处决机会只由当前按下瞬间决定。
	const TArray<FGameplayTag> HeldReplayPriority =
	{
		ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense,
		ActionGameplayTags::InputTag_GameplayAbility_Attack
	};
	ReplayHeldInputsByPriority(HeldReplayPriority);
}

void UHeroCombatInputComponent::ReplayHeldInputsByPriority(const TArray<FGameplayTag>& HeldReplayPriority)
{
	for (const FGameplayTag& HeldInputTag : HeldReplayPriority)
	{
		if (TryReplayHeldInputByPriorityTag(HeldInputTag))
		{
			return;
		}
	}
}

bool UHeroCombatInputComponent::TryReplayHeldInputByPriorityTag(const FGameplayTag& HeldInputTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return false;
	}

	if (HeldInputTag == ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		if (UHeroAttackComponent* AttackComponent = CombatComponent->GetOwningHeroAttackComponent())
		{
			return AttackComponent->TryReplayHeldAttackInputAfterRecovery();
		}

		return false;
	}

	const FActionInputRuntimeStateEntry* InputState = InputRuntimeState.InputRuntimeStateEntries.Find(HeldInputTag);
	if (!InputState
		|| InputState->ButtonState != EActionInputButtonState::Held
		|| InputState->bIsConsumed)
	{
		return false;
	}

	if (HeldInputTag != ActionGameplayTags::InputTag_GameplayAbility_Dodge
		&& HeldInputTag != ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense)
	{
		return false;
	}

	CombatComponent->HandleHeldInputState(HeldInputTag);
	return true;
}

AActionHeroCharacter* UHeroCombatInputComponent::GetOwningHeroCharacter() const
{
	if (CachedHeroCharacter.IsValid())
	{
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedHeroCharacter.Get();
}

UHeroCombatComponent* UHeroCombatInputComponent::GetOwningHeroCombatComponent() const
{
	if (CachedHeroCombatComponent.IsValid())
	{
		return CachedHeroCombatComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroCombatComponent = OwnerCharacter->GetHeroCombatComponent();
	}

	return CachedHeroCombatComponent.Get();
}
