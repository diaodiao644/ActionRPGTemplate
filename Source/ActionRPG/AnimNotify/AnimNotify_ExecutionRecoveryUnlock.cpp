#include "AnimNotify/AnimNotify_ExecutionRecoveryUnlock.h"

#include "Characters/ActionCharacterBase.h"
#include "Components/Execution/ExecutionWindowComponent.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_ExecutionRecoveryUnlock::UAnimNotify_ExecutionRecoveryUnlock()
	: Super()
{
}

FString UAnimNotify_ExecutionRecoveryUnlock::GetNotifyName_Implementation() const
{
	return TEXT("ExecutionRecoveryUnlock");
}

void UAnimNotify_ExecutionRecoveryUnlock::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(MeshComp->GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	UExecutionWindowComponent* ExecutionWindowComponent =
		OwnerCharacter->FindComponentByClass<UExecutionWindowComponent>();
	if (!ExecutionWindowComponent)
	{
		return;
	}

	ExecutionWindowComponent->NotifyExecutionRecoveryUnlockFrame();
}
