#include "Components/Combat/PawnCombatComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const TCHAR* LexToStringRootMotionMode(const ERootMotionMode::Type RootMotionMode)
	{
		switch (RootMotionMode)
		{
		case ERootMotionMode::NoRootMotionExtraction:
			return TEXT("NoRootMotionExtraction");
		case ERootMotionMode::IgnoreRootMotion:
			return TEXT("IgnoreRootMotion");
		case ERootMotionMode::RootMotionFromEverything:
			return TEXT("RootMotionFromEverything");
		case ERootMotionMode::RootMotionFromMontagesOnly:
			return TEXT("RootMotionFromMontagesOnly");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* LexToStringMovementMode(const EMovementMode MovementMode)
	{
		switch (MovementMode)
		{
		case MOVE_None:
			return TEXT("MOVE_None");
		case MOVE_Walking:
			return TEXT("MOVE_Walking");
		case MOVE_NavWalking:
			return TEXT("MOVE_NavWalking");
		case MOVE_Falling:
			return TEXT("MOVE_Falling");
		case MOVE_Swimming:
			return TEXT("MOVE_Swimming");
		case MOVE_Flying:
			return TEXT("MOVE_Flying");
		case MOVE_Custom:
			return TEXT("MOVE_Custom");
		default:
			return TEXT("MOVE_Unknown");
		}
	}
}

UPawnCombatComponent::UPawnCombatComponent()
	: Super()
{
}

bool UPawnCombatComponent::HandleIncomingCombatEvent(FGameplayTag InCombatEventTag, AActor* InstigatorActor)
{
	return false;
}

bool UPawnCombatComponent::TryHandleIncomingDamage(const FActionDamagePayload& InDamagePayload, FActionHitResolveResult& OutResult)
{
	OutResult = FActionHitResolveResult();
	return false;
}

void UPawnCombatComponent::SetCurrentEquippedWeaponTag(const FGameplayTag InWeaponTag)
{
	CurrentEquippedWeaponTag = InWeaponTag;
}

FGameplayTag UPawnCombatComponent::GetCurrentEquippedWeaponTag() const
{
	return CurrentEquippedWeaponTag;
}

UAnimMontage* UPawnCombatComponent::GetCurrentRunningAnimMontage() const
{
	return CurrentRunningAnimMontage;
}

UAnimMontage* UPawnCombatComponent::GetCombatModeTransitionAnimMontage()
{
	return nullptr;
}

void UPawnCombatComponent::UpdateRunningAnimMontage(UAnimMontage* InNewMontage)
{
	// 切换运行蒙太奇前先停掉上一段攻击蒙太奇，避免状态残留。
	StopCurrentAttackMontage();

	if (InNewMontage)
	{
		CurrentRunningAnimMontage = InNewMontage;
	}
}

void UPawnCombatComponent::ClearRunningAnimMontageReference()
{
	// 这里只清本地缓存引用，不主动停止 ASC 上已经进入自然收尾的蒙太奇。
	// 用途是让“运行中蒙太奇引用”与 Ability 运行态保持一致，避免动画已结束但引用还残留。
	CurrentRunningAnimMontage = nullptr;
	ClearRunningAnimationReactGuardContext();
}

bool UPawnCombatComponent::ClearRunningAnimMontageReferenceIfMatches(const UAnimMontage* InMontage)
{
	if (!InMontage || CurrentRunningAnimMontage != InMontage)
	{
		return false;
	}

	ClearRunningAnimMontageReference();
	return true;
}

void UPawnCombatComponent::StopCurrentAttackMontage()
{
	UAbilitySystemComponent* OwnerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (!OwnerASC || !GetCurrentRunningAnimMontage())
	{
		CurrentRunningAnimMontage = nullptr;
		ClearRunningAnimationReactGuardContext();
		return;
	}

	OwnerASC->StopMontageIfCurrent(*GetCurrentRunningAnimMontage());
	CurrentRunningAnimMontage = nullptr;
	ClearRunningAnimationReactGuardContext();
}

void UPawnCombatComponent::SetRunningAnimationReactGuardContext(
	UAnimMontage* InMontage,
	const EActionRunningAnimationSemantic InSemantic,
	const int32 InMinIncomingReactPriorityToInterrupt)
{
	RunningAnimationReactGuardContext.Montage = InMontage;
	RunningAnimationReactGuardContext.Semantic = InSemantic;
	RunningAnimationReactGuardContext.MinIncomingReactPriorityToInterrupt =
		FMath::Max(InMinIncomingReactPriorityToInterrupt, 0);
}

void UPawnCombatComponent::ClearRunningAnimationReactGuardContext()
{
	RunningAnimationReactGuardContext = FActionRunningAnimationReactGuardContext();
}

bool UPawnCombatComponent::ClearRunningAnimationReactGuardContextIfMatches(
	const UAnimMontage* InMontage,
	const EActionRunningAnimationSemantic InSemantic)
{
	if (!InMontage || RunningAnimationReactGuardContext.Montage != InMontage)
	{
		return false;
	}

	if (InSemantic != EActionRunningAnimationSemantic::None
		&& RunningAnimationReactGuardContext.Semantic != InSemantic)
	{
		return false;
	}

	ClearRunningAnimationReactGuardContext();
	return true;
}

const FActionRunningAnimationReactGuardContext& UPawnCombatComponent::GetRunningAnimationReactGuardContext() const
{
	return RunningAnimationReactGuardContext;
}

bool UPawnCombatComponent::IsCurrentRunningAnimationProtectedFromIncomingReact(const int32 IncomingPriority) const
{
	return RunningAnimationReactGuardContext.IsValid()
		&& RunningAnimationReactGuardContext.Semantic == EActionRunningAnimationSemantic::NonReact
		&& IncomingPriority <= RunningAnimationReactGuardContext.MinIncomingReactPriorityToInterrupt;
}

bool UPawnCombatComponent::BeginMontageRootMotionOverride(
	ACharacter* Character,
	UAnimMontage* Montage,
	const FName Reason)
{
	if (!Character || !Montage)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[PawnCombat] RootMotionOverride begin failed owner=%s character=%s montage=%s reason=%s failure=invalid_input"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Character),
			*GetNameSafe(Montage),
			*Reason.ToString());
		return false;
	}

	USkeletalMeshComponent* MeshComponent = Character->GetMesh();
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[PawnCombat] RootMotionOverride begin failed owner=%s character=%s montage=%s reason=%s failure=missing_anim_instance"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Character),
			*GetNameSafe(Montage),
			*Reason.ToString());
		return false;
	}

	if (MontageRootMotionOverrideRuntime.bActive)
	{
		EndMontageRootMotionOverride(
			MontageRootMotionOverrideRuntime.Character.Get(),
			MontageRootMotionOverrideRuntime.Montage.Get(),
			MontageRootMotionOverrideRuntime.Reason);
	}

	const ERootMotionMode::Type PreviousRootMotionMode = AnimInstance->RootMotionMode;
	MontageRootMotionOverrideRuntime.Character = Character;
	MontageRootMotionOverrideRuntime.Montage = Montage;
	MontageRootMotionOverrideRuntime.PreviousRootMotionMode = PreviousRootMotionMode;
	MontageRootMotionOverrideRuntime.Reason = Reason;
	MontageRootMotionOverrideRuntime.bActive = true;
	AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);

	const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	const EMovementMode MovementMode = MovementComponent ? MovementComponent->MovementMode.GetValue() : MOVE_None;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[PawnCombat] RootMotionOverride begin owner=%s character=%s montage=%s reason=%s previous_root_motion_mode=%s new_root_motion_mode=%s montage_has_root_motion=%s movement_mode=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Character),
		*GetNameSafe(Montage),
		*Reason.ToString(),
		LexToStringRootMotionMode(PreviousRootMotionMode),
		LexToStringRootMotionMode(AnimInstance->RootMotionMode),
		Montage->HasRootMotion() ? TEXT("true") : TEXT("false"),
		LexToStringMovementMode(MovementMode));

	if (!Montage->HasRootMotion())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[PawnCombat] RootMotionOverride asset warning owner=%s montage=%s reason=%s failure=montage_has_no_root_motion check=EnableRootMotion/RootBone/Retarget"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Montage),
			*Reason.ToString());
	}

	if (MovementMode == MOVE_None)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[PawnCombat] RootMotionOverride movement warning owner=%s character=%s montage=%s reason=%s failure=movement_mode_none check=ANS_SetMoveMode_or_external_disable_movement"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Character),
			*GetNameSafe(Montage),
			*Reason.ToString());
	}

	return true;
}

void UPawnCombatComponent::EndMontageRootMotionOverride(
	ACharacter* Character,
	UAnimMontage* Montage,
	const FName Reason)
{
	if (!MontageRootMotionOverrideRuntime.bActive)
	{
		return;
	}

	ACharacter* RuntimeCharacter = MontageRootMotionOverrideRuntime.Character.Get();
	UAnimMontage* RuntimeMontage = MontageRootMotionOverrideRuntime.Montage.Get();
	if (Character && RuntimeCharacter && Character != RuntimeCharacter)
	{
		return;
	}

	if (Montage && RuntimeMontage && Montage != RuntimeMontage)
	{
		return;
	}

	if (RuntimeCharacter)
	{
		if (USkeletalMeshComponent* MeshComponent = RuntimeCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
			{
				const ERootMotionMode::Type CurrentRootMotionMode = AnimInstance->RootMotionMode;
				AnimInstance->SetRootMotionMode(MontageRootMotionOverrideRuntime.PreviousRootMotionMode);
				UE_LOG(
					LogTemp,
					Log,
					TEXT("[PawnCombat] RootMotionOverride end owner=%s character=%s montage=%s reason=%s previous_root_motion_mode=%s restored_root_motion_mode=%s"),
					*GetNameSafe(GetOwner()),
					*GetNameSafe(RuntimeCharacter),
					*GetNameSafe(RuntimeMontage),
					*Reason.ToString(),
					LexToStringRootMotionMode(CurrentRootMotionMode),
					LexToStringRootMotionMode(AnimInstance->RootMotionMode));
			}
		}
	}

	MontageRootMotionOverrideRuntime.Reset();
}

int32 UPawnCombatComponent::GetComboIndex() const
{
	return ComboIndex;
}

void UPawnCombatComponent::AdvanceComboIndex(const int32 InComboLength)
{
	// 攻击分支真正可用的蒙太奇数量由武器数据决定，
	// 基类这里只负责按外部传入的数量推进索引。
	ComboMaxIndex = FMath::Max(InComboLength, 0);

	if (ComboMaxIndex <= 0)
	{
		ComboIndex = 0;
		return;
	}

	ComboIndex = (ComboIndex + 1) % ComboMaxIndex;
}

void UPawnCombatComponent::UpdateComboMaxIndex(const int32 Num)
{
	ComboMaxIndex = FMath::Max(Num, 0);
}

void UPawnCombatComponent::ResetComboIndex()
{
	ComboIndex = 0;
}

void UPawnCombatComponent::SetAttackEnabled(const bool bInAttackEnabled)
{
	bAttackEnabled = bInAttackEnabled;
}

bool UPawnCombatComponent::IsAttackEnabled() const
{
	return bAttackEnabled;
}

void UPawnCombatComponent::SetCombatMode(const EHeroCombatMode InCombatMode)
{
	CombatMode = InCombatMode;
}

EHeroCombatMode UPawnCombatComponent::GetCombatMode() const
{
	return CombatMode;
}

void UPawnCombatComponent::SetCurrentEquippedWeaponCategory(const EHeroWeaponCategory InWeaponCategory)
{
	CurrentEquippedWeaponCategory = InWeaponCategory;
}

EHeroWeaponCategory UPawnCombatComponent::GetCurrentEquippedWeaponCategory() const
{
	return CurrentEquippedWeaponCategory;
}
