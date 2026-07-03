// 文件说明：声明 ActionHeroLinkedAnimLayer 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionCombatRuntimeTypes.h"
#include "ActionType/ActionInputTypes.h"
#include "AnimInstance/ActionLinkedAnimLayer.h"
#include "ActionHeroLinkedAnimLayer.generated.h"

class UActionHeroAnimInstance;

UCLASS()
class ACTIONRPG_API UActionHeroLinkedAnimLayer : public UActionLinkedAnimLayer
{
	GENERATED_BODY()

public:
	UActionHeroLinkedAnimLayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void NativeInitializeAnimation() override;
	/** 每帧只从主 ABP 镜像当前表现数据；linked layer 自己不再并行推导 Combat / Idle 退出条件。 */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe))
	/** 读取所属主 ABP，linked layer 只通过它消费只读镜像数据。 */
	UActionHeroAnimInstance* GetHeroAnimInstance() const;

public:
	// 这些值全部来自主 ABP 的只读镜像，linked layer 自身不再并行推导 Combat / Idle 退出条件。
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	float GroundSpeed = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bHasAccelerating = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	EHeroCombatMode CombatMode = EHeroCombatMode::Idle;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	EMoveState MoveState = EMoveState::Walk;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bIsFalling = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bShouldUseWeaponLinkedLayer = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bHasMoveInput = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bLargeTurnInputActive = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	float InputYawDeltaDegrees = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	float ActorYawDeltaDegrees = 0.f;
	
};
