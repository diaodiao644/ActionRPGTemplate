// 文件说明：声明 ActionCharacterAnimInstance 相关接口。

#pragma once

#include "CoreMinimal.h"
#include "AnimInstance/ActionBaseAnimInstance.h"
#include "ActionType/ActionAnimationTypes.h"
#include "ActionCharacterAnimInstance.generated.h"

class AActionCharacterBase;
class UCharacterMovementComponent;

UCLASS()
class ACTIONRPG_API UActionCharacterAnimInstance : public UActionBaseAnimInstance
{
	GENERATED_BODY()

public:
	UActionCharacterAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 动画实例生命周期函数。
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(Blueprintcallable, BlueprintPure, Category = "AnimInstance|Data")
	virtual AActionCharacterBase* GetOwningCharacter() const;

	UFUNCTION(Blueprintcallable, BlueprintPure, Category = "AnimInstance|Data")
	virtual UCharacterMovementComponent* GetOwningMovementComponent() const;

public:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float GroundSpeed = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bHasAccelerating = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	EHeroCombatMode CombatMode = EHeroCombatMode::Idle;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	EMoveState MoveState = EMoveState::Walk;

protected:
	UPROPERTY()
	AActionCharacterBase* OwningCharacter = nullptr;

	UPROPERTY()
	UCharacterMovementComponent* OwningMovementComponent = nullptr;
};
