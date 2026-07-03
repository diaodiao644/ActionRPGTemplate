// 文件说明：实现 ActionCharacterAnimInstance 相关逻辑。


#include "AnimInstance/ActionCharacterAnimInstance.h"

#include "Characters/ActionCharacterBase.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UActionCharacterAnimInstance::UActionCharacterAnimInstance(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

void UActionCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningCharacter = Cast<AActionCharacterBase>(TryGetPawnOwner());
	OwningMovementComponent = OwningCharacter ? OwningCharacter->GetCharacterMovement() : nullptr;

}

void UActionCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

}

AActionCharacterBase* UActionCharacterAnimInstance::GetOwningCharacter() const
{
	return OwningCharacter;
}

UCharacterMovementComponent* UActionCharacterAnimInstance::GetOwningMovementComponent() const
{
	return OwningMovementComponent;
}

void FCharacterAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	Super::PreUpdate(InAnimInstance, DeltaSeconds);

	if (UActionCharacterAnimInstance* ActionAnimInstance = Cast<UActionCharacterAnimInstance>(InAnimInstance))
	{
		if (ActionAnimInstance->GetOwningCharacter() && ActionAnimInstance->GetOwningMovementComponent())
		{
			Velocity = ActionAnimInstance->GetOwningMovementComponent()->Velocity;
			CurrentAcceleration = ActionAnimInstance->GetOwningMovementComponent()->GetCurrentAcceleration().Size2D();

			if (UPawnCombatComponent* PawnCombatComponent = ActionAnimInstance->GetOwningCharacter()->FindComponentByClass<UPawnCombatComponent>())
			{
				CombatMode = PawnCombatComponent->GetCombatMode();
			}

		}

	}

}

void FCharacterAnimInstanceProxy::Update(float DeltaSeconds)
{
	Super::Update(DeltaSeconds);

}

void FCharacterAnimInstanceProxy::PostUpdate(UAnimInstance* InAnimInstance) const
{
	Super::PostUpdate(InAnimInstance);

	if (UActionCharacterAnimInstance* ActionAnimInstance = Cast<UActionCharacterAnimInstance>(InAnimInstance))
	{
		ActionAnimInstance->GroundSpeed = Velocity.Size2D();
		ActionAnimInstance->bHasAccelerating = CurrentAcceleration > 0;
		ActionAnimInstance->CombatMode = this->CombatMode;
	}

}
