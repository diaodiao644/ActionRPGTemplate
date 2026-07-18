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
 * 这一层是镜头模式消费桥：
 * 1. Character 中继续保留 SpringArm 和 CameraComponent 实体；
 * 2. 这里统一消费 Combat / Targeting 正式状态，解析当前应使用的镜头模式；
 * 3. 这里只驱动镜头参数与平滑过渡，不单独持有 Combat / TargetLock 的正式状态源。
 * 它是镜头模式消费桥，不是 Combat、TargetLock、Defense、Dodge 或 SpecialWeaponSwitch 的正式状态源。
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
	/** 每帧根据当前正式状态更新镜头模式与镜头参数。它只消费外部正式状态，不自行决定业务资格。 */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/**
	 * 初始化相机实体引用。
	 * 角色在构造时创建相机实体，但真正的逻辑控制统一交给这个组件。
	 * 它只是镜头桥接入口，不承担角色启动链或 HUD 初始化职责。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Camera")
	void InitializeCameraRig(USpringArmComponent* InCameraBoom, UCameraComponent* InFollowCamera);

	/** 读取当前解析出的正式镜头模式。它是只读运行态结果，不会推动镜头继续切换。 */
	UFUNCTION(BlueprintPure, Category = "Action|Camera")
	EHeroCameraMode GetCurrentCameraMode() const { return CurrentCameraMode; }

	/** 立刻按当前状态刷新一次镜头，不走平滑插值。它只刷新表现结果，不推进战斗状态。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Camera")
	void ForceRefreshCameraMode();

protected:
	/** 缓存相机依赖组件，避免每帧重复 Cast 和查找。它只做宿主解析，不推进任何战斗语义。 */
	void CacheOwnerDependencies();

	/** 检查相机实体引用是否已经准备完毕。它只是运行保护，不代表角色其它系统已经正式就绪。 */
	bool HasValidCameraRig() const;

	/** 根据当前正式战斗 / 锁定状态解析目标镜头模式，不自行创建第二套持续状态。 */
	EHeroCameraMode ResolveDesiredCameraMode() const;

	/** 按模式读取对应的静态参数配置。它是配置入口，不是当前镜头状态快照。 */
	const FHeroCameraModeConfig& GetCameraModeConfig(EHeroCameraMode InCameraMode) const;

	/** 将某个模式的参数即时应用到相机实体上。它只处理相机表现层参数。 */
	void ApplyCameraModeConfigImmediately(const FHeroCameraModeConfig& InConfig);

	/** 将某个模式的参数平滑推进到相机实体上。它同样只处理表现层插值。 */
	void BlendToCameraModeConfig(const FHeroCameraModeConfig& InConfig, float DeltaTime);

protected:
	/** 非战斗探索状态下的镜头静态模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes", meta = (ToolTip = "非战斗探索状态的镜头静态模板。它只描述 Exploration 模式该长什么样，不代表运行时当前一定处于探索态。"))
	FHeroCameraModeConfig ExplorationCameraConfig;

	/** 普通战斗状态下的镜头静态模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes", meta = (ToolTip = "普通战斗状态的镜头静态模板。HeroCameraComponent 只会在解析到 Combat 模式时消费它。"))
	FHeroCameraModeConfig CombatCameraConfig;

	/** 正式锁定目标状态下的镜头静态模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes", meta = (ToolTip = "正式 TargetLock 状态的镜头静态模板。锁定资格和当前锁定目标仍回到 HeroTargetingComponent。"))
	FHeroCameraModeConfig TargetLockCameraConfig;

	/** 防御状态下的镜头静态模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes", meta = (ToolTip = "防御状态的镜头静态模板。它只服务镜头表现，不替代防御窗口或防御资格状态源。"))
	FHeroCameraModeConfig DefenseCameraConfig;

	/** 闪避状态下的镜头静态模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes", meta = (ToolTip = "闪避状态的镜头静态模板。它只在当前正式闪避运行态成立时被消费。"))
	FHeroCameraModeConfig DodgeCameraConfig;

	/** 特殊切武表现期的镜头静态模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes", meta = (ToolTip = "特殊切武表现期的镜头静态模板。它只服务表现层，不替代切武事务或当前装备结果。"))
	FHeroCameraModeConfig SpecialWeaponSwitchCameraConfig;

private:
	/** 当前生效中的镜头模式。它是 HeroCameraComponent 的解析结果，不是 Combat、TargetLock 或 Defense 的正式状态源。 */
	UPROPERTY(VisibleAnywhere, Category = "Camera|Runtime", meta = (ToolTip = "当前相机解析出的镜头模式结果。它只是 HeroCameraComponent 的只读运行态结果，不会反向替代 Combat 或 TargetLock 的正式状态。"))
	EHeroCameraMode CurrentCameraMode = EHeroCameraMode::Exploration;

	/** 角色上真实承载镜头的相机臂。 */
	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	/** 角色上真实承载画面的相机组件。*/
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;

	/** 依赖组件缓存。它们只服务镜头消费链与调试，不额外保存第二套角色、战斗或锁定状态。 */
	TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	TWeakObjectPtr<AActionPlayerController> CachedHeroController;
	TWeakObjectPtr<UHeroCombatComponent> CachedHeroCombatComponent;
	TWeakObjectPtr<UHeroTargetingComponent> CachedHeroTargetingComponent;
};
