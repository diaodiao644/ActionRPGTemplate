// 文件说明：声明玩家英雄角色相关接口。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "Characters/ActionCharacterBase.h"
#include "Logging/LogMacros.h"
#include "ActionHeroCharacter.generated.h"

class AHeroWeaponBase;
class UActionHeroLinkedAnimLayer;
class UBoxComponent;
class UCameraComponent;
class UDataAsset_HeroLoadoutData;
class UHeroAttackComponent;
class UHeroAssemblyComponent;
class UHeroCameraComponent;
class UHeroCombatComponent;
class UHeroCombatInputComponent;
class UHeroCombatFeedbackComponent;
class UHeroDefenseComponent;
class UHeroEquipmentComponent;
class UHeroLoadoutContextComponent;
class UHeroLoadoutEffectComponent;
class UHeroLoadoutRuntimeComponent;
class UHeroLoadoutStateComponent;
class UHeroExecutionCoordinatorComponent;
class UHeroHitSourceComponent;
class UHeroTargetingComponent;
class UHeroWeaponSwitchComponent;
class USpringArmComponent;
class UHeroAttributeComponent;

/** 角色固定武器槽启动链全部完成后的蓝图事件。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeroSystemsStartupReady);
/** 角色固定武器槽启动链失败后的蓝图事件，会把失败槽位和原因一并抛给 UI。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHeroSystemsStartupFailed, EHeroWeaponLoadoutSlot, LoadoutSlot, const FString&, FailureReason);

/**
 * 玩家英雄角色。
 * 当前版本里，角色本体主要负责三件事：
 * 1. 初始化 ASC 与 AttributeSet；
 * 2. 创建并持有战斗组件、装备组件、相机组件与角色装配桥组件；
 * 3. 作为 Hero 侧宿主公共入口，向外围暴露稳定的组件 getter 与装配桥入口。
 *
 * 说明：
 * 1. 空手与各类武器的战斗动画已经统一收口到 WeaponDefinition.AnimationConfig；
 * 2. 启动链协调、HUD 初始化桥和武器表现桥已经迁入 HeroAssemblyComponent；
 * 3. 固定武器槽配置与通用能力初始化，统一来自 HeroLoadoutData；
 * 4. 新的内部 C++ 读链应优先直连正式宿主或使用本类私有 helper，组件 getter 继续主要承担宿主解析与蓝图稳定入口职责。
 */
UCLASS(config = Game)
class AActionHeroCharacter : public AActionCharacterBase
{
	GENERATED_BODY()

protected:
	/** 第三人称镜头弹簧臂。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom = nullptr;

	/** 跟随角色的主相机。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera = nullptr;

	/** 独立的相机逻辑组件，负责镜头模式切换与参数过渡。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UHeroCameraComponent* HeroCameraComponent = nullptr;

public:
	AActionHeroCharacter();

protected:
	/** ACharacter 生命周期回调。 */
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 初始化 ASC 与 AttributeSet 引用。 */
	void InitAbilitySystem();

	/**
	 * 授予角色通用负载能力。
	 * 这里只处理 HeroLoadoutData 里与“角色本体常驻能力”相关的部分，
	 * 不负责固定武器槽主攻击能力的授予，那部分会继续由装备 / 战斗链按槽位完成。
	 */
	void InitLoadoutData();

	/**
	 * 统一初始化角色战斗与装备侧逻辑。
	 * 这一步是角色侧真正把“ASC、HeroLoadoutData、战斗组件、装备组件、输入绑定条件”串起来的入口。
	 */
	void InitializeHeroSystems();

public:
	/** 获取弹簧臂。 */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** 获取主相机。 */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** 获取相机逻辑组件。 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	UHeroCameraComponent* GetHeroCameraComponent() const { return HeroCameraComponent; }

	/** 获取战斗组件。 */
	UFUNCTION(Blueprintpure, Category = "AbilitySystem", meta = (BlueprintThreadSafe))
	UHeroCombatComponent* GetHeroCombatComponent() const { return HeroCombatComponent; }

	/** 获取角色装配桥组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroAssemblyComponent* GetHeroAssemblyComponent() const { return HeroAssemblyComponent; }

	/** 获取战斗输入运行态组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroCombatInputComponent* GetHeroCombatInputComponent() const { return HeroCombatInputComponent; }

	/** 获取装备组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroEquipmentComponent* GetHeroEquipmentComponent() const { return HeroEquipmentComponent; }

	/** 获取装备域资源链 / 实例链运行时组件。该 getter 继续只承担宿主解析与蓝图稳定入口职责，不再作为后续收窄批次候选。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroLoadoutRuntimeComponent* GetHeroLoadoutRuntimeComponent() const
	{
		return HeroLoadoutRuntimeComponent;
	}

	/** 获取装备域属性缓存 / 发射物标签上下文组件。该 getter 继续只承担宿主解析与蓝图稳定入口职责，不再作为后续收窄批次候选。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroLoadoutContextComponent* GetHeroLoadoutContextComponent() const
	{
		return HeroLoadoutContextComponent;
	}

	/** 获取装备域启动/预热状态机与 UI 快照桥组件。该 getter 继续只承担宿主解析与蓝图稳定入口职责，不再作为后续收窄批次候选。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroLoadoutStateComponent* GetHeroLoadoutStateComponent() const
	{
		return HeroLoadoutStateComponent;
	}

	/** 获取装备域外部命中效果生命周期组件。该 getter 继续只承担宿主解析与蓝图稳定入口职责，不再作为后续收窄批次候选。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroLoadoutEffectComponent* GetHeroLoadoutEffectComponent() const
	{
		return HeroLoadoutEffectComponent;
	}

	/** 获取攻击组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroAttackComponent* GetHeroAttackComponent() const { return HeroAttackComponent; }

	/** 获取防御系输入组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroDefenseComponent* GetHeroDefenseComponent() const { return HeroDefenseComponent; }

	/** 获取英雄侧通用战斗反馈组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroCombatFeedbackComponent* GetHeroCombatFeedbackComponent() const { return HeroCombatFeedbackComponent; }

	/** 获取切武组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroWeaponSwitchComponent* GetHeroWeaponSwitchComponent() const { return HeroWeaponSwitchComponent; }

	/** 获取英雄侧处决协调组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroExecutionCoordinatorComponent* GetHeroExecutionCoordinatorComponent() const { return HeroExecutionCoordinatorComponent; }

	/** 获取命中来源窗口组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroHitSourceComponent* GetHeroHitSourceComponent() const { return HeroHitSourceComponent; }

	/** 获取轻量转向辅助与锁定目标组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroTargetingComponent* GetHeroTargetingComponent() const { return HeroTargetingComponent; }

	/** 获取属性变化的传递组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UHeroAttributeComponent* GetHeroAttributeComponent() const { return HeroAttributeComponent; }

	/** 设置角色当前是否存在移动输入。 */
	void SetHasMoveInput(bool bInHasMoveInput);

	/** 查询角色当前是否存在移动输入。 */
	bool HasMoveInput() const { return bHasMoveInput; }

	/** 角色当前是否应正式启用武器 LinkedLayer 表现。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Weapon")
	bool ShouldUseWeaponLinkedLayer() const;

	/** 读取角色当前正式武器层类。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Weapon")
	TSubclassOf<UActionHeroLinkedAnimLayer> GetCurrentWeaponLinkedAnimLayerClass() const;

	UFUNCTION(BlueprintPure, Category = "Action|Startup")
	bool IsHeroSystemsStartupReady() const;

	/** 查询角色固定武器槽启动链是否仍在执行中，可用于加载界面是否继续显示。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup")
	bool IsHeroSystemsStartupInProgress() const;

	/** 查询角色固定武器槽启动链是否已经失败。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup")
	bool HasHeroSystemsStartupFailed() const;

	/** 查询最近一次启动失败原因，供 UI 直接显示错误文本。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup")
	FString GetHeroSystemsStartupFailureReason() const;

	/** 查询启动链总体状态枚举，适合在蓝图里做分支判断。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup")
	EHeroWeaponLoadoutStartupState GetHeroSystemsStartupState() const;

	/** 查询启动链进度比例，返回值范围为 0 到 1。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup")
	float GetHeroSystemsStartupProgressRatio() const;

	/** 查询当前还剩多少固定武器槽没有完成启动预热。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup")
	int32 GetHeroSystemsStartupPendingSlotCount() const;

	/** 查询这次启动一共需要预热多少固定武器槽。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup")
	int32 GetHeroSystemsStartupTotalSlotCount() const;

	/** 构建当前固定四槽 UI 所需的只读聚合快照。 */
	UFUNCTION(BlueprintPure, Category = "Action|UI")
	bool BuildHeroLoadoutUISnapshot(FHeroWeaponLoadoutUISnapshot& OutSnapshot) const;

	/** 供加载失败界面手动重试启动链。*/
	UFUNCTION(BlueprintCallable, Category = "Action|Startup")
	bool RetryHeroSystemsStartup();

	/** 供加载失败界面重新进入当前关卡。*/
	UFUNCTION(BlueprintCallable, Category = "Action|Startup")
	bool ReloadCurrentLevel();

	/** 打印当前玩家身上实际授予的 Hero 战斗 GA 关系配置摘要，便于核对蓝图默认值是否与 C++ 基线一致。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability|Debug")
	void PrintGrantedHeroCombatAbilityRelationshipAudit() const;

	/** 按输入标签打印指定 Hero 战斗 GA 的当前调试摘要，便于在 OpenMap 里解释“为什么能放/为什么不能放”。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability|Debug")
	bool PrintHeroCombatAbilityDebugByInputTag(FGameplayTag AbilityInputTag) const;

	/**
	 * 应用武器实例的角色表现状态。
	 * 这里是 Character 层对装配桥的稳定转发口：
	 * 1. 非当前槽位武器会回到 Holster 并隐藏；
	 * 2. 当前已装备武器会保持可见，再由 Combat 表现态决定挂 Holster 还是 WeaponSocket；
	 * 3. 武器碰撞正式状态仍由武器自身表现态运行时统一接管。
	 */
	void ApplyWeaponActorPresentation(AHeroWeaponBase* InWeapon, bool bIsEquipped) const;

	/**
	 * 刷新角色当前武器动画层表现。
	 * 若传入空类，则卸下当前动画层并回到角色基础动画层；
	 * 若传入有效类，则先卸下当前层，再挂上新的武器动画层。
	 */
	void RefreshWeaponAnimLayer(TSubclassOf<UActionHeroLinkedAnimLayer> InLinkedAnimLayerClass);

public:
	/** 角色当前是否有移动输入，用于闪避方向与动作选择。 */
	UPROPERTY()
	bool bHasMoveInput = false;

protected:
	/** 输入与状态分发核心组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroCombatComponent* HeroCombatComponent = nullptr;

	/** 角色装配桥组件，承接启动链、HUD 与武器表现桥。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroAssemblyComponent* HeroAssemblyComponent = nullptr;

	/** 输入运行时、缓冲输入与 Held 回放组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroCombatInputComponent* HeroCombatInputComponent = nullptr;

	/** 固定武器槽、武器实例与武器资源管理核心组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroEquipmentComponent* HeroEquipmentComponent = nullptr;

	/** 装备域资源加载、资源预热、实例生命周期与挂起自动装备组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroLoadoutRuntimeComponent* HeroLoadoutRuntimeComponent = nullptr;

	/** 装备域属性缓存镜像与发射物标签上下文组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroLoadoutContextComponent* HeroLoadoutContextComponent = nullptr;

	/** 装备域启动/预热状态机与 UI 快照桥组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroLoadoutStateComponent* HeroLoadoutStateComponent = nullptr;

	/** 装备域 direct/来源型外部命中效果生命周期组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroLoadoutEffectComponent* HeroLoadoutEffectComponent = nullptr;

	/** 攻击请求解析、攻击命中上下文与攻击收尾恢复组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroAttackComponent* HeroAttackComponent = nullptr;

	/** 闪避、防御与非攻击输入放行组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroDefenseComponent* HeroDefenseComponent = nullptr;

	/** 命中反馈、发射物生命周期反馈与 HUD 桥接组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroCombatFeedbackComponent* HeroCombatFeedbackComponent = nullptr;

	/** 固定武器槽切换事务与切武演出组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroWeaponSwitchComponent* HeroWeaponSwitchComponent = nullptr;

	/** 英雄侧处决协调组件，负责目标查询、预占、结算与处决后输入恢复。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroExecutionCoordinatorComponent* HeroExecutionCoordinatorComponent = nullptr;

	/** 命中来源组件，负责统一管理攻击命中窗口与窗口内目标去重。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroHitSourceComponent* HeroHitSourceComponent = nullptr;

	/** 轻量转向辅助与完整锁定目标组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroTargetingComponent* HeroTargetingComponent = nullptr;

	/** 身体整体命中体，适合空手默认推掌或整段身体碰撞的简单配置。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* CharacterBodyHitBox = nullptr;

	/** 左拳命中体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* LeftFistHitBox = nullptr;

	/** 右拳命中体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* RightFistHitBox = nullptr;

	/** 左脚命中体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* LeftFootHitBox = nullptr;

	/** 右脚命中体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* RightFootHitBox = nullptr;

	/** 左肘命中体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* LeftElbowHitBox = nullptr;

	/** 右肘命中体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* RightElbowHitBox = nullptr;

	/** 左膝命中体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* LeftKneeHitBox = nullptr;

	/** 右膝命中体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* RightKneeHitBox = nullptr;

	/** 属性变化时传递的中转站，负责向MVVM中传递数据。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Attribute, meta = (AllowPrivateAccess = "true"))
	UHeroAttributeComponent* HeroAttributeComponent = nullptr;

	/** 英雄默认负载配置资产，包含通用能力与固定武器槽配置。 */
	UPROPERTY(EditDefaultsOnly, Category = "ConfigData|Loadout")
	UDataAsset_HeroLoadoutData* HeroLoadoutData = nullptr;

	/** 通用负载能力是否已经授予完成，避免 PossessedBy / OnRep_PlayerState 重入时重复授予。 */
	bool bLoadoutDataApplied = false;

	/** 本地战斗输入是否已经绑定完成，避免角色在重初始化路径里重复绑定同一批输入回调。 */
	bool bCombatInputConfigured = false;

	bool bHeroASCInitialized = false;

	/** 启动链全部就绪后广播，适合 UI 关闭加载界面并开放交互。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|Startup")
	FOnHeroSystemsStartupReady OnHeroSystemsStartupReady;

	/** 启动链失败后广播，适合 UI 显示错误面板或停留在加载界面。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|Startup")
	FOnHeroSystemsStartupFailed OnHeroSystemsStartupFailed;

	/**
	 * 从负载配置资产构建一份规范化固定武器槽配置。
	 * 如果角色没配 HeroLoadoutData，也会在代码侧补出最小四槽壳层，
	 * 保证装备组件至少能拿到固定四槽的稳定顺序。
	 */
	void BuildNormalizedWeaponLoadoutDefinitions(TArray<FHeroWeaponLoadoutDefinition>& OutDefinitions) const;

private:
	friend class UHeroAssemblyComponent;
};
