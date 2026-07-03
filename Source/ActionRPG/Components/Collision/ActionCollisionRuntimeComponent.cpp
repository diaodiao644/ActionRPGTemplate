#include "Components/Collision/ActionCollisionRuntimeComponent.h"

#include "Components/PrimitiveComponent.h"

namespace
{
	const TCHAR* LexToStringActionCollisionSlot(const EActionCollisionSlot InSlot)
	{
		switch (InSlot)
		{
		case EActionCollisionSlot::CharacterCapsule:
			return TEXT("CharacterCapsule");
		case EActionCollisionSlot::CharacterMesh:
			return TEXT("CharacterMesh");
		case EActionCollisionSlot::WeaponHit:
			return TEXT("WeaponHit");
		case EActionCollisionSlot::OwnedBodyHit:
			return TEXT("OwnedBodyHit");
		case EActionCollisionSlot::ProjectileCollision:
			return TEXT("ProjectileCollision");
		case EActionCollisionSlot::Custom:
		default:
			break;
		}

		return TEXT("Custom");
	}

	const TCHAR* LexToStringActionCollisionPreset(const EActionCollisionPreset InPreset)
	{
		switch (InPreset)
		{
		case EActionCollisionPreset::Default:
			return TEXT("Default");
		case EActionCollisionPreset::Disabled:
			return TEXT("Disabled");
		case EActionCollisionPreset::HitQueryPawnOverlap:
			return TEXT("HitQueryPawnOverlap");
		case EActionCollisionPreset::ProjectilePawnOverlap:
			return TEXT("ProjectilePawnOverlap");
		case EActionCollisionPreset::ExecutionVictimPawnPassThrough:
			return TEXT("ExecutionVictimPawnPassThrough");
		default:
			break;
		}

		return TEXT("Unknown");
	}
}

UActionCollisionRuntimeComponent::UActionCollisionRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UActionCollisionRuntimeComponent::RegisterCollisionSlot(
	const EActionCollisionSlot InSlot,
	UPrimitiveComponent* InComponent,
	FName InRegistrationName)
{
	if (!IsValid(InComponent))
	{
		return false;
	}

	if (InRegistrationName == NAME_None)
	{
		InRegistrationName = InComponent->GetFName();
	}

	for (TPair<EActionCollisionSlot, TArray<FRegisteredCollisionComponentRuntime>>& Pair : RegisteredCollisionSlots)
	{
		for (FRegisteredCollisionComponentRuntime& ExistingRuntime : Pair.Value)
		{
			if (ExistingRuntime.Component.Get() == InComponent)
			{
				if (ExistingRuntime.Slot == InSlot && ExistingRuntime.RegistrationName == InRegistrationName)
				{
					return true;
				}

				ExistingRuntime.Slot = InSlot;
				ExistingRuntime.RegistrationName = InRegistrationName;
				ExistingRuntime.DefaultSnapshot = CaptureSnapshot(InComponent);
				RecomputeCollisionSlot(InSlot);
				return true;
			}
		}
	}

	FRegisteredCollisionComponentRuntime RegisteredRuntime;
	RegisteredRuntime.Component = InComponent;
	RegisteredRuntime.Slot = InSlot;
	RegisteredRuntime.RegistrationName = InRegistrationName;
	RegisteredRuntime.DefaultSnapshot = CaptureSnapshot(InComponent);
	RegisteredCollisionSlots.FindOrAdd(InSlot).Add(MoveTemp(RegisteredRuntime));
	RecomputeCollisionSlot(InSlot);
	return true;
}

FActionCollisionOverrideHandle UActionCollisionRuntimeComponent::AcquireCollisionOverride(
	const FActionCollisionOverrideRequest& InRequest)
{
	FActionCollisionOverrideHandle InvalidHandle;
	if (!InRequest.IsValidRequest())
	{
		return InvalidHandle;
	}

	FActiveCollisionOverrideRuntime OverrideRuntime;
	OverrideRuntime.Handle.HandleId = NextCollisionOverrideHandleId++;
	OverrideRuntime.Handle.Slot = InRequest.Slot;
	OverrideRuntime.Request = InRequest;
	ActiveCollisionOverrides.Add(OverrideRuntime.Handle.HandleId, OverrideRuntime);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[CollisionRuntime] collision_override_applied owner=%s slot=%s preset=%s priority=%d reason=%s handle=%d target_count=%d"),
		*GetNameSafe(GetOwner()),
		LexToStringActionCollisionSlot(InRequest.Slot),
		LexToStringActionCollisionPreset(InRequest.Preset),
		InRequest.Priority,
		*InRequest.OwnerReason.ToString(),
		OverrideRuntime.Handle.HandleId,
		InRequest.TargetRegistrationNames.Num());

	RecomputeCollisionSlot(InRequest.Slot);
	return OverrideRuntime.Handle;
}

void UActionCollisionRuntimeComponent::ReleaseCollisionOverride(const FActionCollisionOverrideHandle& InHandle)
{
	if (!InHandle.IsValid())
	{
		return;
	}

	const FActiveCollisionOverrideRuntime* ExistingRuntime = ActiveCollisionOverrides.Find(InHandle.HandleId);
	if (!ExistingRuntime)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[CollisionRuntime] collision_override_released owner=%s slot=%s preset=%s priority=%d reason=%s handle=%d"),
		*GetNameSafe(GetOwner()),
		LexToStringActionCollisionSlot(ExistingRuntime->Request.Slot),
		LexToStringActionCollisionPreset(ExistingRuntime->Request.Preset),
		ExistingRuntime->Request.Priority,
		*ExistingRuntime->Request.OwnerReason.ToString(),
		InHandle.HandleId);

	const EActionCollisionSlot Slot = ExistingRuntime->Request.Slot;
	ActiveCollisionOverrides.Remove(InHandle.HandleId);
	RecomputeCollisionSlot(Slot);
}

void UActionCollisionRuntimeComponent::ReleaseCollisionOverridesByReason(const FName InOwnerReason)
{
	if (InOwnerReason == NAME_None)
	{
		return;
	}

	TArray<int32> HandlesToRemove;
	TSet<EActionCollisionSlot> DirtySlots;

	for (const TPair<int32, FActiveCollisionOverrideRuntime>& Pair : ActiveCollisionOverrides)
	{
		if (Pair.Value.Request.OwnerReason == InOwnerReason)
		{
			HandlesToRemove.Add(Pair.Key);
			DirtySlots.Add(Pair.Value.Request.Slot);

			UE_LOG(
				LogTemp,
				Log,
				TEXT("[CollisionRuntime] collision_override_released owner=%s slot=%s preset=%s priority=%d reason=%s handle=%d"),
				*GetNameSafe(GetOwner()),
				LexToStringActionCollisionSlot(Pair.Value.Request.Slot),
				LexToStringActionCollisionPreset(Pair.Value.Request.Preset),
				Pair.Value.Request.Priority,
				*Pair.Value.Request.OwnerReason.ToString(),
				Pair.Key);
		}
	}

	for (const int32 HandleId : HandlesToRemove)
	{
		ActiveCollisionOverrides.Remove(HandleId);
	}

	for (const EActionCollisionSlot Slot : DirtySlots)
	{
		RecomputeCollisionSlot(Slot);
	}
}

void UActionCollisionRuntimeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TPair<EActionCollisionSlot, TArray<FRegisteredCollisionComponentRuntime>>& Pair : RegisteredCollisionSlots)
	{
		RestoreDefaultsForSlot(Pair.Key);
	}

	ActiveCollisionOverrides.Reset();
	RegisteredCollisionSlots.Reset();

	Super::EndPlay(EndPlayReason);
}

FActionCollisionComponentSnapshot UActionCollisionRuntimeComponent::CaptureSnapshot(
	const UPrimitiveComponent* InComponent) const
{
	FActionCollisionComponentSnapshot Snapshot;
	if (!InComponent)
	{
		return Snapshot;
	}

	Snapshot.CollisionEnabled = InComponent->GetCollisionEnabled();
	Snapshot.CollisionObjectType = InComponent->GetCollisionObjectType();
	Snapshot.CollisionResponses = InComponent->GetCollisionResponseToChannels();
	Snapshot.bGenerateOverlapEvents = InComponent->GetGenerateOverlapEvents();
	return Snapshot;
}

void UActionCollisionRuntimeComponent::RestoreDefaultsForSlot(const EActionCollisionSlot InSlot)
{
	TArray<FRegisteredCollisionComponentRuntime>* RegisteredRuntimes = RegisteredCollisionSlots.Find(InSlot);
	if (!RegisteredRuntimes)
	{
		return;
	}

	for (const FRegisteredCollisionComponentRuntime& RegisteredRuntime : *RegisteredRuntimes)
	{
		if (UPrimitiveComponent* PrimitiveComponent = RegisteredRuntime.Component.Get())
		{
			ApplySnapshotToComponent(PrimitiveComponent, RegisteredRuntime.DefaultSnapshot);
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[CollisionRuntime] collision_override_restored_default owner=%s slot=%s component=%s registration=%s"),
				*GetNameSafe(GetOwner()),
				LexToStringActionCollisionSlot(InSlot),
				*GetNameSafe(PrimitiveComponent),
				*RegisteredRuntime.RegistrationName.ToString());
		}
	}
}

void UActionCollisionRuntimeComponent::RecomputeCollisionSlot(const EActionCollisionSlot InSlot)
{
	TArray<FRegisteredCollisionComponentRuntime>* RegisteredRuntimes = RegisteredCollisionSlots.Find(InSlot);
	if (!RegisteredRuntimes)
	{
		return;
	}

	for (const FRegisteredCollisionComponentRuntime& RegisteredRuntime : *RegisteredRuntimes)
	{
		UPrimitiveComponent* PrimitiveComponent = RegisteredRuntime.Component.Get();
		if (!PrimitiveComponent)
		{
			continue;
		}

		FActionCollisionComponentSnapshot EffectiveSnapshot = RegisteredRuntime.DefaultSnapshot;
		if (const FActiveCollisionOverrideRuntime* EffectiveOverride =
			SelectEffectiveOverrideForRegistration(InSlot, RegisteredRuntime))
		{
			ApplyPresetToSnapshot(EffectiveOverride->Request.Preset, EffectiveSnapshot);
			for (const FActionCollisionChannelOverride& ChannelOverride : EffectiveOverride->Request.ChannelOverrides)
			{
				EffectiveSnapshot.CollisionResponses.SetResponse(ChannelOverride.Channel, ChannelOverride.Response);
			}
		}

		ApplySnapshotToComponent(PrimitiveComponent, EffectiveSnapshot);
	}

	bool bHasAnyActiveOverride = false;
	for (const TPair<int32, FActiveCollisionOverrideRuntime>& Pair : ActiveCollisionOverrides)
	{
		if (Pair.Value.Request.Slot == InSlot)
		{
			bHasAnyActiveOverride = true;
			break;
		}
	}

	if (!bHasAnyActiveOverride)
	{
		for (const FRegisteredCollisionComponentRuntime& RegisteredRuntime : *RegisteredRuntimes)
		{
			if (RegisteredRuntime.Component.IsValid())
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT("[CollisionRuntime] collision_override_restored_default owner=%s slot=%s component=%s registration=%s"),
					*GetNameSafe(GetOwner()),
					LexToStringActionCollisionSlot(InSlot),
					*GetNameSafe(RegisteredRuntime.Component.Get()),
					*RegisteredRuntime.RegistrationName.ToString());
			}
		}
	}
}

bool UActionCollisionRuntimeComponent::DoesRequestTargetRegistration(
	const FActionCollisionOverrideRequest& InRequest,
	const FRegisteredCollisionComponentRuntime& InRegistration) const
{
	return InRequest.TargetRegistrationNames.Num() == 0
		|| InRequest.TargetRegistrationNames.Contains(InRegistration.RegistrationName);
}

const UActionCollisionRuntimeComponent::FActiveCollisionOverrideRuntime*
UActionCollisionRuntimeComponent::SelectEffectiveOverrideForRegistration(
	const EActionCollisionSlot InSlot,
	const FRegisteredCollisionComponentRuntime& InRegistration) const
{
	const FActiveCollisionOverrideRuntime* BestRuntime = nullptr;

	for (const TPair<int32, FActiveCollisionOverrideRuntime>& Pair : ActiveCollisionOverrides)
	{
		const FActiveCollisionOverrideRuntime& Candidate = Pair.Value;
		if (Candidate.Request.Slot != InSlot
			|| !DoesRequestTargetRegistration(Candidate.Request, InRegistration))
		{
			continue;
		}

		if (!BestRuntime
			|| Candidate.Request.Priority > BestRuntime->Request.Priority
			|| (Candidate.Request.Priority == BestRuntime->Request.Priority
				&& Candidate.Handle.HandleId > BestRuntime->Handle.HandleId))
		{
			BestRuntime = &Candidate;
		}
	}

	return BestRuntime;
}

void UActionCollisionRuntimeComponent::ApplyPresetToSnapshot(
	const EActionCollisionPreset InPreset,
	FActionCollisionComponentSnapshot& InOutSnapshot) const
{
	switch (InPreset)
	{
	case EActionCollisionPreset::Default:
		return;

	case EActionCollisionPreset::Disabled:
		InOutSnapshot.CollisionEnabled = ECollisionEnabled::NoCollision;
		return;

	case EActionCollisionPreset::HitQueryPawnOverlap:
	case EActionCollisionPreset::ProjectilePawnOverlap:
		InOutSnapshot.CollisionEnabled = ECollisionEnabled::QueryOnly;
		InOutSnapshot.CollisionResponses.SetAllChannels(ECR_Ignore);
		InOutSnapshot.CollisionResponses.SetResponse(ECC_Pawn, ECR_Overlap);
		return;

	case EActionCollisionPreset::ExecutionVictimPawnPassThrough:
		InOutSnapshot.CollisionResponses.SetResponse(ECC_Pawn, ECR_Ignore);
		return;

	default:
		return;
	}
}

void UActionCollisionRuntimeComponent::ApplySnapshotToComponent(
	UPrimitiveComponent* InComponent,
	const FActionCollisionComponentSnapshot& InSnapshot) const
{
	if (!IsValid(InComponent))
	{
		return;
	}

	InComponent->SetGenerateOverlapEvents(InSnapshot.bGenerateOverlapEvents);
	InComponent->SetCollisionObjectType(InSnapshot.CollisionObjectType);
	InComponent->SetCollisionResponseToChannels(InSnapshot.CollisionResponses);
	InComponent->SetCollisionEnabled(InSnapshot.CollisionEnabled);
}
