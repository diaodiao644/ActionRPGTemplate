// 文件说明：实现 AnimNotify_AttackInAir 相关逻辑。


#include "AnimNotify/AnimNotify_AttackInAir.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UAnimNotify_AttackInAir::UAnimNotify_AttackInAir()
    :Super()
{
}

FString UAnimNotify_AttackInAir::GetNotifyName_Implementation() const
{
    return Super::GetNotifyName_Implementation();
}

void UAnimNotify_AttackInAir::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
    if (!Character) return;

    //确保数据有效
    UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
    if (!AnimInstance) return;

    //确保数据有效
    UAnimMontage* CurrentMontage = Cast<UAnimMontage>(Animation);
    if (!CurrentMontage || !AnimInstance->Montage_IsPlaying(CurrentMontage))
    {
        return;
    }

    if (Character->GetCharacterMovement()->IsFalling())
    {
        AnimInstance->Montage_JumpToSection(FName(TEXT("Loop")), CurrentMontage);
    }
    else
    {
        AnimInstance->Montage_JumpToSection(FName(TEXT("End")), CurrentMontage);
    }
}
