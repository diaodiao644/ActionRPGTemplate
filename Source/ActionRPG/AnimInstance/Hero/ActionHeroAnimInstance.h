// 文件说明：声明 ActionHeroAnimInstance 相关接口。

#pragma once

#include "CoreMinimal.h"
#include "AnimInstance/ActionCharacterAnimInstance.h"
#include "GameplayTagContainer.h"
#include "ActionHeroAnimInstance.generated.h"

class AActionHeroCharacter;
class AActionPlayerController;
class UActionHeroLinkedAnimLayer;

UCLASS()
class ACTIONRPG_API UActionHeroAnimInstance : public UActionCharacterAnimInstance
{
	GENERATED_BODY()

public:
	/** Hero 主 ABP：在角色主 ABP 镜像上补 Hero 专用 locomotion / weapon layer 只读镜像。 */
	UActionHeroAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 创建和销毁代理：
	// Hero 主 ABP 通过专用 anim proxy 把游戏线程镜像同步给动画线程，不在这里形成新的业务状态。
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	// 动画实例生命周期函数：
	// 这里只刷新主 ABP 镜像、weapon layer 可视化镜像和宿主引用，不推进正式战斗/输入状态。
	virtual void NativeBeginPlay() override;
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 读取拥有者英雄角色，仅供表现镜像与调试辅助使用。它只是主 ABP 的宿主解析入口，不形成新的角色状态源。 */
	AActionHeroCharacter* GetOwningHeroCharacter() { return OwningHeroCharacter; }
	/** 读取拥有者控制器，仅供表现镜像与调试辅助使用。它只是主 ABP 的宿主解析入口，不形成新的输入或朝向状态源。 */
	AActionPlayerController* GetOwningHeroController() { return OwningHeroController; }

public:
	/** Hero 主 ABP 当前帧读取到的下落状态镜像。正式 Falling 状态仍回到移动组件。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bIsFalling;

	/** Hero 主 ABP 当前帧读取到的移动输入镜像。正式移动输入语义仍回到控制器与输入链。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bHasMoveInput = false;

	/** Hero 主 ABP 当前帧读取到的输入朝向变化镜像。它只服务大转身与表现查询。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float InputYawDeltaDegrees = 0.f;

	/** Hero 主 ABP 当前帧读取到的角色朝向变化镜像。它只服务表现层，不替代正式转向状态。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float ActorYawDeltaDegrees = 0.f;

	/** Hero 主 ABP 当前帧读取到的大转身输入标记镜像。它只服务表现，不是新的战斗状态。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bLargeTurnInputActive = false;

	// 这组字段都只是主 ABP 的只读镜像，不是 linked layer 或 Combat / Idle 切换的正式状态源。
	/** Hero 主 ABP 当前帧读取到的 weapon linked layer 使用结果镜像；真正应用与校验仍回到 `HeroAssemblyComponent`。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bShouldUseWeaponLinkedLayer = false;

	// 仅用于表现查询与调试镜像，不再作为主 ABP 推导 linked layer 开关的正式状态源。
	/** Hero 主 ABP 当前帧读取到的武器标签镜像。它只服务表现查询与调试，不是正式装备状态源。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	FGameplayTag CurrentWeaponTag;

	// 仅用于表现查询与调试镜像；真正的 linked layer 应用与校验由 HeroAssemblyComponent 负责。
	/** Hero 主 ABP 当前帧读取到的 linked layer 类镜像。它只服务表现查询与调试，不等于 linked layer 已正式应用。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	TSubclassOf<UActionHeroLinkedAnimLayer> CurrentWeaponLinkedLayerClass;

protected:
	/** Hero 主 ABP 当前解析到的拥有者角色。它只服务镜像刷新与调试辅助。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|References")
	AActionHeroCharacter* OwningHeroCharacter = nullptr;

	/** Hero 主 ABP 当前解析到的拥有者控制器。它只服务表现查询与调试辅助。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|References")
	AActionPlayerController* OwningHeroController = nullptr;

private:
	bool bLastLoggedShouldUseWeaponLinkedLayer = false;

	bool bLoggedMovingIdleWhileWeaponLayerActive = false;

	bool bLoggedLargeTurnWhileWeaponLayerActive = false;

	FGameplayTag LastLoggedWeaponTag;

	TSubclassOf<UActionHeroLinkedAnimLayer> LastLoggedWeaponLinkedLayerClass;

	EHeroCombatMode LastLoggedCombatMode = EHeroCombatMode::Idle;

	float LastActorYawDegrees = 0.f;
};
