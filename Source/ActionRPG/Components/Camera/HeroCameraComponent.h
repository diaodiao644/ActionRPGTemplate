// 文件说明：声明英雄角色相机逻辑组件，负责按战斗状态解析镜头模式并驱动相机参数过渡。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HeroCameraComponent.generated.h"

class AActionHeroCharacter;
class AActionPlayerController;
class UCameraComponent;
class UHeroCombatComponent;
class UHeroTargetingComponent;
class USpringArmComponent;

/**
 * 单个镜头模式的数据配置。
 * 这一层只描述“镜头应该长什么样”，
 * 不负责决定什么时候切入这个模式。
 */
USTRUCT(BlueprintType)
struct FHeroCameraModeConfig
{
	GENERATED_BODY()

public:
	/** 相机臂长度。值越大，镜头离角色越远。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float TargetArmLength = 400.f;

	/** 相机臂末端插槽偏移。用于微调镜头落点。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	FVector SocketOffset = FVector::ZeroVector;

	/** 相机臂目标偏移。用于把镜头整体抬高或侧移。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	FVector TargetOffset = FVector::ZeroVector;

	/** 相机视野。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float FieldOfView = 90.f;

	/** 镜头参数向目标收敛时的插值速度。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float BlendSpeed = 8.f;

	/** 是否启用相机位置延迟。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	bool bEnableCameraLag = true;

	/** 相机位置延迟速度。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (EditCondition = "bEnableCameraLag"))
	float CameraLagSpeed = 12.f;

	/** 是否启用相机旋转延迟。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	bool bEnableCameraRotationLag = false;

	/** 相机旋转延迟速度。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (EditCondition = "bEnableCameraRotationLag"))
	float CameraRotationLagSpeed = 10.f;
};

/** 当前相机逻辑所处的镜头模式。 */
UENUM(BlueprintType)
enum class EHeroCameraMode : uint8
{
	Exploration,
	Combat,
	TargetLock,
	Defense,
	Dodge,
	SpecialWeaponSwitch
};

/**
 * 英雄角色相机逻辑组件。
 * 设计目标：
 * 1. Character 中继续保留 SpringArm 和 CameraComponent 实体；
 * 2. 所有镜头模式判断、参数切换、平滑过渡都集中到这里；
 * 3. 为后续锁定目标、处决演出镜头、远程瞄准镜头继续扩展留出稳定入口。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroCameraComponent();

protected:
	/** 组件生命周期入口。 */
	virtual void BeginPlay() override;

public:
	/** 每帧根据当前战斗状态更新镜头模式与镜头参数。 */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/**
	 * 初始化相机实体引用。
	 * 角色在构造时创建相机实体，但真正的逻辑控制统一交给这个组件。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Camera")
	void InitializeCameraRig(USpringArmComponent* InCameraBoom, UCameraComponent* InFollowCamera);

	/** 读取当前解析出的镜头模式。 */
	UFUNCTION(BlueprintPure, Category = "Action|Camera")
	EHeroCameraMode GetCurrentCameraMode() const { return CurrentCameraMode; }

	/** 立刻按当前状态刷新一次镜头，不走平滑插值。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Camera")
	void ForceRefreshCameraMode();

protected:
	/** 缓存相机依赖组件，避免每帧重复 Cast 和查找。 */
	void CacheOwnerDependencies();

	/** 检查相机实体引用是否已经准备完毕。 */
	bool HasValidCameraRig() const;

	/** 根据当前战斗状态解析目标镜头模式。 */
	EHeroCameraMode ResolveDesiredCameraMode() const;

	/** 按模式读取对应的参数配置。*/
	const FHeroCameraModeConfig& GetCameraModeConfig(EHeroCameraMode InCameraMode) const;

	/** 将某个模式的参数即时应用到相机实体上。*/
	void ApplyCameraModeConfigImmediately(const FHeroCameraModeConfig& InConfig);

	/** 将某个模式的参数平滑推进到相机实体上。*/
	void BlendToCameraModeConfig(const FHeroCameraModeConfig& InConfig, float DeltaTime);

protected:
	/** 非战斗探索状态下的镜头配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes")
	FHeroCameraModeConfig ExplorationCameraConfig;

	/** 普通战斗状态下的镜头配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes")
	FHeroCameraModeConfig CombatCameraConfig;

	/** 正式锁定目标状态下的镜头配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes")
	FHeroCameraModeConfig TargetLockCameraConfig;

	/** 防御状态下的镜头配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes")
	FHeroCameraModeConfig DefenseCameraConfig;

	/** 闪避状态下的镜头配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes")
	FHeroCameraModeConfig DodgeCameraConfig;

	/** 特殊切武表现期的镜头配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes")
	FHeroCameraModeConfig SpecialWeaponSwitchCameraConfig;

private:
	/** 当前生效中的镜头模式。 */
	UPROPERTY(VisibleAnywhere, Category = "Camera|Runtime")
	EHeroCameraMode CurrentCameraMode = EHeroCameraMode::Exploration;

	/** 角色上真实承载镜头的相机臂。 */
	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	/** 角色上真实承载画面的相机组件。*/
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;

	/** 依赖组件缓存。 */
	TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	TWeakObjectPtr<AActionPlayerController> CachedHeroController;
	TWeakObjectPtr<UHeroCombatComponent> CachedHeroCombatComponent;
	TWeakObjectPtr<UHeroTargetingComponent> CachedHeroTargetingComponent;
};
