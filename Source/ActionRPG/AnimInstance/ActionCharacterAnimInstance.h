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
	/** 角色主 ABP 基类：只维护角色与移动相关的只读镜像，不承接战斗正式状态。 */
	UActionCharacterAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 动画实例生命周期函数：
	// 这里只负责解析宿主并刷新主 ABP 可消费的只读镜像，不在这里推进业务状态。
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 读取拥有者角色。它只是主 ABP 的宿主解析入口，不是新的状态源。 */
	UFUNCTION(Blueprintcallable, BlueprintPure, Category = "AnimInstance|Data", meta = (ToolTip = "读取拥有者角色。它只是主 ABP 的稳定宿主解析入口，不会创建新的角色或战斗状态。"))
	virtual AActionCharacterBase* GetOwningCharacter() const;

	/** 读取拥有者移动组件。它只服务表现层查询，不替代 CharacterMovement 正式移动状态。 */
	UFUNCTION(Blueprintcallable, BlueprintPure, Category = "AnimInstance|Data", meta = (ToolTip = "读取拥有者移动组件。它只服务主 ABP 镜像查询，不替代 CharacterMovement 正式移动状态。"))
	virtual UCharacterMovementComponent* GetOwningMovementComponent() const;

public:
	/** 主 ABP 当前帧读取到的地面速度镜像。它只服务表现，不是正式移动状态源。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float GroundSpeed = 0.f;

	/** 主 ABP 当前帧读取到的加速状态镜像。它只表达表现层是否在加速，不替代底层移动组件状态。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bHasAccelerating = false;

	/** 主 ABP 当前帧读取到的战斗模式镜像。正式战斗模式仍回到 `HeroCombatComponent`。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	EHeroCombatMode CombatMode = EHeroCombatMode::Idle;

	/** 主 ABP 当前帧读取到的移动语义镜像。正式移动输入语义仍回到 `ActionPlayerController`。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	EMoveState MoveState = EMoveState::Walk;

protected:
	/** 主 ABP 当前解析到的拥有者角色引用。它只服务宿主解析与镜像刷新。 */
	UPROPERTY()
	AActionCharacterBase* OwningCharacter = nullptr;

	/** 主 ABP 当前解析到的移动组件引用。它只服务表现层镜像读取。 */
	UPROPERTY()
	UCharacterMovementComponent* OwningMovementComponent = nullptr;
};
