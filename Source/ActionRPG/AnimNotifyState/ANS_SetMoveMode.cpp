// 文件说明：实现 ANS_SetMoveMode 相关逻辑。


#include "AnimNotifyState/ANS_SetMoveMode.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UANS_SetMoveMode::UANS_SetMoveMode()
	:Super()
{
}

void UANS_SetMoveMode::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		OwnerCharacter->GetCharacterMovement()->SetMovementMode(BeginMovementMode);
	}

}

void UANS_SetMoveMode::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UANS_SetMoveMode::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		OwnerCharacter->GetCharacterMovement()->SetMovementMode(EndMovementMode);
	}
}
