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
	/** Hero 武器 linked layer：只消费主 ABP 当前帧镜像，不自己推导正式 Combat / Idle 状态。 */
	UActionHeroLinkedAnimLayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 初始化 linked layer 与主 ABP 的消费关系。它只做表现层宿主解析。 */
	virtual void NativeInitializeAnimation() override;
	/** 每帧只从主 ABP 镜像同步当前表现数据；linked layer 自己不再并行推导 Combat / Idle 退出条件或武器 layer 正式应用状态。 */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe))
	/** 读取所属主 ABP，linked layer 只通过它消费只读镜像数据。它只是主 ABP 解析入口，不是新的状态源。 */
	UActionHeroAnimInstance* GetHeroAnimInstance() const;

public:
	// 这些值全部来自主 ABP 的只读镜像，linked layer 自身不再并行推导 Combat / Idle 退出条件。
	/** linked layer 当前帧消费到的地面速度镜像。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	float GroundSpeed = 0.f;

	/** linked layer 当前帧消费到的加速状态镜像。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bHasAccelerating = false;

	/** linked layer 当前帧消费到的战斗模式镜像。正式战斗模式仍回到 `HeroCombatComponent`。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	EHeroCombatMode CombatMode = EHeroCombatMode::Idle;

	/** linked layer 当前帧消费到的移动语义镜像。正式移动语义仍回到控制器与输入链。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	EMoveState MoveState = EMoveState::Walk;

	/** linked layer 当前帧消费到的下落状态镜像。它只服务表现，不替代正式 Falling 状态。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bIsFalling = false;

	/** linked layer 当前帧消费到的 weapon layer 使用结果镜像；真正应用与校验仍回到 `HeroAssemblyComponent`。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bShouldUseWeaponLinkedLayer = false;

	/** linked layer 当前帧消费到的移动输入镜像。它只服务表现层，不替代控制器或输入链正式状态。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bHasMoveInput = false;

	/** linked layer 当前帧消费到的大转身输入标记镜像。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bLargeTurnInputActive = false;

	/** linked layer 当前帧消费到的输入朝向变化镜像。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	float InputYawDeltaDegrees = 0.f;

	/** linked layer 当前帧消费到的角色朝向变化镜像。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	float ActorYawDeltaDegrees = 0.f;
};
