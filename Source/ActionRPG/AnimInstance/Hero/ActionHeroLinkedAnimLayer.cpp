// 文件说明：实现 ActionHeroLinkedAnimLayer 相关逻辑。


#include "AnimInstance/Hero/ActionHeroLinkedAnimLayer.h"

#include "Components/SkeletalMeshComponent.h"

#include "AnimInstance/Hero/ActionHeroAnimInstance.h"

UActionHeroLinkedAnimLayer::UActionHeroLinkedAnimLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UActionHeroLinkedAnimLayer::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
}

void UActionHeroLinkedAnimLayer::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (const UActionHeroAnimInstance* HeroAnimInstance = GetHeroAnimInstance())
	{
		// linked layer 只消费主 ABP 已经整理好的只读镜像，
		// 不自己再推导 CombatMode、武器层开关或大转向资格。
		// 这样武器层和主 ABP 对“当前正式状态”只会认同一份上游来源。
		GroundSpeed = HeroAnimInstance->GroundSpeed;
		bHasAccelerating = HeroAnimInstance->bHasAccelerating;
		CombatMode = HeroAnimInstance->CombatMode;
		MoveState = HeroAnimInstance->MoveState;
		bIsFalling = HeroAnimInstance->bIsFalling;
		bShouldUseWeaponLinkedLayer = HeroAnimInstance->bShouldUseWeaponLinkedLayer;
		bHasMoveInput = HeroAnimInstance->bHasMoveInput;
		bLargeTurnInputActive = HeroAnimInstance->bLargeTurnInputActive;
		InputYawDeltaDegrees = HeroAnimInstance->InputYawDeltaDegrees;
		ActorYawDeltaDegrees = HeroAnimInstance->ActorYawDeltaDegrees;
	}
	else
	{
		GroundSpeed = 0.f;
		bHasAccelerating = false;
		CombatMode = EHeroCombatMode::Idle;
		MoveState = EMoveState::Walk;
		bIsFalling = false;
		bShouldUseWeaponLinkedLayer = false;
		bHasMoveInput = false;
		bLargeTurnInputActive = false;
		InputYawDeltaDegrees = 0.f;
		ActorYawDeltaDegrees = 0.f;
	}
}

UActionHeroAnimInstance* UActionHeroLinkedAnimLayer::GetHeroAnimInstance() const
{
	// 当前 linked layer 只允许挂在英雄主 ABP 上。
	// 这里统一从 OwningComponent 的 AnimInstance 反查主实例，避免 layer 自己平行缓存另一份引用。
	return Cast<UActionHeroAnimInstance>(GetOwningComponent()->GetAnimInstance());
}
