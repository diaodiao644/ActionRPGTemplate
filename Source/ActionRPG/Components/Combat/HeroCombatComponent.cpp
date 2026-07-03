
#include "Components/Combat/HeroCombatComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "AnimInstance/Hero/ActionHeroLinkedAnimLayer.h"
#include "Components/Input/ActionEnhancedInputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/ActionHitResolver.h"
#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Camera/HeroCameraComponent.h"
#include "Components/Character/HeroAssemblyComponent.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroExecutionCoordinatorComponent.h"
#include "Components/Combat/HeroHitSourceComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutStateComponent.h"
#include "Components/Execution/ExecutionWindowComponent.h"
#include "Debug/ActionDebugHelper.h"
#include "Engine/World.h"
#include "GameBase/ActionPlayerController.h"
#include "Items/Weapons/HeroWeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroCombatComponent, Log, All);

namespace HeroCombatTagRouting
{
	static FGameplayTag ResolveTagByLoadoutSlot(
		const EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FGameplayTag& UnarmedTag,
		const FGameplayTag& MeleeTag,
		const FGameplayTag& RangedTag,
		const FGameplayTag& HybridTag)
{
		switch (InLoadoutSlot)
		{
		case EHeroWeaponLoadoutSlot::Unarmed:
			return UnarmedTag;

		case EHeroWeaponLoadoutSlot::MeleeWeapon:
			return MeleeTag;

		case EHeroWeaponLoadoutSlot::RangedWeapon:
			return RangedTag;

		case EHeroWeaponLoadoutSlot::HybridWeapon:
			return HybridTag;

		default:
			break;
		}

		return FGameplayTag();
	}
}

namespace
{
	FString HeroCombatBoolToDebugText(const bool bValue)
	{
        return bValue ? TEXT("yes") : TEXT("no");
	}

	bool IsExpectedSpiritActivationGateBlock(const FString& InGateText)
	{
		return InGateText.Contains(TEXT("layer=WeaponSwitchPresentation"))
			|| InGateText.Contains(TEXT("layer=WeaponSwitchChainWindow"));
	}

	FString HeroCombatInputTagToDebugText(const FGameplayTag& InputTag)
	{
		return InputTag.IsValid() ? InputTag.ToString() : TEXT("Invalid");
	}

	FString HeroCombatStartupStateToDebugText(const EHeroWeaponLoadoutStartupState StartupState)
	{
		switch (StartupState)
		{
		case EHeroWeaponLoadoutStartupState::Ready:
			return TEXT("Ready");

		case EHeroWeaponLoadoutStartupState::InProgress:
			return TEXT("InProgress");

		case EHeroWeaponLoadoutStartupState::Failed:
			return TEXT("Failed");

		default:
			break;
		}

		return TEXT("None");
	}
}

UHeroCombatComponent::UHeroCombatComponent()
	: Super()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bLastWeaponSwitchCooldownBlocked = false;
}

void UHeroCombatComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateCombatModeIdleExitTransition(DeltaTime);
	UpdateWeaponSwitchCooldownUIState();
}


void UHeroCombatComponent::InitializeWeaponLoadout(const TArray<FHeroWeaponLoadoutDefinition>& InWeaponLoadoutDefinitions)
{
	if (UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		EquipmentComponent->InitializeWeaponLoadout(InWeaponLoadoutDefinitions);

		if (UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
		{
			WeaponSwitchComponent->InitializeCurrentWeaponStateFromEquipment();
		}
	}
}

bool UHeroCombatComponent::EquipWeaponByLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	if (UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		return WeaponSwitchComponent->EquipWeaponByLoadoutSlot(InLoadoutSlot);
	}

	return false;
}

AHeroWeaponBase* UHeroCombatComponent::GetCurrentEquippedWeapon() const
{
	EnsureCurrentWeaponStateSynchronizedFromEquipment();
	return CurrentEquippedWeaponInstance;
}

UDataAsset_WeaponDefinition* UHeroCombatComponent::GetCurrentWeaponDefinition() const
{
	EnsureCurrentWeaponStateSynchronizedFromEquipment();
	return CurrentEquippedWeaponDefinition;
}

bool UHeroCombatComponent::HasEquippedWeaponLinkedLayerPresentation() const
{
	EnsureCurrentWeaponStateSynchronizedFromEquipment();
	return bHasEquippedWeaponLinkedLayerPresentation;
}

bool UHeroCombatComponent::ShouldCurrentEquippedWeaponUseWeaponSocketPresentation() const
{
	EnsureCurrentWeaponStateSynchronizedFromEquipment();
	return bCurrentEquippedWeaponShouldUseWeaponSocketPresentation;
}

TSubclassOf<UActionHeroLinkedAnimLayer> UHeroCombatComponent::GetCurrentWeaponLinkedAnimLayerClass() const
{
	EnsureCurrentWeaponStateSynchronizedFromEquipment();
	return CurrentWeaponLinkedAnimLayerClass;
}

bool UHeroCombatComponent::TryEnterCombatModeFromIdle()
{
	if (GetCombatMode() != EHeroCombatMode::Idle)
	{
		return false;
	}

	// 这里先正式写入 CombatMode=Combo，再把“武器何时从 Holster 切到 WeaponSocket”
	// 交给 Enter Montage 上的 CombatWeaponPresentationSwitch 或结束兜底处理。
	// 这样 Combat 语义和精确挂点时机可以并行存在，但不会被强行绑成同一帧。
	// 正式进入 Combat 表现态时，先写入 CombatMode，再由 Enter Montage 或结束兜底决定何时切手持挂点。
	UAnimMontage* EnterCombatModeMontage = GetCombatModeTransitionAnimMontage();
	const bool bStartedTransition = TryPlayCombatModeTransitionMontage(EnterCombatModeMontage, false);
	SetCombatMode(EHeroCombatMode::Combo);
	if (!bStartedTransition)
	{
		RequestCurrentEquippedWeaponPresentationToCombatSocket();
	}

	return true;
}

void UHeroCombatComponent::EnterComboModeImmediatelyForActivePresentation()
{
	if (GetCombatMode() == EHeroCombatMode::Combo)
	{
		return;
	}

	SetCombatMode(EHeroCombatMode::Combo);
}

void UHeroCombatComponent::NotifyCombatWeaponPresentationSwitchFrame(const bool bAttachToWeaponSocket)
{
	// Notify 只负责精确时机：它告诉战斗组件“当前已装备武器这一帧应挂在哪个 socket”。
	// CombatMode 是否已经进入 Combo / Idle、移动是否应锁定，都不在这里裁决。
	bCombatModeTransitionNotifyReceived = true;
	if (bAttachToWeaponSocket)
	{
		RequestCurrentEquippedWeaponPresentationToCombatSocket();
		return;
	}

	RequestCurrentEquippedWeaponPresentationToHolster();
}

void UHeroCombatComponent::FinalizeWeaponSwitchRuntimeState()
{
	RepairStaleCombatRuntimeIfNeeded();
	if (CurrentEquippedWeaponCategory != EHeroWeaponCategory::Unarmed)
	{
		SetCombatMode(EHeroCombatMode::Combo);
	}

	UE_LOG(
		LogHeroCombatComponent,
		Log,
		TEXT("weapon_switch_runtime_finalized weapon_tag=%s weapon_category=%d combat_mode=%d should_use_weapon_layer=%s linked_layer=%s running_montage=%s weapon_socket_presentation=%s"),
		*GetCurrentEquippedWeaponTag().ToString(),
		static_cast<int32>(CurrentEquippedWeaponCategory),
		static_cast<int32>(GetCombatMode()),
		bHasEquippedWeaponLinkedLayerPresentation ? TEXT("true") : TEXT("false"),
		*GetNameSafe(CurrentWeaponLinkedAnimLayerClass),
		*GetNameSafe(GetCurrentRunningAnimMontage()),
		bCurrentEquippedWeaponShouldUseWeaponSocketPresentation ? TEXT("true") : TEXT("false"));

	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		if (UHeroCameraComponent* HeroCameraComponent = OwnerHeroCharacter->GetHeroCameraComponent())
		{
			HeroCameraComponent->ForceRefreshCameraMode();
		}
	}
}

void UHeroCombatComponent::SetCombatMode(const EHeroCombatMode InCombatMode)
{
	const EHeroCombatMode PreviousCombatMode = GetCombatMode();
	if (PreviousCombatMode == InCombatMode)
	{
		return;
	}

	if (bCombatModeTransitionActive)
	{
		const bool bEnteringTransitionInterruptedByIdle = !bCombatModeTransitionTargetIdle && InCombatMode == EHeroCombatMode::Idle;
		const bool bExitTransitionInterruptedByCombat = bCombatModeTransitionTargetIdle && InCombatMode != EHeroCombatMode::Idle;
		if (bEnteringTransitionInterruptedByIdle || bExitTransitionInterruptedByCombat)
		{
			StopActiveCombatModeTransitionMontage(InCombatMode != EHeroCombatMode::Idle);
		}
	}

	Super::SetCombatMode(InCombatMode);
	if (InCombatMode == EHeroCombatMode::Idle)
	{
		CurrentCombatIdleElapsedSeconds = 0.f;
		if (!bCombatModeTransitionActive || !bCombatModeTransitionTargetIdle)
		{
			RequestCurrentEquippedWeaponPresentationToHolster();
		}
	}
	else
	{
		CurrentCombatIdleElapsedSeconds = 0.f;
		if (!bCombatModeTransitionActive || bCombatModeTransitionTargetIdle)
		{
			RequestCurrentEquippedWeaponPresentationToCombatSocket();
		}
	}
}

UAnimInstance* UHeroCombatComponent::GetOwnerAnimInstance() const
{
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !OwnerHeroCharacter->GetMesh())
	{
		return nullptr;
	}

	return OwnerHeroCharacter->GetMesh()->GetAnimInstance();
}

void UHeroCombatComponent::HandleCombatModeTransitionMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	FinishCombatModeTransitionMontage(Montage, bInterrupted);
}

bool UHeroCombatComponent::IsWeaponSwitchPresentationActive() const
{
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	return WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchPresentationActive();
}

bool UHeroCombatComponent::IsSpecialWeaponSwitchPresentationActive() const
{
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	return WeaponSwitchComponent && WeaponSwitchComponent->IsSpecialWeaponSwitchPresentationActive();
}

bool UHeroCombatComponent::IsCombatModeTransitionPresentationActive() const
{
	return HasActiveCombatModeTransitionRuntime();
}

void UHeroCombatComponent::BeginActiveDamageContext(
	const int32 InAbilityLevel,
	const FGameplayTag& InSourceAbilityTag)
{
	DamageContextRuntimeState.bActive = true;
	DamageContextRuntimeState.AbilityLevel = FMath::Max(InAbilityLevel, 1);
	DamageContextRuntimeState.SourceAbilityTag = InSourceAbilityTag;
}

void UHeroCombatComponent::ClearActiveDamageContext()
{
	DamageContextRuntimeState.Reset();
}

bool UHeroCombatComponent::TryGetActiveDamageContext(
	FActionDamageContextRuntimeState& OutDamageContext) const
{
	OutDamageContext = DamageContextRuntimeState;
	return DamageContextRuntimeState.HasActiveContext();
}

// 姝﹀櫒鏁版嵁椹卞姩鏌ヨ

UAnimMontage* UHeroCombatComponent::GetCurrentRunningAnimMontage() const
{
	return CurrentRunningAnimMontage;
}

UAnimMontage* UHeroCombatComponent::GetCombatModeTransitionAnimMontage()
{
	if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition())
	{
		return CurrentWeaponDefinition->GetCombatModeTransitionAnimMontage(GetCombatMode());
	}

	return nullptr;
}


AActionHeroCharacter* UHeroCombatComponent::GetOwningHeroCharacter() const
{
	if (CachedHeroCharacter.IsValid())
	{
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedHeroCharacter.Get();
}

AActionPlayerController* UHeroCombatComponent::GetOwningHeroController() const
{
	if (CachedHeroController.IsValid())
	{
		return CachedHeroController.Get();
	}

	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		CachedHeroController = Cast<AActionPlayerController>(OwnerHeroCharacter->GetController());
	}

	return CachedHeroController.Get();
}

UActionAbilitySystemComponent* UHeroCombatComponent::GetOwningActionAbilitySystemComponent() const
{
	if (CachedActionAbilitySystemComponent.IsValid())
	{
		return CachedActionAbilitySystemComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedActionAbilitySystemComponent = OwnerCharacter->GetActionAbilitySystemComponent();
	}

	return CachedActionAbilitySystemComponent.Get();
}

UHeroEquipmentComponent* UHeroCombatComponent::GetOwningHeroEquipmentComponent() const
{
	if (CachedHeroEquipmentComponent.IsValid())
	{
		return CachedHeroEquipmentComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroEquipmentComponent = OwnerCharacter->FindComponentByClass<UHeroEquipmentComponent>();
	}

	return CachedHeroEquipmentComponent.Get();
}

UHeroLoadoutStateComponent* UHeroCombatComponent::GetOwningHeroLoadoutStateComponent() const
{
	if (CachedHeroLoadoutStateComponent.IsValid())
	{
		return CachedHeroLoadoutStateComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroLoadoutStateComponent = OwnerCharacter->FindComponentByClass<UHeroLoadoutStateComponent>();
	}

	return CachedHeroLoadoutStateComponent.Get();
}

UHeroWeaponSwitchComponent* UHeroCombatComponent::GetOwningHeroWeaponSwitchComponent() const
{
	if (CachedHeroWeaponSwitchComponent.IsValid())
	{
		return CachedHeroWeaponSwitchComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroWeaponSwitchComponent = OwnerCharacter->GetHeroWeaponSwitchComponent();
	}

	return CachedHeroWeaponSwitchComponent.Get();
}

UHeroAttackComponent* UHeroCombatComponent::GetOwningHeroAttackComponent() const
{
	if (CachedHeroAttackComponent.IsValid())
	{
		return CachedHeroAttackComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroAttackComponent = OwnerCharacter->GetHeroAttackComponent();
	}

	return CachedHeroAttackComponent.Get();
}

UHeroDefenseComponent* UHeroCombatComponent::GetOwningHeroDefenseComponent() const
{
	if (CachedHeroDefenseComponent.IsValid())
	{
		return CachedHeroDefenseComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroDefenseComponent = OwnerCharacter->GetHeroDefenseComponent();
	}

	return CachedHeroDefenseComponent.Get();
}

UHeroCombatInputComponent* UHeroCombatComponent::GetOwningHeroCombatInputComponent() const
{
	if (CachedHeroCombatInputComponent.IsValid())
	{
		return CachedHeroCombatInputComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroCombatInputComponent = OwnerCharacter->GetHeroCombatInputComponent();
	}

	return CachedHeroCombatInputComponent.Get();
}

UHeroExecutionCoordinatorComponent* UHeroCombatComponent::GetOwningHeroExecutionCoordinatorComponent() const
{
	if (CachedHeroExecutionCoordinatorComponent.IsValid())
	{
		return CachedHeroExecutionCoordinatorComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroExecutionCoordinatorComponent = OwnerCharacter->GetHeroExecutionCoordinatorComponent();
	}

	return CachedHeroExecutionCoordinatorComponent.Get();
}

UHeroHitSourceComponent* UHeroCombatComponent::GetOwningHeroHitSourceComponent() const
{
	if (CachedHeroHitSourceComponent.IsValid())
	{
		return CachedHeroHitSourceComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroHitSourceComponent = OwnerCharacter->GetHeroHitSourceComponent();
	}

	return CachedHeroHitSourceComponent.Get();
}

UHeroTargetingComponent* UHeroCombatComponent::GetOwningHeroTargetingComponent() const
{
	if (CachedHeroTargetingComponent.IsValid())
	{
		return CachedHeroTargetingComponent.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedHeroTargetingComponent = OwnerCharacter->GetHeroTargetingComponent();
	}

	return CachedHeroTargetingComponent.Get();
}

UActionAttributeSetBase* UHeroCombatComponent::GetOwningActionAttributeSet() const
{
	if (CachedActionAttributeSet.IsValid())
	{
		return CachedActionAttributeSet.Get();
	}

	if (const AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		CachedActionAttributeSet = OwnerCharacter->GetActionAttributeSet();
	}

	return CachedActionAttributeSet.Get();
}


void UHeroCombatComponent::HandleCombatInputConfig()
{
	ACharacter* OwningCharacter = Cast<ACharacter>(GetOwner());
	if (!OwningCharacter)
	{
		return;
	}

	APlayerController* OwnerController = Cast<APlayerController>(OwningCharacter->GetController());
	if (!OwnerController)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwnerController->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(CombatMappingContext, 1);
	}

	UActionEnhancedInputComponent* InputCmpt = Cast<UActionEnhancedInputComponent>(OwnerController->InputComponent);
	if (!InputCmpt)
	{
		return;
	}

	InputCmpt->BindAbilityActions(CombatInputConfig, this, &UHeroCombatComponent::HandleCombatInputPressed, &UHeroCombatComponent::HandleCombatInputReleased, &UHeroCombatComponent::HandleCombatInputHeld);
}

void UHeroCombatComponent::QueueBufferedInput(
	FGameplayTag InputTag,
	EActionInputEvent InputEvent,
	FGameplayTag ResolvedAttackRequestTag,
	const int32 BufferedInputOrder)
{
	if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
	{
		InputComponent->QueueBufferedInput(InputTag, InputEvent, ResolvedAttackRequestTag, BufferedInputOrder);
	}
}

void UHeroCombatComponent::OpenAbilityChainWindow(
	const FGameplayTagContainer& InAllowedInputTags,
	const bool bConsumeBufferedInputImmediately)
{
	AbilityWindowRuntimeState.OpenAbilityChainWindow(InAllowedInputTags);

	if (bConsumeBufferedInputImmediately)
	{
		if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
		{
			if (!InputComponent->ConsumeBufferedInput())
			{
				InputComponent->RequestConsumeBufferedInputOnNextTick();
			}
		}
	}
}
void UHeroCombatComponent::CloseAbilityChainWindow()
{
	AbilityWindowRuntimeState.CloseAbilityChainWindow();
}

void UHeroCombatComponent::OpenAbilityCancelWindow(const FGameplayTagContainer& InAllowedInputTags)
{
	AbilityWindowRuntimeState.OpenAbilityCancelWindow(InAllowedInputTags);
}

void UHeroCombatComponent::CloseAbilityCancelWindow()
{
	AbilityWindowRuntimeState.CloseAbilityCancelWindow();
}

void UHeroCombatComponent::BroadcastCombatEvent(FGameplayTag EventTag) const
{
	if (!EventTag.IsValid() || !GetOwner())
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = GetOwner();
	EventData.Target = GetOwner();

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), EventTag, EventData);
}

void UHeroCombatComponent::BroadcastCombatEventIf(const bool bShouldBroadcast, const FGameplayTag EventTag) const
{
	if (!bShouldBroadcast)
	{
		return;
	}

	BroadcastCombatEvent(EventTag);
}

void UHeroCombatComponent::RequestRecoverCombatInputAfterExecution()
{
	if (UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		ExecutionComponent->RequestRecoverCombatInputAfterExecution();
	}
}

void UHeroCombatComponent::ClearCombatTimer(FTimerHandle& TimerHandle)
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
}

void UHeroCombatComponent::StartCombatTimer(
	FTimerHandle& TimerHandle,
	void (UHeroCombatComponent::*ExpireCallback)(),
	const float Duration)
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, ExpireCallback, Duration, false);
}

FHeroSpiritSkillComboRuntimeState* UHeroCombatComponent::FindMutableSpiritSkillComboRuntimeState(const FGameplayTag& SpiritInputTag)
{
	if (!ActionGameplayTags::IsSpiritSkillInputTag(SpiritInputTag))
	{
		return nullptr;
	}

	return SpiritSkillComboRuntimeStates.Find(SpiritInputTag);
}

const FHeroSpiritSkillComboRuntimeState* UHeroCombatComponent::FindSpiritSkillComboRuntimeState(const FGameplayTag& SpiritInputTag) const
{
	if (!ActionGameplayTags::IsSpiritSkillInputTag(SpiritInputTag))
	{
		return nullptr;
	}

	return SpiritSkillComboRuntimeStates.Find(SpiritInputTag);
}

void UHeroCombatComponent::ClearSpiritSkillComboTimer(FHeroSpiritSkillComboRuntimeState& RuntimeState)
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(RuntimeState.ComboChainTimeoutTimerHandle);
}

void UHeroCombatComponent::StartSpiritSkillComboTimer(
	const FGameplayTag& SpiritInputTag,
	FHeroSpiritSkillComboRuntimeState& RuntimeState)
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(RuntimeState.ComboChainTimeoutTimerHandle);

	FTimerDelegate TimeoutDelegate;
	TimeoutDelegate.BindUObject(this, &UHeroCombatComponent::HandleSpiritSkillComboTimeoutByInputTag, SpiritInputTag);
	GetWorld()->GetTimerManager().SetTimer(
		RuntimeState.ComboChainTimeoutTimerHandle,
		TimeoutDelegate,
		RuntimeState.ComboChainTimeoutSeconds,
		false);
}

void UHeroCombatComponent::ClearBufferedSpiritSkillInputByTag(const FGameplayTag& SpiritInputTag)
{
	if (!SpiritInputTag.IsValid())
	{
		return;
	}

	UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent();
	if (!InputComponent)
	{
		return;
	}

	FActionBufferedInput BufferedInputSnapshot;
	if (InputComponent->PeekBufferedInputSnapshot(BufferedInputSnapshot)
		&& BufferedInputSnapshot.InputTag == SpiritInputTag)
	{
		InputComponent->ClearBufferedInput();
	}
}

int32 UHeroCombatComponent::ResolveSpiritSkillClipIndexToPlay(
	const FGameplayTag& SpiritInputTag,
	const FActionSpiritSkillConfig& SpiritSkillConfig,
	const int32 SkillClipCount) const
{
	if (!SpiritSkillConfig.bUseComboIndex || SkillClipCount <= 0)
	{
		return 0;
	}

	const FHeroSpiritSkillComboRuntimeState* RuntimeState = FindSpiritSkillComboRuntimeState(SpiritInputTag);
	const int32 PendingClipIndex = RuntimeState ? RuntimeState->PendingClipIndex : 0;
	return FMath::Clamp(PendingClipIndex, 0, SkillClipCount - 1);
}

bool UHeroCombatComponent::HasCommittedSpiritSkillChainCost(const FGameplayTag& SpiritInputTag) const
{
	const FHeroSpiritSkillComboRuntimeState* RuntimeState = FindSpiritSkillComboRuntimeState(SpiritInputTag);
	return RuntimeState ? RuntimeState->bCostCommittedForCurrentChain : false;
}

void UHeroCombatComponent::HandleSpiritSkillClipStarted(
	const FGameplayTag& SpiritInputTag,
	const FActionSpiritSkillConfig& SpiritSkillConfig,
	const int32 StartedClipIndex,
	const int32 SkillClipCount)
{
	FHeroSpiritSkillComboRuntimeState* RuntimeState = FindMutableSpiritSkillComboRuntimeState(SpiritInputTag);
	if (!RuntimeState)
	{
		RuntimeState = &SpiritSkillComboRuntimeStates.FindOrAdd(SpiritInputTag);
	}

	ClearSpiritSkillComboTimer(*RuntimeState);
	RuntimeState->SkillClipCount = FMath::Max(SkillClipCount, 0);
	RuntimeState->bWaitingForNextClip = false;
	RuntimeState->bCostCommittedForCurrentChain = true;
	RuntimeState->ComboChainTimeoutSeconds =
		SpiritSkillConfig.bUseComboIndex
			? FMath::Max(SpiritSkillConfig.ComboChainTimeoutSeconds, 0.f)
			: 0.f;

	if (!SpiritSkillConfig.bUseComboIndex || SkillClipCount <= 0)
	{
		RuntimeState->PendingClipIndex = 0;
		return;
	}

	if (SpiritSkillConfig.bAdvanceComboIndexOnPlay && StartedClipIndex + 1 < SkillClipCount)
	{
		RuntimeState->PendingClipIndex = StartedClipIndex + 1;
		return;
	}

	RuntimeState->PendingClipIndex = FMath::Clamp(StartedClipIndex, 0, SkillClipCount - 1);
}

bool UHeroCombatComponent::BeginWaitingForNextSpiritSkillClip(const FGameplayTag& SpiritInputTag)
{
	FHeroSpiritSkillComboRuntimeState* RuntimeState = FindMutableSpiritSkillComboRuntimeState(SpiritInputTag);
	if (!RuntimeState
		|| RuntimeState->SkillClipCount <= 1
		|| RuntimeState->ComboChainTimeoutSeconds <= 0.f)
	{
		return false;
	}

	RuntimeState->bWaitingForNextClip = true;
	StartSpiritSkillComboTimer(SpiritInputTag, *RuntimeState);
	return true;
}

void UHeroCombatComponent::ResetSpiritSkillComboState(const FGameplayTag& SpiritInputTag)
{
	FHeroSpiritSkillComboRuntimeState* RuntimeState = FindMutableSpiritSkillComboRuntimeState(SpiritInputTag);
	if (!RuntimeState)
	{
		return;
	}

	ClearSpiritSkillComboTimer(*RuntimeState);
	RuntimeState->Reset();
	SpiritSkillComboRuntimeStates.Remove(SpiritInputTag);
}

void UHeroCombatComponent::ResetAllSpiritSkillComboStates()
{
	for (TPair<FGameplayTag, FHeroSpiritSkillComboRuntimeState>& Pair : SpiritSkillComboRuntimeStates)
	{
		ClearSpiritSkillComboTimer(Pair.Value);
		Pair.Value.Reset();
	}

	SpiritSkillComboRuntimeStates.Reset();
}

void UHeroCombatComponent::HandleSpiritSkillComboTimeoutByInputTag(FGameplayTag SpiritInputTag)
{
	FHeroSpiritSkillComboRuntimeState* RuntimeState = FindMutableSpiritSkillComboRuntimeState(SpiritInputTag);
	if (!RuntimeState || !RuntimeState->HasWaitingState())
	{
		return;
	}

	ClearSpiritSkillComboTimer(*RuntimeState);
	ClearBufferedSpiritSkillInputByTag(SpiritInputTag);

	const bool bCooldownCommitted =
		GetOwningActionAbilitySystemComponent()
		&& GetOwningActionAbilitySystemComponent()->ApplyAbilityCooldownByInputTag(SpiritInputTag);
	const FGameplayTag SpiritCooldownTag = ActionGameplayTags::ResolveSpiritSkillCooldownTag(SpiritInputTag);

	Debug::Print(
		FString::Printf(
			TEXT("[Combat][SpiritSkill] Wait timeout: input=%s cooldown=%s cooldown_committed=%d"),
			*SpiritInputTag.ToString(),
			SpiritCooldownTag.IsValid() ? *SpiritCooldownTag.ToString() : TEXT("无效"),
			bCooldownCommitted ? 1 : 0),
		bCooldownCommitted ? FColor::Yellow : FColor::Red,
		1.5f);

	ResetSpiritSkillComboState(SpiritInputTag);
}

void UHeroCombatComponent::OpenTimedCombatWindow(
	FTimerHandle& TimerHandle,
	void (UHeroCombatComponent::*ExpireCallback)(),
	const float Duration,
	const FGameplayTag OpenEventTag)
{
	BroadcastCombatEvent(OpenEventTag);
	StartCombatTimer(TimerHandle, ExpireCallback, Duration);
}

void UHeroCombatComponent::CloseTimedCombatWindow(
	FTimerHandle& TimerHandle,
	const bool bWasActive,
	const FGameplayTag CloseEventTag)
{
	BroadcastCombatEventIf(bWasActive, CloseEventTag);
	ClearCombatTimer(TimerHandle);
}

bool UHeroCombatComponent::HandleIncomingCombatEvent(FGameplayTag InCombatEventTag, AActor* InstigatorActor)
{
	if (!InCombatEventTag.IsValid())
	{
		return false;
	}

	return TryHandleResolvedCombatReactEvent(InCombatEventTag, InstigatorActor);
}

bool UHeroCombatComponent::TryHandleIncomingDamage(const FActionDamagePayload& InDamagePayload, FActionHitResolveResult& OutResult)
{
	OutResult = FActionHitResolveResult();

	if (!InDamagePayload.IsValidPayload())
	{
		OutResult.ResultType = EActionHitResultType::Ignored;
		return false;
	}

	if (UHeroDefenseComponent* DefenseComponent = GetOwningHeroDefenseComponent())
	{
		return DefenseComponent->TryHandleIncomingDamageDefenseReaction(InDamagePayload, OutResult);
	}

	OutResult.ResultType = EActionHitResultType::None;
	return false;
}

bool UHeroCombatComponent::TryHandleResolvedCombatReactEvent(
	const FGameplayTag InCombatEventTag,
	AActor* InstigatorActor)
{
	if (InCombatEventTag == ActionGameplayTags::Combat_Event_HitReact)
	{
		HandleHitReactEvent(InstigatorActor);
		return true;
	}

	if (InCombatEventTag == ActionGameplayTags::Combat_Event_GuardBreak)
	{
		HandleGuardBreakEvent(InstigatorActor);
		return true;
	}

	if (InCombatEventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		HandlePoiseBreakEvent(InstigatorActor);
		return true;
	}

	if (InCombatEventTag == ActionGameplayTags::Combat_Event_Launch)
	{
		HandleLaunchEvent(InstigatorActor);
		return true;
	}

	if (InCombatEventTag == ActionGameplayTags::Combat_Event_Knockdown)
	{
		HandleKnockdownEvent(InstigatorActor);
		return true;
	}

	return false;
}


void UHeroCombatComponent::ResetCombatInputRecoveryRuntime()
{
	if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
	{
		InputComponent->ClearBufferedInput();
	}
	CloseAbilityChainWindow();
	CloseAbilityCancelWindow();
	if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
	{
		InputComponent->ResetCombatInputRecoveryRuntime();
	}
}

void UHeroCombatComponent::ResetOwnedCombatSubcomponentRuntimeStates()
{
	if (UHeroAttackComponent* AttackComponent = GetOwningHeroAttackComponent())
	{
		AttackComponent->ResetRuntimeStateForHeroStartup();
	}
	if (UHeroDefenseComponent* DefenseComponent = GetOwningHeroDefenseComponent())
	{
		DefenseComponent->ResetRuntimeStateForHeroStartup();
	}
	if (UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		ExecutionComponent->ResetRuntimeStateForHeroStartup();
	}
}

void UHeroCombatComponent::ResetAttackPresentationRuntime(const bool bRestoreAttackEnabled)
{
	StopCurrentAttackMontage();
	ClearActiveDamageContext();
	if (CurrentEquippedWeaponInstance)
	{
		CurrentEquippedWeaponInstance->EndAttackDetection();
	}
	if (UHeroHitSourceComponent* HitSourceComponent = GetOwningHeroHitSourceComponent())
	{
		HitSourceComponent->ResetHitWindowRuntime();
	}
	if (UHeroAttackComponent* AttackComponent = GetOwningHeroAttackComponent())
	{
		AttackComponent->ClearCurrentAttackHitConfig();
	}
	ResetComboIndex();
	SetCombatMode(EHeroCombatMode::Idle);
	if (bRestoreAttackEnabled)
	{
		SetAttackEnabled(true);
	}
}

void UHeroCombatComponent::ResetCombatReactionState()
{
	ResetCombatInputRecoveryRuntime();
	ResetOwnedCombatSubcomponentRuntimeStates();

	bCombatReactStateResetInProgress = true;

	if (UHeroDefenseComponent* DefenseComponent = GetOwningHeroDefenseComponent())
	{
		DefenseComponent->HandleCombatReactStateReset();
	}

	bCombatReactStateResetInProgress = false;
}

void UHeroCombatComponent::HandleHitReactEvent(AActor* InstigatorActor)
{
	ResetCombatReactionState();
}

void UHeroCombatComponent::HandleGuardBreakEvent(AActor* InstigatorActor)
{
	ResetCombatReactionState();
	if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
	{
		InputComponent->ClearInputStateByTag(ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense);
	}
}

void UHeroCombatComponent::HandlePoiseBreakEvent(AActor* InstigatorActor)
{
	ResetCombatReactionState();
}

void UHeroCombatComponent::HandleLaunchEvent(AActor* InstigatorActor)
{
	ResetCombatReactionState();
}

void UHeroCombatComponent::HandleKnockdownEvent(AActor* InstigatorActor)
{
	ResetCombatReactionState();
}

bool UHeroCombatComponent::CanExecuteTarget(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->CanExecuteTarget(InTargetActor, OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::CanActivateExecutionAbility(AActor*& OutTargetActor, FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->CanActivateExecutionAbility(OutTargetActor, OutFailureReason);
	}

	OutTargetActor = nullptr;
	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::RevalidateExecutionAbilityAfterActivation(
	AActor*& OutTargetActor,
	FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->RevalidateExecutionAbilityAfterActivation(OutTargetActor, OutFailureReason);
	}

	OutTargetActor = nullptr;
	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::TryFindExecutionTarget(AActor*& OutTargetActor, FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->TryFindExecutionTarget(OutTargetActor, OutFailureReason);
	}

	OutTargetActor = nullptr;
	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::TryReserveExecutionTarget(AActor* InTargetActor) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->TryReserveExecutionTarget(InTargetActor);
	}

	return false;
}

bool UHeroCombatComponent::TryCommitExecutionTarget(AActor* InTargetActor) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->TryCommitExecutionTarget(InTargetActor);
	}

	return false;
}

void UHeroCombatComponent::CancelReservedExecutionTarget(AActor* InTargetActor) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		ExecutionComponent->CancelReservedExecutionTarget(InTargetActor);
	}
}

void UHeroCombatComponent::ReleaseExecutionTargetVictimLock(AActor* InTargetActor) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		ExecutionComponent->ReleaseExecutionTargetVictimLock(InTargetActor);
	}
}

void UHeroCombatComponent::AbortConsumedExecutionTargetPresentation(AActor* InTargetActor) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		ExecutionComponent->AbortConsumedExecutionTargetPresentation(InTargetActor);
	}
}

bool UHeroCombatComponent::TryExecuteReservedExecutionTarget(
	AActor* InTargetActor,
	FActionHitResolveResult& OutResolveResult) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->TryExecuteReservedExecutionTarget(InTargetActor, OutResolveResult);
	}

	OutResolveResult = FActionHitResolveResult();
	return false;
}

UAnimMontage* UHeroCombatComponent::GetExecutionMontageForCurrentWeapon() const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->GetExecutionMontageForCurrentWeapon();
	}

	return nullptr;
}

float UHeroCombatComponent::GetExecutionVictimTurnDurationForCurrentWeapon() const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->GetExecutionVictimTurnDurationForCurrentWeapon();
	}

	return 0.f;
}

float UHeroCombatComponent::GetExecutionStartDistanceForCurrentWeapon() const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->GetExecutionStartDistanceForCurrentWeapon();
	}

	return 0.f;
}

bool UHeroCombatComponent::CanStartReservedExecutionPresentation(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->CanStartReservedExecutionPresentation(InTargetActor, OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::TryBeginExecutionTargetPreparation(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->TryBeginExecutionTargetPreparation(InTargetActor, OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::IsExecutionTargetPreparationReady(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->IsExecutionTargetPreparationReady(InTargetActor, OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::TryStartExecutionTargetPresentation(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->TryStartExecutionTargetPresentation(InTargetActor, OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::DescribeExecutionTargetPreparationState(
	AActor* InTargetActor,
	FString& OutDescription,
	FString* OutFailureReason) const
{
	if (const UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->DescribeExecutionTargetPreparationState(
			InTargetActor,
			OutDescription,
			OutFailureReason);
	}

    OutDescription = TEXT("Execution prepare state: ExecutionCoordinator is invalid.");
	if (OutFailureReason)
	{
		*OutFailureReason = TEXT("hero execution coordinator component is invalid");
	}
	return false;
}

bool UHeroCombatComponent::ConsumeSpecialWeaponSwitchEnergy()
{
	if (UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		return EquipmentComponent->ConsumeSpecialWeaponSwitchEnergy();
	}

	return false;
}

void UHeroCombatComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UHeroCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopActiveCombatModeTransitionMontage(false);
	if (UHeroTargetingComponent* TargetingComponent = GetOwningHeroTargetingComponent())
	{
		TargetingComponent->ClearTargetLock(EActionTargetLockBreakReason::RuntimeReset);
	}
	ResetAllSpiritSkillComboStates();
	Super::EndPlay(EndPlayReason);
}

void UHeroCombatComponent::HandleCombatInputPressed(const FGameplayTag InputTag)
{
	if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
	{
		InputComponent->HandleCombatInputPressed(InputTag);
	}
}

void UHeroCombatComponent::HandleCombatInputHeld(const FGameplayTag InputTag)
{
	if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
	{
		InputComponent->HandleCombatInputHeld(InputTag);
	}
}

void UHeroCombatComponent::HandleCombatInputReleased(const FGameplayTag InputTag)
{
	if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
	{
		InputComponent->HandleCombatInputReleased(InputTag);
	}
}

bool UHeroCombatComponent::HandleAllPressedLogic(
	UActionAbilitySystemComponent* InActionASC,
	FGameplayTag InputTag)
{
	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_LockTarget)
	{
		if (UHeroTargetingComponent* TargetingComponent = GetOwningHeroTargetingComponent())
		{
			return TargetingComponent->ToggleTargetLock();
		}

		return false;
	}

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_Execution)
	{
		return HandleExecutionLogic(InActionASC, InputTag);
	}

	return false;
}

bool UHeroCombatComponent::HandleAllHeldLogic(
	UActionAbilitySystemComponent* InActionASC,
	FGameplayTag InputTag)
{
	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_LockTarget)
	{
		return true;
	}

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_Execution)
	{
		return true;
	}

	return false;
}

bool UHeroCombatComponent::HandleAllReleasedLogic(
	UActionAbilitySystemComponent* InActionASC,
	FGameplayTag InputTag)
{
	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_LockTarget)
	{
		return true;
	}

	return false;
}

bool UHeroCombatComponent::TryResolveWeaponLoadoutSlotFromInputTag(const FGameplayTag InputTag, EHeroWeaponLoadoutSlot& OutLoadoutSlot) const
{
	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Unarmed)
	{
		OutLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;
		return true;
	}

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Melee)
	{
		OutLoadoutSlot = EHeroWeaponLoadoutSlot::MeleeWeapon;
		return true;
	}

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Ranged)
	{
		OutLoadoutSlot = EHeroWeaponLoadoutSlot::RangedWeapon;
		return true;
	}

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Hybrid)
	{
		OutLoadoutSlot = EHeroWeaponLoadoutSlot::HybridWeapon;
		return true;
	}

	return false;
}

FGameplayTag UHeroCombatComponent::GetInputTagForWeaponLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		return WeaponSwitchComponent->GetInputTagForWeaponLoadoutSlot(InLoadoutSlot);
	}

	return FGameplayTag();
}

bool UHeroCombatComponent::IsWeaponSwitchInputTag(const FGameplayTag InputTag) const
{
	if (const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		return WeaponSwitchComponent->IsWeaponSwitchInputTag(InputTag);
	}

	return false;
}

EHeroWeaponLoadoutSlot UHeroCombatComponent::GetCurrentCombatLoadoutSlot() const
{
	if (const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		return EquipmentComponent->GetCurrentEquippedLoadoutSlot();
	}

	return EHeroWeaponLoadoutSlot::Unarmed;
}

FGameplayTag UHeroCombatComponent::ResolveLoadoutScopedCombatInputTag(const FGameplayTag InputTag) const
{
	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_Dodge)
	{
		return HeroCombatTagRouting::ResolveTagByLoadoutSlot(
			GetCurrentCombatLoadoutSlot(),
			ActionGameplayTags::InputTag_GameplayAbility_Dodge_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Dodge_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Dodge_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Dodge_Hybrid);
	}

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense)
	{
		return HeroCombatTagRouting::ResolveTagByLoadoutSlot(
			GetCurrentCombatLoadoutSlot(),
			ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense_Hybrid);
	}

	return InputTag;
}

bool UHeroCombatComponent::IsWeaponSwitchBlockedByCombatState(const FGameplayTag InputTag) const
{
	if (const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		return WeaponSwitchComponent->IsWeaponSwitchBlockedByCombatState(InputTag);
	}

	return true;
}

bool UHeroCombatComponent::IsWeaponSwitchBlockedByCooldown() const
{
	if (const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		return WeaponSwitchComponent->IsWeaponSwitchBlockedByCooldown();
	}

	return false;
}

void UHeroCombatComponent::UpdateWeaponSwitchCooldownUIState()
{
	const bool bCurrentBlockedByCooldown = IsWeaponSwitchBlockedByCooldown();
	if (bLastWeaponSwitchCooldownBlocked == bCurrentBlockedByCooldown)
	{
		return;
	}

	bLastWeaponSwitchCooldownBlocked = bCurrentBlockedByCooldown;
	BroadcastCombatUIStateChanged();
}

void UHeroCombatComponent::UpdateCombatModeIdleExitTransition(const float DeltaTime)
{
	// Combat -> Idle 的正式退出判断只在组件侧统一执行，主 ABP 不再并行轮询这条时序。
	if (bCombatModeTransitionActive || GetCombatMode() != EHeroCombatMode::Combo)
	{
		CurrentCombatIdleElapsedSeconds = 0.f;
		return;
	}

	if (WindowRuntimeState.IsDefenseActive()
		|| WindowRuntimeState.IsDodgeActive()
		|| IsWeaponSwitchPresentationActive()
		|| GetCurrentRunningAnimMontage() != nullptr)
	{
		CurrentCombatIdleElapsedSeconds = 0.f;
		return;
	}

	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	const UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter
		? OwnerHeroCharacter->GetCharacterMovement()
		: nullptr;
	if (!MovementComponent
		|| MovementComponent->IsFalling()
		|| MovementComponent->Velocity.SizeSquared2D() > KINDA_SMALL_NUMBER
		|| !MovementComponent->GetCurrentAcceleration().IsNearlyZero())
	{
		CurrentCombatIdleElapsedSeconds = 0.f;
		return;
	}

	CurrentCombatIdleElapsedSeconds += DeltaTime;
	if (CurrentCombatIdleElapsedSeconds < CombatModeIdleExitDelaySeconds)
	{
		return;
	}

	CurrentCombatIdleElapsedSeconds = 0.f;
	TryBeginExitCombatModeTransition();
}

bool UHeroCombatComponent::TryBeginExitCombatModeTransition()
{
	if (GetCombatMode() != EHeroCombatMode::Combo || bCombatModeTransitionActive)
	{
		return false;
	}

	// Combat -> Idle 的正式退出资格已经由战斗组件自己判完。
	// 若当前武器有 Exit 过渡蒙太奇，就把“何时切回 HolsterSocket”交给 Exit Montage；
	// 否则直接写回 Idle，并让当前已装备武器立刻回 Holster 表现。
	// 若当前武器没有配置 Exit 蒙太奇，则直接写回 Idle；否则由 Exit 过渡推进挂点切回 Holster。
	UAnimMontage* ExitCombatModeMontage = GetCombatModeTransitionAnimMontage();
	if (TryPlayCombatModeTransitionMontage(ExitCombatModeMontage, true))
	{
		return true;
	}

	SetCombatMode(EHeroCombatMode::Idle);
	return true;
}

bool UHeroCombatComponent::TryPlayCombatModeTransitionMontage(
	UAnimMontage* TransitionMontage,
	const bool bToIdle)
{
	if (!TransitionMontage)
	{
		return false;
	}

	UAnimInstance* OwnerAnimInstance = GetOwnerAnimInstance();
	if (!OwnerAnimInstance)
	{
		UE_LOG(
			LogHeroCombatComponent,
			Warning,
			TEXT("CombatModeTransition 播放失败：AnimInstance 无效。owner=%s target_idle=%s montage=%s"),
			*GetNameSafe(GetOwner()),
			bToIdle ? TEXT("true") : TEXT("false"),
			*GetNameSafe(TransitionMontage));
		return false;
	}

	StopActiveCombatModeTransitionMontage(!bToIdle);

	if (OwnerAnimInstance->Montage_Play(TransitionMontage) <= 0.f)
	{
		UE_LOG(
			LogHeroCombatComponent,
			Warning,
			TEXT("CombatModeTransition 播放失败：Montage_Play rejected。owner=%s target_idle=%s montage=%s"),
			*GetNameSafe(GetOwner()),
			bToIdle ? TEXT("true") : TEXT("false"),
			*GetNameSafe(TransitionMontage));
		return false;
	}

	// 从这里开始，过渡蒙太奇就是当前这条 Combat 表现过渡的正式运行态：
	// 1. CurrentRunningAnimMontage 用于 notify 过滤和异常收尾；
	// 2. ReactGuardContext 用于演出期间的受击接管阈值；
	// 3. 移动锁只服务这段过渡，不扩成新的长期战斗状态。
	bCombatModeTransitionActive = true;
	bCombatModeTransitionTargetIdle = bToIdle;
	bCombatModeTransitionNotifyReceived = false;
	ActiveCombatModeTransitionMontage = TransitionMontage;
	UpdateRunningAnimMontage(TransitionMontage);
	SetRunningAnimationReactGuardContext(
		TransitionMontage,
		EActionRunningAnimationSemantic::NonReact,
		CurrentEquippedWeaponDefinition
			? CurrentEquippedWeaponDefinition->GetCombatModeTransitionReactGuardThreshold(
				bToIdle ? EHeroCombatMode::Combo : EHeroCombatMode::Idle)
			: 0);
	ApplyCombatModeTransitionMoveLock(true);

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &ThisClass::HandleCombatModeTransitionMontageEnded);
	OwnerAnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, TransitionMontage);
	return true;
}

void UHeroCombatComponent::FinishCombatModeTransitionMontage(
	UAnimMontage* TransitionMontage,
	const bool bInterrupted)
{
	if (!bCombatModeTransitionActive || ActiveCombatModeTransitionMontage != TransitionMontage)
	{
		return;
	}

	const bool bTargetIdle = bCombatModeTransitionTargetIdle;
	const bool bNotifyReceived = bCombatModeTransitionNotifyReceived;
	ClearRunningAnimMontageReferenceIfMatches(TransitionMontage);
	ClearCombatModeTransitionRuntime();

	// 结束收尾固定按两层处理：
	// 1. 先清这段过渡的运行时与移动锁；
	// 2. 再根据目标态、是否中断、Notify 是否真的到达，决定当前已装备武器最终该挂在哪个 socket。
	// 这样就算 Notify 缺失或蒙太奇异常结束，也不会残留错误挂点。
	if (bTargetIdle)
	{
		if (bInterrupted)
		{
			RequestCurrentEquippedWeaponPresentationToCombatSocket();
			return;
		}

		if (!bNotifyReceived)
		{
			UE_LOG(
				LogHeroCombatComponent,
				Warning,
				TEXT("CombatModeTransition Exit 蒙太奇缺少或未命中 CombatWeaponPresentationSwitch Notify，已走结束兜底。owner=%s montage=%s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(TransitionMontage));
			RequestCurrentEquippedWeaponPresentationToHolster();
		}

		// 退出 Combat 表现态时，只有 Exit 过渡正式收尾后才写回 Idle。
		SetCombatMode(EHeroCombatMode::Idle);
		return;
	}

	if (!bNotifyReceived)
	{
		UE_LOG(
			LogHeroCombatComponent,
			Warning,
			TEXT("CombatModeTransition Enter 蒙太奇缺少或未命中 CombatWeaponPresentationSwitch Notify，已走结束兜底。owner=%s montage=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(TransitionMontage));
	}

	RequestCurrentEquippedWeaponPresentationToCombatSocket();
}

void UHeroCombatComponent::ApplyCombatModeTransitionMoveLock(const bool bLocked)
{
	if (bCombatModeTransitionMoveInputLocked == bLocked)
	{
		if (bLocked)
		{
			if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
			{
				if (UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter->GetCharacterMovement())
				{
					MovementComponent->StopMovementImmediately();
				}
			}
		}
		return;
	}

	bCombatModeTransitionMoveInputLocked = bLocked;

	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		if (AController* OwnerController = OwnerHeroCharacter->GetController())
		{
			OwnerController->SetIgnoreMoveInput(bLocked);
		}

		if (UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}
}

void UHeroCombatComponent::ClearCombatModeTransitionRuntime()
{
	// 这里清的只是过渡壳运行态：
	// 它不会回滚当前 CombatMode，也不会重写当前武器 socket 目标态，
	// 真正的挂点恢复仍要由结束收尾函数按当前语义决定。
	ApplyCombatModeTransitionMoveLock(false);
	bCombatModeTransitionActive = false;
	bCombatModeTransitionTargetIdle = false;
	bCombatModeTransitionNotifyReceived = false;
	ActiveCombatModeTransitionMontage = nullptr;
}

void UHeroCombatComponent::StopActiveCombatModeTransitionMontage(const bool bRestoreSocketForCurrentCombatMode)
{
	UAnimInstance* OwnerAnimInstance = GetOwnerAnimInstance();
	if (OwnerAnimInstance && ActiveCombatModeTransitionMontage && OwnerAnimInstance->Montage_IsPlaying(ActiveCombatModeTransitionMontage))
	{
		FOnMontageEnded EmptyMontageEndedDelegate;
		OwnerAnimInstance->Montage_SetEndDelegate(EmptyMontageEndedDelegate, ActiveCombatModeTransitionMontage);
		OwnerAnimInstance->Montage_Stop(0.1f, ActiveCombatModeTransitionMontage);
	}

	ClearRunningAnimMontageReferenceIfMatches(ActiveCombatModeTransitionMontage);
	ClearCombatModeTransitionRuntime();
	if (bRestoreSocketForCurrentCombatMode)
	{
		// 这里的兜底只认“当前正式 CombatMode 是什么”，
		// 不尝试猜测被打断那一段过渡原本想切到哪一边。
		// 这样异常终止时，当前已装备武器至少能回到和现行 CombatMode 一致的挂点表现。
		if (GetCombatMode() == EHeroCombatMode::Idle)
		{
			RequestCurrentEquippedWeaponPresentationToHolster();
		}
		else
		{
			RequestCurrentEquippedWeaponPresentationToCombatSocket();
		}
	}
}

void UHeroCombatComponent::RefreshCurrentEquippedWeaponSocketPresentation() const
{
	// 这里不是重新装备武器，只是把“当前已经装备着的那把武器”
	// 按最新 Combat 表现态重新落到 HolsterSocket 或 WeaponSocket。
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	UHeroAssemblyComponent* HeroAssemblyComponent = OwnerHeroCharacter
		? OwnerHeroCharacter->GetHeroAssemblyComponent()
		: nullptr;
	if (HeroAssemblyComponent)
	{
		HeroAssemblyComponent->RefreshCurrentEquippedWeaponSocketPresentation();
	}
}

void UHeroCombatComponent::RequestCurrentEquippedWeaponPresentationToCombatSocket()
{
	// 这里只改“当前已装备武器应该挂手上”的表现目标态，
	// 真正的附挂、隐藏和碰撞切换由装配桥统一落地。
	bCurrentEquippedWeaponShouldUseWeaponSocketPresentation = true;
	RefreshCurrentEquippedWeaponSocketPresentation();
}

void UHeroCombatComponent::RequestCurrentEquippedWeaponPresentationToHolster()
{
	// 和 CombatSocket 请求对称：这里只改当前已装备武器的目标挂点语义，
	// 不在这里重新走装备链或 linked layer 切换链。
	bCurrentEquippedWeaponShouldUseWeaponSocketPresentation = false;
	RefreshCurrentEquippedWeaponSocketPresentation();
}

void UHeroCombatComponent::BroadcastCombatUIStateChanged() const
{
	CombatUIStateChangedDelegate.Broadcast();
}

bool UHeroCombatComponent::CanActivateWeaponSwitchAbility(const FGameplayTag InputTag) const
{
	if (const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		return WeaponSwitchComponent->CanActivateWeaponSwitchAbility(InputTag);
	}

	return false;
}

void UHeroCombatComponent::CancelActiveAttackAbilityIfNeeded(UActionAbilitySystemComponent* InActionASC) const
{
	if (!InActionASC)
	{
		return;
	}

	InActionASC->CancelAbilityByAbilityTag(ActionGameplayTags::Player_Ability_Attack);
}

bool UHeroCombatComponent::HandleExecutionLogic(UActionAbilitySystemComponent* InActionASC, FGameplayTag InputTag)
{
	if (UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->HandleExecutionLogic(InActionASC, InputTag);
	}

	return false;
}

bool UHeroCombatComponent::TryCommitWeaponSwitchAbilityInput(UActionAbilitySystemComponent* InActionASC, const FGameplayTag InputTag)
{
	if (UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		return WeaponSwitchComponent->TryCommitWeaponSwitchAbilityInput(InActionASC, InputTag);
	}

	return false;
}

bool UHeroCombatComponent::TryCommitExecutionAbilityInput(UActionAbilitySystemComponent* InActionASC)
{
	if (UHeroExecutionCoordinatorComponent* ExecutionComponent = GetOwningHeroExecutionCoordinatorComponent())
	{
		return ExecutionComponent->TryCommitExecutionAbilityInput(InActionASC);
	}

	return false;
}

bool UHeroCombatComponent::HandleWeaponLoadoutSlotInput(
	UActionAbilitySystemComponent* InActionASC,
	FGameplayTag InputTag,
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	if (UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		return WeaponSwitchComponent->HandleWeaponLoadoutSlotInput(InActionASC, InputTag, InLoadoutSlot);
	}

	return false;
}

bool UHeroCombatComponent::HandleCombatActionInputByEvent(
	UActionAbilitySystemComponent* InActionASC,
	FGameplayTag InputTag,
	const EActionInputEvent InputEvent,
	FGameplayTag ResolvedAttackRequestTag)
{
	if (InputEvent != EActionInputEvent::Released)
	{
		EHeroWeaponLoadoutSlot TargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
		if (TryResolveWeaponLoadoutSlotFromInputTag(InputTag, TargetLoadoutSlot))
		{
			return InputEvent == EActionInputEvent::Pressed
				? HandleWeaponLoadoutSlotInput(InActionASC, InputTag, TargetLoadoutSlot)
				: true;
		}
	}

	if (UHeroAttackComponent* AttackComponent = GetOwningHeroAttackComponent())
	{
		if (AttackComponent->TryHandleAttackInputByEvent(
			InActionASC,
			InputTag,
			InputEvent,
			ResolvedAttackRequestTag))
		{
			return true;
		}
	}

	if (UHeroDefenseComponent* DefenseComponent = GetOwningHeroDefenseComponent())
	{
		if (DefenseComponent->TryHandleDefenseInputByEvent(InActionASC, InputTag, InputEvent))
		{
			return true;
		}
	}

	switch (InputEvent)
	{
	case EActionInputEvent::Pressed:
		if (HandleAllPressedLogic(InActionASC, InputTag))
		{
			return true;
		}
		break;

	case EActionInputEvent::Held:
		if (HandleAllHeldLogic(InActionASC, InputTag))
		{
			return true;
		}
		break;

	case EActionInputEvent::Released:
		if (HandleAllReleasedLogic(InActionASC, InputTag))
		{
			return true;
		}
		break;

	default:
		break;
	}

	// Spirit 主动技能不走攻击/防御/处决这些专用分发壳，
	// 而是直接按输入标签命中 ASC 中已授予的 Spirit AbilitySpec。
	if (ActionGameplayTags::IsSpiritSkillInputTag(InputTag))
	{
		switch (InputEvent)
		{
		case EActionInputEvent::Pressed:
			return InActionASC->OnAbilityInputPressed(InputTag);

		case EActionInputEvent::Held:
			return InActionASC->OnAbilityInputHeld(InputTag);

		case EActionInputEvent::Released:
			InActionASC->OnAbilityInputReleased(InputTag);
			return InActionASC->GetActivatableAbilitySpecByTag(InputTag) != nullptr;

		default:
			break;
		}
	}

	return false;
}

bool UHeroCombatComponent::IsAttackInputTag(const FGameplayTag InputTag) const
{
	return InputTag == ActionGameplayTags::InputTag_GameplayAbility_Attack;
}

bool UHeroCombatComponent::IsAbilityCancelContextActive() const
{
	if (WindowRuntimeState.HasCombatLockState())
	{
		return true;
	}

	if (IsWeaponSwitchPresentationCancelContextActive())
	{
		return true;
	}

	const UActionCombatReactComponent* CombatReactComponent = GetOwningCombatReactComponent();
	if (CombatReactComponent
		&& CombatReactComponent->IsRecoveryPhaseActive()
		&& AbilityWindowRuntimeState.IsAbilityCancelWindowActive())
	{
		return true;
	}

	const UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!OwnerASC)
	{
		return false;
	}

	return OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Attack_Active)
		|| OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Execution_Active);
}

bool UHeroCombatComponent::IsWeaponSwitchPresentationChainContextActive() const
{
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	return WeaponSwitchComponent
		&& WeaponSwitchComponent->IsWeaponSwitchPresentationActive()
		&& AbilityWindowRuntimeState.IsAbilityChainWindowActive();
}

bool UHeroCombatComponent::IsWeaponSwitchPresentationCancelContextActive() const
{
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	return WeaponSwitchComponent
		&& WeaponSwitchComponent->IsWeaponSwitchPresentationActive()
		&& AbilityWindowRuntimeState.IsAbilityCancelWindowActive();
}

bool UHeroCombatComponent::IsAbilityCancelInputAllowedNow(const FGameplayTag InputTag) const
{
	return InputTag.IsValid()
		&& AbilityWindowRuntimeState.IsAbilityCancelWindowActive()
		&& AbilityWindowRuntimeState.AcceptsCancelInput(InputTag);
}

bool UHeroCombatComponent::IsWeaponSwitchPresentationChainInputAllowed(const FGameplayTag InputTag) const
{
	return InputTag.IsValid()
		&& IsWeaponSwitchPresentationChainContextActive()
		&& AbilityWindowRuntimeState.AcceptsChainInput(InputTag);
}

bool UHeroCombatComponent::IsWeaponSwitchPresentationCancelInputAllowed(const FGameplayTag InputTag) const
{
	return IsWeaponSwitchPresentationCancelContextActive()
		&& IsAbilityCancelInputAllowedNow(InputTag);
}

bool UHeroCombatComponent::CanBufferCombatInputNow(const FGameplayTag InputTag) const
{
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	const UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();

	if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_LockTarget)
	{
		return false;
	}

	if (!IsAttackInputTag(InputTag) && IsNonAttackInputBlockedByAirborneState())
	{
		return false;
	}

	if (!InputTag.IsValid())
	{
		return false;
	}

	if (IsAttackInputTag(InputTag))
	{
		if (IsAttackInputBlockedByCombatReact(InputTag))
		{
			return false;
		}
	}
	else if (IsNonAttackInputBlockedByCombatReact(InputTag))
	{
		return false;
	}

	if (WeaponSwitchComponent
		&& WeaponSwitchComponent->IsWeaponSwitchPresentationActive()
		&& !IsWeaponSwitchPresentationCancelInputAllowed(InputTag))
	{
		return !IsWeaponSwitchInputTag(InputTag);
	}

	if (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchTransactionInProgress())
	{
		return false;
	}

	if (IsAbilityCancelContextActive())
	{
		if (InputTag == ActionGameplayTags::InputTag_GameplayAbility_Execution
			&& OwnerASC
			&& OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Execution_Active))
		{
			return false;
		}

		if (WindowRuntimeState.HasCombatLockState())
		{
			return AbilityWindowRuntimeState.IsAbilityCancelWindowActive()
				&& AbilityWindowRuntimeState.AcceptsCancelInput(InputTag);
		}

		return !AbilityWindowRuntimeState.IsAbilityCancelWindowActive()
			|| AbilityWindowRuntimeState.AcceptsCancelInput(InputTag);
	}

	if (!IsAttackEnabled())
	{
		return true;
	}

	return IsWeaponSwitchInputTag(InputTag) && IsWeaponSwitchBlockedByCombatState(InputTag);
}
bool UHeroCombatComponent::CanConsumeBufferedInputNow(const FActionBufferedInput& BufferedInput) const
{
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	const UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent();

	if (BufferedInput.InputTag == ActionGameplayTags::InputTag_GameplayAbility_LockTarget)
	{
		return false;
	}

	if (BufferedInput.TriggerEvent == EActionInputEvent::Held
		&& (!InputComponent || InputComponent->GetInputButtonStateByTag(BufferedInput.InputTag) != EActionInputButtonState::Held))
	{
		return false;
	}

	if (!BufferedInput.InputTag.IsValid())
	{
		return false;
	}

	if (!IsAttackInputTag(BufferedInput.InputTag) && IsNonAttackInputBlockedByAirborneState())
	{
		return false;
	}

	if (IsAttackInputTag(BufferedInput.InputTag))
	{
		if (IsAttackInputBlockedByCombatReact(BufferedInput.InputTag))
		{
			return false;
		}
	}
	else if (IsNonAttackInputBlockedByCombatReact(BufferedInput.InputTag))
	{
		return false;
	}

	if (WeaponSwitchComponent
		&& WeaponSwitchComponent->IsWeaponSwitchPresentationActive()
		&& !IsWeaponSwitchPresentationCancelInputAllowed(BufferedInput.InputTag))
	{
		return false;
	}

	if (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchTransactionInProgress())
	{
		return false;
	}

	if (IsWeaponSwitchInputTag(BufferedInput.InputTag) && IsWeaponSwitchBlockedByCombatState(BufferedInput.InputTag))
	{
		return false;
	}

	if (IsWeaponSwitchInputTag(BufferedInput.InputTag) && IsWeaponSwitchBlockedByCooldown())
	{
		return false;
	}

	if (IsAbilityCancelContextActive())
	{
		if (const UHeroDefenseComponent* DefenseComponent = GetOwningHeroDefenseComponent())
		{
			return DefenseComponent->CanActivateNonAttackInputNow(BufferedInput.InputTag);
		}

		return false;
	}

	return true;
}

bool UHeroCombatComponent::ShouldBufferInput(const FGameplayTag InputTag) const
{
	return CanBufferCombatInputNow(InputTag);
}

bool UHeroCombatComponent::ShouldQueueBufferedInput(const FGameplayTag InputTag, const bool bCanBuffer) const
{
	return bCanBuffer && ShouldBufferInput(InputTag);
}

bool UHeroCombatComponent::ProcessAbilityInput(
	FGameplayTag InputTag,
	EActionInputEvent InputEvent,
	bool bCanBuffer,
	FGameplayTag ResolvedAttackRequestTag)
{
	UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!OwnerASC || !InputTag.IsValid() || !CanProcessCombatInput())
	{
		return false;
	}

	RepairStaleCombatRuntimeIfNeeded();

	if (IsAttackInputTag(InputTag))
	{
		if (UHeroAttackComponent* AttackComponent = GetOwningHeroAttackComponent())
		{
			return AttackComponent->ProcessAttackInput(
				OwnerASC,
				InputTag,
				InputEvent,
				bCanBuffer,
				ResolvedAttackRequestTag);
		}

		return false;
	}

	if (HandleCombatActionInputByEvent(OwnerASC, InputTag, InputEvent, ResolvedAttackRequestTag))
	{
		return true;
	}

	if (InputEvent == EActionInputEvent::Pressed
		&& ActionGameplayTags::IsSpiritSkillInputTag(InputTag))
	{
		const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
		if (!CurrentWeaponDefinition)
		{
			UE_LOG(
				LogHeroCombatComponent,
				Warning,
				TEXT("[SpiritSkill] 输入未命中：当前武器定义无效。InputTag=%s"),
				*InputTag.ToString());
		}
		else if (!CurrentWeaponDefinition->SupportsSpiritWeaponAbilities())
		{
			UE_LOG(
				LogHeroCombatComponent,
				Warning,
				TEXT("[SpiritSkill] 输入未命中：当前装备武器不是灵武器。InputTag=%s WeaponTag=%s PropertyType=%d HasSpiritEntries=%s"),
				*InputTag.ToString(),
				*CurrentWeaponDefinition->WeaponTag.ToString(),
				static_cast<int32>(CurrentWeaponDefinition->GetWeaponPropertyType()),
				CurrentWeaponDefinition->HasAnySpiritAbilityEntryConfigs() ? TEXT("true") : TEXT("false"));
		}
		else
		{
			FActionSpiritAbilityEntryConfig ResolvedSpiritEntryConfig;
			if (!CurrentWeaponDefinition->TryResolveSpiritAbilityEntryConfigByInputTag(InputTag, ResolvedSpiritEntryConfig))
			{
				UE_LOG(
					LogHeroCombatComponent,
					Warning,
					TEXT("[SpiritSkill] 输入未命中：当前灵武器没有为该输入配置 Spirit 条目。InputTag=%s WeaponTag=%s"),
					*InputTag.ToString(),
					*CurrentWeaponDefinition->WeaponTag.ToString());
			}
			else
			{
				FString SpiritEntryFailureReason;
				if (!ResolvedSpiritEntryConfig.IsValidConfig(SpiritEntryFailureReason))
				{
					UE_LOG(
						LogHeroCombatComponent,
						Warning,
						TEXT("[SpiritSkill] 输入未命中：Spirit 条目配置无效。InputTag=%s WeaponTag=%s Ability=%s 原因=%s"),
						*InputTag.ToString(),
						*CurrentWeaponDefinition->WeaponTag.ToString(),
						*GetNameSafe(ResolvedSpiritEntryConfig.AbilityToGrant),
						*SpiritEntryFailureReason);
				}
				else if (OwnerASC->GetActivatableAbilitySpecByTag(InputTag) == nullptr)
				{
					UE_LOG(
						LogHeroCombatComponent,
						Warning,
						TEXT("[SpiritSkill] 输入未命中：当前 Spirit 条目未正式授予到 ASC。InputTag=%s WeaponTag=%s Ability=%s EntryKind=%d"),
						*InputTag.ToString(),
						*CurrentWeaponDefinition->WeaponTag.ToString(),
						*GetNameSafe(ResolvedSpiritEntryConfig.AbilityToGrant),
						static_cast<int32>(ResolvedSpiritEntryConfig.EntryKind));
				}
				else
				{
					const FString NonAttackGateDebugText = DescribeNonAttackInputGateForDebug(InputTag);
					if (IsExpectedSpiritActivationGateBlock(NonAttackGateDebugText))
					{
						UE_LOG(
							LogHeroCombatComponent,
							Log,
							TEXT("[SpiritSkill] 输入已命中 AbilitySpec，但本次未通过激活裁决。InputTag=%s WeaponTag=%s 门禁=%s"),
							*InputTag.ToString(),
							*CurrentWeaponDefinition->WeaponTag.ToString(),
							*NonAttackGateDebugText);
					}
					else
					{
						UE_LOG(
							LogHeroCombatComponent,
							Warning,
							TEXT("[SpiritSkill] 输入已命中 AbilitySpec，但本次未通过激活裁决。InputTag=%s WeaponTag=%s 门禁=%s"),
							*InputTag.ToString(),
							*CurrentWeaponDefinition->WeaponTag.ToString(),
							*NonAttackGateDebugText);
					}
				}
			}
		}
	}

	if (ShouldQueueBufferedInput(InputTag, bCanBuffer))
	{
		QueueBufferedInput(InputTag, InputEvent, ResolvedAttackRequestTag);
	}

	return false;
}

bool UHeroCombatComponent::CanProcessCombatInput() const
{
	const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	return LoadoutStateComponent && LoadoutStateComponent->IsWeaponLoadoutStartupReady();
}

FString UHeroCombatComponent::DescribeNonAttackInputGateForDebug(const FGameplayTag& InputTag) const
{
	const FString InputTagText = HeroCombatInputTagToDebugText(InputTag);
	const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	if (!LoadoutStateComponent)
	{
		return FString::Printf(
            TEXT("Non-attack input gate: block, input=%s, layer=Startup, reason=LoadoutStateComponent is invalid."),
			*InputTagText);
	}

	if (!LoadoutStateComponent->IsWeaponLoadoutStartupReady())
	{
		return FString::Printf(
            TEXT("Non-attack input gate: block, input=%s, layer=Startup, state=%s, pending=%d, total=%d, failure=%s."),
			*InputTagText,
			*HeroCombatStartupStateToDebugText(LoadoutStateComponent->GetWeaponLoadoutStartupState()),
			LoadoutStateComponent->GetWeaponLoadoutStartupPendingSlotCount(),
			LoadoutStateComponent->GetWeaponLoadoutStartupTotalSlotCount(),
			LoadoutStateComponent->GetWeaponLoadoutStartupFailureReason().IsEmpty()
                ? TEXT("none")
				: *LoadoutStateComponent->GetWeaponLoadoutStartupFailureReason());
	}

	const UActionCombatReactComponent* CombatReactComponent = GetOwningCombatReactComponent();
	if (IsInBlockingCombatReactPrimaryPhase())
	{
		return FString::Printf(
			TEXT("Non-attack input gate blocked: input=%s layer=CombatReact reason=primary react phase active. %s"),
			*InputTagText,
			CombatReactComponent
				? *CombatReactComponent->DescribeCurrentCombatReactState()
                : TEXT("Combat react state: component not found."));
	}

	if (CombatReactComponent && CombatReactComponent->IsRecoveryPhaseActive())
	{
		if (!AbilityWindowRuntimeState.IsAbilityCancelWindowActive())
		{
			return FString::Printf(
				TEXT("Non-attack input gate blocked: input=%s layer=CombatReactRecovery reason=recovery phase active and cancel window closed. %s"),
				*InputTagText,
				*CombatReactComponent->DescribeCurrentCombatReactState());
		}

		if (!AbilityWindowRuntimeState.AcceptsCancelInput(InputTag))
		{
			return FString::Printf(
				TEXT("Non-attack input gate blocked: input=%s layer=CombatReactRecovery reason=recovery phase active and cancel whitelist rejected input. %s"),
				*InputTagText,
				*CombatReactComponent->DescribeCurrentCombatReactState());
		}
	}

	if (IsNonAttackInputBlockedByAirborneState())
	{
		return FString::Printf(
            TEXT("Non-attack input gate: block, input=%s, layer=Airborne, reason=character is in falling state."),
			*InputTagText);
	}

	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	const bool bIsWeaponSwitchInput = IsWeaponSwitchInputTag(InputTag)
		|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSwitch;

	if (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchTransactionInProgress())
	{
		return FString::Printf(
            TEXT("Non-attack input gate: block, input=%s, layer=WeaponSwitchTransaction, reason=weapon switch transaction is still active."),
			*InputTagText);
	}

	if (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchPresentationActive())
	{
		if (IsWeaponSwitchPresentationCancelInputAllowed(InputTag))
		{
			return FString::Printf(
				TEXT("Non-attack input gate: allow, input=%s, layer=WeaponSwitchCancelWindow, special=%s, reason=cancel window whitelist allows it during weapon switch presentation."),
				*InputTagText,
				*HeroCombatBoolToDebugText(WeaponSwitchComponent->IsSpecialWeaponSwitchPresentationActive()));
		}

		if (IsWeaponSwitchPresentationChainContextActive())
		{
			return FString::Printf(
				TEXT("Non-attack input gate: block, input=%s, layer=WeaponSwitchChainWindow, special=%s, reason=chain window is active but only attack can use it during weapon switch presentation."),
				*InputTagText,
				*HeroCombatBoolToDebugText(WeaponSwitchComponent->IsSpecialWeaponSwitchPresentationActive()));
		}

		return FString::Printf(
            TEXT("Non-attack input gate: block, input=%s, layer=WeaponSwitchPresentation, special=%s."),
			*InputTagText,
			*HeroCombatBoolToDebugText(WeaponSwitchComponent->IsSpecialWeaponSwitchPresentationActive()));
	}

	if (bIsWeaponSwitchInput && WindowRuntimeState.HasCombatLockState())
	{
		return FString::Printf(
            TEXT("Non-attack input gate: block, input=%s, layer=CombatLock, defense=%s, dodge=%s, parry=%s, perfectDodge=%s."),
			*InputTagText,
			*HeroCombatBoolToDebugText(WindowRuntimeState.IsDefenseActive()),
			*HeroCombatBoolToDebugText(WindowRuntimeState.IsDodgeActive()),
			*HeroCombatBoolToDebugText(WindowRuntimeState.IsParryWindowActive()),
			*HeroCombatBoolToDebugText(WindowRuntimeState.IsPerfectDodgeWindowActive()));
	}

	if (IsAbilityCancelContextActive())
	{
		const UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
		const bool bAttackAbilityActive =
			OwnerASC && OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Attack_Active);
		const bool bExecutionAbilityActive =
			OwnerASC && OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Execution_Active);

		if (!AbilityWindowRuntimeState.IsAbilityCancelWindowActive())
		{
			return FString::Printf(
                TEXT("Non-attack input gate: block, input=%s, layer=CancelContext, reason=active ability cancel context exists but cancel window is closed. attackActive=%s, executionActive=%s."),
				*InputTagText,
				*HeroCombatBoolToDebugText(bAttackAbilityActive),
				*HeroCombatBoolToDebugText(bExecutionAbilityActive));
		}

		if (!IsAbilityCancelInputAllowedNow(InputTag))
		{
			return FString::Printf(
                TEXT("Non-attack input gate: block, input=%s, layer=CancelWindow, windowOpen=%s, whitelistHit=%s."),
				*InputTagText,
				*HeroCombatBoolToDebugText(AbilityWindowRuntimeState.IsAbilityCancelWindowActive()),
				*HeroCombatBoolToDebugText(AbilityWindowRuntimeState.AcceptsCancelInput(InputTag)));
		}

		if (bIsWeaponSwitchInput && WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchBlockedByCooldown())
		{
			return FString::Printf(
                TEXT("Non-attack input gate: block, input=%s, layer=WeaponSwitchCooldown, reason=weapon switch cooldown is active."),
				*InputTagText);
		}

		if (CombatReactComponent && CombatReactComponent->IsRecoveryPhaseActive())
		{
			return FString::Printf(
                TEXT("Non-attack input gate: allow, input=%s, layer=CombatReactRecoveryCancelWindow, reason=recovery cancel whitelist allows it."),
				*InputTagText);
		}

		return FString::Printf(
            TEXT("Non-attack input gate: allow, input=%s, layer=CancelWindow, reason=cancel window whitelist allows it."),
			*InputTagText);
	}

	if (!IsAttackEnabled())
	{
		return FString::Printf(
            TEXT("Non-attack input gate: block, input=%s, layer=CombatRuntime, reason=AttackEnabled is false."),
			*InputTagText);
	}

	if (bIsWeaponSwitchInput && WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchBlockedByCooldown())
	{
		return FString::Printf(
            TEXT("Non-attack input gate: block, input=%s, layer=WeaponSwitchCooldown, reason=weapon switch cooldown is active."),
			*InputTagText);
	}

	return FString::Printf(
        TEXT("Non-attack input gate: allow, input=%s, layer=Ready, startupReady=yes, combatReactBlocked=no, airborneBlocked=no, weaponSwitchPresentation=no, weaponSwitchTransaction=no."),
		*InputTagText);
}

bool UHeroCombatComponent::HasAnyAuthoritativeActiveCombatAbility() const
{
	const UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!OwnerASC)
	{
		return false;
	}

	return OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Attack_Active)
		|| OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Defense_Active)
		|| OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Dodge_Active)
		|| OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Execution_Active)
		|| OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_SpiritSkill_Active)
		|| OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_WeaponSwitch_Active);
}

bool UHeroCombatComponent::HasActiveCombatModeTransitionRuntime() const
{
	return bCombatModeTransitionActive || ActiveCombatModeTransitionMontage != nullptr;
}

void UHeroCombatComponent::RepairStaleCombatRuntimeIfNeeded()
{
	const UActionCombatReactComponent* CombatReactComponent = GetOwningCombatReactComponent();
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	const bool bHasCombatReactLock = CombatReactComponent && CombatReactComponent->IsCombatReactActive();
	const bool bHasWeaponSwitchLock = WeaponSwitchComponent
		&& (WeaponSwitchComponent->IsWeaponSwitchPresentationActive()
			|| WeaponSwitchComponent->IsWeaponSwitchTransactionInProgress());
	const bool bHasCombatModeTransitionLock = HasActiveCombatModeTransitionRuntime();
	if (HasAnyAuthoritativeActiveCombatAbility()
		|| bHasCombatReactLock
		|| bHasWeaponSwitchLock
		|| bHasCombatModeTransitionLock)
	{
		return;
	}

	const bool bAttackDisabledBeforeRepair = !IsAttackEnabled();
	const bool bAbilityChainWindowActiveBeforeRepair = AbilityWindowRuntimeState.IsAbilityChainWindowActive();
	const bool bAbilityCancelWindowActiveBeforeRepair = AbilityWindowRuntimeState.IsAbilityCancelWindowActive();
	const bool bDefenseActiveBeforeRepair = WindowRuntimeState.IsDefenseActive();
	const bool bParryWindowActiveBeforeRepair = WindowRuntimeState.IsParryWindowActive();
	const bool bDodgeActiveBeforeRepair = WindowRuntimeState.IsDodgeActive();
	const bool bPerfectDodgeWindowActiveBeforeRepair = WindowRuntimeState.IsPerfectDodgeWindowActive();
	const bool bDefenseCombatModeBeforeRepair = GetCombatMode() == EHeroCombatMode::Defense;
	const bool bRunningMontageActiveBeforeRepair = GetCurrentRunningAnimMontage() != nullptr;
	const EHeroCombatMode CombatModeBeforeRepair = GetCombatMode();
	UAnimMontage* RunningMontageBeforeRepair = GetCurrentRunningAnimMontage();

	bool bRepaired = false;

	if (bAttackDisabledBeforeRepair)
	{
		SetAttackEnabled(true);
		bRepaired = true;
	}

	if (bAbilityChainWindowActiveBeforeRepair)
	{
		CloseAbilityChainWindow();
		bRepaired = true;
	}

	if (bAbilityCancelWindowActiveBeforeRepair)
	{
		CloseAbilityCancelWindow();
		bRepaired = true;
	}

	if (bDefenseActiveBeforeRepair)
	{
		WindowRuntimeState.SetDefenseActive(false);
		bRepaired = true;
	}

	if (bParryWindowActiveBeforeRepair)
	{
		WindowRuntimeState.SetParryWindowActive(false);
		bRepaired = true;
	}

	if (bDodgeActiveBeforeRepair)
	{
		WindowRuntimeState.SetDodgeActive(false);
		bRepaired = true;
	}

	if (bPerfectDodgeWindowActiveBeforeRepair)
	{
		WindowRuntimeState.SetPerfectDodgeWindowActive(false);
		bRepaired = true;
	}

	if (bDefenseCombatModeBeforeRepair)
	{
		SetCombatMode(EHeroCombatMode::Combo);
		bRepaired = true;
	}

	if (bRunningMontageActiveBeforeRepair)
	{
		ClearRunningAnimMontageReference();
		bRepaired = true;
	}

	if (!bRepaired)
	{
		return;
	}

    Debug::Print(TEXT("[Combat] Auto repaired lingering combat runtime state."), FColor::Yellow, 2.0f);
	UE_LOG(
		LogHeroCombatComponent,
		Warning,
		TEXT("Detected lingering public combat runtime state without authoritative combat lock; auto repaired. Before: AttackDisabled=%d ChainWindow=%d CancelWindow=%d Defense=%d Parry=%d Dodge=%d PerfectDodge=%d DefenseMode=%d RunningMontage=%s CombatMode=%d After: AttackEnabled=%d CombatMode=%d Defense=%d Dodge=%d"),
		bAttackDisabledBeforeRepair ? 1 : 0,
		bAbilityChainWindowActiveBeforeRepair ? 1 : 0,
		bAbilityCancelWindowActiveBeforeRepair ? 1 : 0,
		bDefenseActiveBeforeRepair ? 1 : 0,
		bParryWindowActiveBeforeRepair ? 1 : 0,
		bDodgeActiveBeforeRepair ? 1 : 0,
		bPerfectDodgeWindowActiveBeforeRepair ? 1 : 0,
		bDefenseCombatModeBeforeRepair ? 1 : 0,
		*GetNameSafe(RunningMontageBeforeRepair),
		static_cast<int32>(CombatModeBeforeRepair),
		IsAttackEnabled() ? 1 : 0,
		static_cast<int32>(GetCombatMode()),
		WindowRuntimeState.IsDefenseActive() ? 1 : 0,
		WindowRuntimeState.IsDodgeActive() ? 1 : 0);
}

UActionCombatReactComponent* UHeroCombatComponent::GetOwningCombatReactComponent() const
{
	if (const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetOwner()))
	{
		return OwnerCharacter->GetActionCombatReactComponent();
	}

	return nullptr;
}

bool UHeroCombatComponent::IsInBlockingCombatReactPrimaryPhase() const
{
	if (const UActionCombatReactComponent* CombatReactComponent = GetOwningCombatReactComponent())
	{
		return CombatReactComponent->IsPrimaryReactPhaseActive()
			|| CombatReactComponent->IsAirborneUncontrolledActive();
	}

	return false;
}

bool UHeroCombatComponent::IsCombatReactRecoveryCancelInputAllowed(const FGameplayTag InputTag) const
{
	if (const UActionCombatReactComponent* CombatReactComponent = GetOwningCombatReactComponent())
	{
		return CombatReactComponent->IsRecoveryPhaseActive()
			&& InputTag.IsValid()
			&& AbilityWindowRuntimeState.IsAbilityCancelWindowActive()
			&& AbilityWindowRuntimeState.AcceptsCancelInput(InputTag);
	}

	return false;
}

bool UHeroCombatComponent::IsInBlockingCombatReactRecoveryPhase(const FGameplayTag InputTag) const
{
	if (const UActionCombatReactComponent* CombatReactComponent = GetOwningCombatReactComponent())
	{
		return CombatReactComponent->IsRecoveryPhaseActive()
			&& !IsCombatReactRecoveryCancelInputAllowed(InputTag);
	}

	return false;
}

bool UHeroCombatComponent::IsAttackInputBlockedByCombatReact(const FGameplayTag InputTag) const
{
	return IsInBlockingCombatReactPrimaryPhase()
		|| IsInBlockingCombatReactRecoveryPhase(InputTag);
}

bool UHeroCombatComponent::IsNonAttackInputBlockedByCombatReact(const FGameplayTag InputTag) const
{
	return IsInBlockingCombatReactPrimaryPhase()
		|| IsInBlockingCombatReactRecoveryPhase(InputTag);
}

bool UHeroCombatComponent::IsNonAttackInputBlockedByAirborneState() const
{
	return IsInNormalFallingState();
}

bool UHeroCombatComponent::IsInNormalFallingState() const
{
	const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter();
	const UCharacterMovementComponent* MovementComponent = HeroCharacter
		? HeroCharacter->GetCharacterMovement()
		: nullptr;
	return MovementComponent && MovementComponent->IsFalling();
}

void UHeroCombatComponent::HandlePressedInputState(FGameplayTag InputTag)
{
	ProcessAbilityInput(InputTag, EActionInputEvent::Pressed, true);
}

void UHeroCombatComponent::HandleHeldInputState(FGameplayTag InputTag)
{
	ProcessAbilityInput(InputTag, EActionInputEvent::Held, true);
}

void UHeroCombatComponent::HandleReleasedInputState(FGameplayTag InputTag)
{
	ProcessAbilityInput(InputTag, EActionInputEvent::Released, true);
}

bool UHeroCombatComponent::ApplyComponentCombatModifierEffect(
	FActiveGameplayEffectHandle& InOutEffectHandle,
	const FActionCombatModifierEffectSpec& EffectTemplate,
	const float OverrideDuration)
{
	RemoveComponentCombatModifierEffect(InOutEffectHandle);

	FActionCombatModifierEffectSpec RuntimeEffectSpec = EffectTemplate;
	if (OverrideDuration > 0.f)
	{
		RuntimeEffectSpec.Duration = OverrideDuration;
	}

	if (!RuntimeEffectSpec.IsValidSpec())
	{
		return false;
	}

	UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent();
	if (!ActionAbilitySystemComponent)
	{
		return false;
	}

	InOutEffectHandle = ActionAbilitySystemComponent->ApplyCombatModifierEffect(RuntimeEffectSpec);
	return InOutEffectHandle.IsValid();
}

void UHeroCombatComponent::RemoveComponentCombatModifierEffect(FActiveGameplayEffectHandle& InOutEffectHandle)
{
	if (!InOutEffectHandle.IsValid())
	{
		return;
	}

	if (UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent())
	{
		ActionAbilitySystemComponent->RemoveActiveGameplayEffect(InOutEffectHandle);
	}

	InOutEffectHandle.Invalidate();
}

void UHeroCombatComponent::ResetCombatStateForWeaponSwitch()
{
	StopActiveCombatModeTransitionMontage(false);

	if (UHeroDefenseComponent* DefenseComponent = GetOwningHeroDefenseComponent())
	{
		DefenseComponent->ClearDodgeCounterAvailability();
	}

	ResetCombatInputRecoveryRuntime();
	ResetOwnedCombatSubcomponentRuntimeStates();
	if (UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		WeaponSwitchComponent->ClearDeferredQueuedWeaponSwitchConsumeRequest();
	}
	ResetAllSpiritSkillComboStates();
	ResetAttackPresentationRuntime(true);
}


void UHeroCombatComponent::RefreshCurrentWeaponComboLimit()
{
	if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CurrentEquippedWeaponDefinition)
	{
		const FGameplayTag PrimaryComboBranchTag =
			CurrentWeaponDefinition->ResolveAttackBranchTag(ActionGameplayTags::Attack_Request_Default);
		UpdateComboMaxIndex(FMath::Max(CurrentWeaponDefinition->GetAttackMontageCountForBranch(PrimaryComboBranchTag), 0));
		return;
	}

	UpdateComboMaxIndex(0);
}

void UHeroCombatComponent::ApplyEquippedWeaponState(const FHeroEquippedWeaponState& InWeaponState)
{
	const bool bPreviousShouldUseWeaponLinkedLayer = bHasEquippedWeaponLinkedLayerPresentation;
	const TSubclassOf<UActionHeroLinkedAnimLayer> PreviousLinkedLayerClass = CurrentWeaponLinkedAnimLayerClass;
	const bool bWasSameLoggedEquippedState = bHasLoggedEquippedWeaponState
		&& LastLoggedEquippedWeaponState.MatchesEquippedState(InWeaponState);
	const bool bWasSameLoggedLinkedLayerState = bHasLoggedEquippedWeaponState
		&& bLastLoggedWeaponLinkedLayerPresentation == bHasEquippedWeaponLinkedLayerPresentation
		&& LastLoggedWeaponLinkedAnimLayerClass == CurrentWeaponLinkedAnimLayerClass;

	CurrentEquippedWeaponInstance = InWeaponState.EquippedWeaponInstance;
	CurrentEquippedWeaponDefinition = InWeaponState.EquippedWeaponDefinition;
	if (UHeroAttackComponent* AttackComponent = GetOwningHeroAttackComponent())
	{
		AttackComponent->ClearCurrentAttackHitConfig();
	}
	SetCurrentEquippedWeaponTag(InWeaponState.EquippedWeaponTag);
	SetCurrentEquippedWeaponCategory(InWeaponState.EquippedWeaponCategory);
	bHasEquippedWeaponLinkedLayerPresentation = ResolveWeaponLinkedLayerPresentationState(
		CurrentEquippedWeaponDefinition,
		&CurrentWeaponLinkedAnimLayerClass);
	RefreshCurrentWeaponComboLimit();

	if (CurrentEquippedWeaponDefinition != nullptr
		&& CurrentEquippedWeaponCategory != EHeroWeaponCategory::Unarmed
		&& !CurrentWeaponLinkedAnimLayerClass)
	{
		UE_LOG(
			LogHeroCombatComponent,
			Warning,
			TEXT("weapon_linked_layer_invalid_definition slot=%d weapon_tag=%s weapon_category=%d linked_layer=None should_use=false"),
			static_cast<int32>(InWeaponState.EquippedLoadoutSlot),
			*InWeaponState.EquippedWeaponTag.ToString(),
			static_cast<int32>(InWeaponState.EquippedWeaponCategory));
	}

	if (bPreviousShouldUseWeaponLinkedLayer != bHasEquippedWeaponLinkedLayerPresentation
		|| PreviousLinkedLayerClass != CurrentWeaponLinkedAnimLayerClass)
	{
		UE_LOG(
			LogHeroCombatComponent,
			Log,
			TEXT("weapon_linked_layer_switch_state_changed slot=%d weapon_tag=%s weapon_category=%d linked_layer=%s should_use=%s previous_should_use=%s previous_linked_layer=%s"),
			static_cast<int32>(InWeaponState.EquippedLoadoutSlot),
			*InWeaponState.EquippedWeaponTag.ToString(),
			static_cast<int32>(InWeaponState.EquippedWeaponCategory),
			*GetNameSafe(CurrentWeaponLinkedAnimLayerClass),
			bHasEquippedWeaponLinkedLayerPresentation ? TEXT("true") : TEXT("false"),
			bPreviousShouldUseWeaponLinkedLayer ? TEXT("true") : TEXT("false"),
			*GetNameSafe(PreviousLinkedLayerClass));
	}

	const bool bLinkedLayerStateChanged =
		bPreviousShouldUseWeaponLinkedLayer != bHasEquippedWeaponLinkedLayerPresentation
		|| PreviousLinkedLayerClass != CurrentWeaponLinkedAnimLayerClass;
	const bool bShouldLogAppliedState = !bWasSameLoggedEquippedState
		|| !bWasSameLoggedLinkedLayerState
		|| bLinkedLayerStateChanged;
	if (bShouldLogAppliedState)
	{
		bHasLoggedEquippedWeaponState = true;
		LastLoggedEquippedWeaponState = InWeaponState;
		bLastLoggedWeaponLinkedLayerPresentation = bHasEquippedWeaponLinkedLayerPresentation;
		LastLoggedWeaponLinkedAnimLayerClass = CurrentWeaponLinkedAnimLayerClass;

		UE_LOG(
			LogHeroCombatComponent,
			Log,
			TEXT("equipped_weapon_state_applied slot=%d weapon_tag=%s weapon_category=%d combat_mode=%d should_use_weapon_layer=%s linked_layer=%s running_montage=%s"),
			static_cast<int32>(InWeaponState.EquippedLoadoutSlot),
			*InWeaponState.EquippedWeaponTag.ToString(),
			static_cast<int32>(InWeaponState.EquippedWeaponCategory),
			static_cast<int32>(GetCombatMode()),
			bHasEquippedWeaponLinkedLayerPresentation ? TEXT("true") : TEXT("false"),
			*GetNameSafe(CurrentWeaponLinkedAnimLayerClass),
			*GetNameSafe(GetCurrentRunningAnimMontage()));
	}

	if (CurrentEquippedWeaponInstance)
	{
		if (GetCombatMode() == EHeroCombatMode::Idle)
		{
			RequestCurrentEquippedWeaponPresentationToHolster();
		}
		else
		{
			RequestCurrentEquippedWeaponPresentationToCombatSocket();
		}
	}
}

bool UHeroCombatComponent::IsEquippedWeaponStateMeaningfulForCombatCache(
	const FHeroEquippedWeaponState& InWeaponState) const
{
	return InWeaponState.EquippedWeaponDefinition
		|| InWeaponState.EquippedWeaponTag != ActionGameplayTags::Player_Weapon_Unarmed_Default
		|| InWeaponState.EquippedLoadoutSlot == EHeroWeaponLoadoutSlot::Unarmed;
}

bool UHeroCombatComponent::IsCombatCacheAlreadySynchronizedToEquippedState(
	const FHeroEquippedWeaponState& InWeaponState) const
{
	return CurrentEquippedWeaponInstance == InWeaponState.EquippedWeaponInstance
		&& CurrentEquippedWeaponDefinition == InWeaponState.EquippedWeaponDefinition
		&& GetCurrentEquippedWeaponTag() == InWeaponState.EquippedWeaponTag
		&& GetCurrentEquippedWeaponCategory() == InWeaponState.EquippedWeaponCategory;
}

void UHeroCombatComponent::EnsureCurrentWeaponStateSynchronizedFromEquipment() const
{
	if (const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		const FHeroEquippedWeaponState EquippedState = EquipmentComponent->GetCurrentEquippedWeaponState();
		if (IsEquippedWeaponStateMeaningfulForCombatCache(EquippedState))
		{
			if (IsCombatCacheAlreadySynchronizedToEquippedState(EquippedState))
			{
				return;
			}

			const_cast<UHeroCombatComponent*>(this)->ApplyEquippedWeaponState(EquippedState);
		}
	}
}

bool UHeroCombatComponent::ResolveWeaponLinkedLayerPresentationState(
	const UDataAsset_WeaponDefinition* InWeaponDefinition,
	TSubclassOf<UActionHeroLinkedAnimLayer>* OutLinkedAnimLayerClass) const
{
	TSubclassOf<UActionHeroLinkedAnimLayer> ResolvedLinkedAnimLayerClass = nullptr;
	if (InWeaponDefinition != nullptr)
	{
		ResolvedLinkedAnimLayerClass = InWeaponDefinition->GetLinkedAnimLayerClass();
	}

	if (OutLinkedAnimLayerClass != nullptr)
	{
		*OutLinkedAnimLayerClass = ResolvedLinkedAnimLayerClass;
	}

	return InWeaponDefinition != nullptr
		&& InWeaponDefinition->GetWeaponCategory() != EHeroWeaponCategory::Unarmed
		&& ResolvedLinkedAnimLayerClass != nullptr;
}

void UHeroCombatComponent::InitializeCurrentWeaponStateFromEquipment()
{
	UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent();
	if (!WeaponSwitchComponent)
	{
		return;
	}

	WeaponSwitchComponent->InitializeCurrentWeaponStateFromEquipment();
}

void UHeroCombatComponent::ResetRuntimeStateForHeroStartup()
{
	if (UHeroCombatInputComponent* InputComponent = GetOwningHeroCombatInputComponent())
	{
		InputComponent->ResetRuntimeStateForHeroStartup();
	}
	if (UHeroTargetingComponent* TargetingComponent = GetOwningHeroTargetingComponent())
	{
		TargetingComponent->ResetRuntimeStateForHeroStartup();
	}
	ResetCombatInputRecoveryRuntime();

	if (UHeroWeaponSwitchComponent* WeaponSwitchComponent = GetOwningHeroWeaponSwitchComponent())
	{
		WeaponSwitchComponent->ResetRuntimeStateForHeroStartup();
	}
	ResetOwnedCombatSubcomponentRuntimeStates();
	ResetAllSpiritSkillComboStates();
	bHasLoggedEquippedWeaponState = false;
	LastLoggedEquippedWeaponState.ResetToUnarmed();
	bLastLoggedWeaponLinkedLayerPresentation = false;
	LastLoggedWeaponLinkedAnimLayerClass = nullptr;

	ResetAttackPresentationRuntime(false);
	ClearActiveDamageContext();
	bLastWeaponSwitchCooldownBlocked = IsWeaponSwitchBlockedByCooldown();
	BroadcastCombatUIStateChanged();
}

void UHeroCombatComponent::AddSpecialWeaponSwitchEnergy(float DeltaEnergy) const
{
	if (DeltaEnergy <= 0.f)
	{
		return;
	}

	if (UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		EquipmentComponent->AddSpecialWeaponSwitchEnergy(DeltaEnergy);
	}
}
