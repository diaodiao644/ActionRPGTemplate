#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionCollisionTypes.h"
#include "Components/ActorComponent.h"
#include "ActionCollisionRuntimeComponent.generated.h"

class UPrimitiveComponent;

UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UActionCollisionRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionCollisionRuntimeComponent();

	bool RegisterCollisionSlot(
		EActionCollisionSlot InSlot,
		UPrimitiveComponent* InComponent,
		FName InRegistrationName = NAME_None);

	FActionCollisionOverrideHandle AcquireCollisionOverride(const FActionCollisionOverrideRequest& InRequest);
	void ReleaseCollisionOverride(const FActionCollisionOverrideHandle& InHandle);
	void ReleaseCollisionOverridesByReason(FName InOwnerReason);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FRegisteredCollisionComponentRuntime
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		EActionCollisionSlot Slot = EActionCollisionSlot::Custom;
		FName RegistrationName = NAME_None;
		FActionCollisionComponentSnapshot DefaultSnapshot;
	};

	struct FActiveCollisionOverrideRuntime
	{
		FActionCollisionOverrideHandle Handle;
		FActionCollisionOverrideRequest Request;
	};

	FActionCollisionComponentSnapshot CaptureSnapshot(const UPrimitiveComponent* InComponent) const;
	void RestoreDefaultsForSlot(EActionCollisionSlot InSlot);
	void RecomputeCollisionSlot(EActionCollisionSlot InSlot);
	bool DoesRequestTargetRegistration(
		const FActionCollisionOverrideRequest& InRequest,
		const FRegisteredCollisionComponentRuntime& InRegistration) const;
	const FActiveCollisionOverrideRuntime* SelectEffectiveOverrideForRegistration(
		EActionCollisionSlot InSlot,
		const FRegisteredCollisionComponentRuntime& InRegistration) const;
	void ApplyPresetToSnapshot(
		EActionCollisionPreset InPreset,
		FActionCollisionComponentSnapshot& InOutSnapshot) const;
	void ApplySnapshotToComponent(
		UPrimitiveComponent* InComponent,
		const FActionCollisionComponentSnapshot& InSnapshot) const;

private:
	UPROPERTY(Transient)
	int32 NextCollisionOverrideHandleId = 1;

	TMap<EActionCollisionSlot, TArray<FRegisteredCollisionComponentRuntime>> RegisteredCollisionSlots;
	TMap<int32, FActiveCollisionOverrideRuntime> ActiveCollisionOverrides;
};
