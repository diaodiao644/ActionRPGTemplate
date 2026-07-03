#include "AnimNotify/AnimNotify_CombatReactRecoveryUnlock.h"

#include "Characters/ActionCharacterBase.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_CombatReactRecoveryUnlock::UAnimNotify_CombatReactRecoveryUnlock()
	: Super()
{
}

FString UAnimNotify_CombatReactRecoveryUnlock::GetNotifyName_Implementation() const
{
	return TEXT("CombatReactRecoveryUnlock");
}

void UAnimNotify_CombatReactRecoveryUnlock::Notify(
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

	if (UActionCombatReactComponent* CombatReactComponent = OwnerCharacter->GetActionCombatReactComponent())
	{
		CombatReactComponent->NotifyCombatReactUnlockFrame();
	}
}
