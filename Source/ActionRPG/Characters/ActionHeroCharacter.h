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
 * 它是 Hero 宿主总装配壳与稳定门面，不是各条战斗子链、装备子链、startup 状态机或相机模式的正式状态源本体。
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
	/** ACharacter 生命周期回调。它们只负责角色宿主级初始化、装配时机与收尾，不在角色本体里平行维护第二套业务运行态。 */
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 初始化 ASC 与 AttributeSet 引用。它只建立角色与 ASC / AttributeSet 的宿主关系。 */
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
	 * 它只负责角色宿主级装配时机，不在这里重建各子链正式运行态。
	 */
	void InitializeHeroSystems();

public:
	/** 获取弹簧臂。 */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** 获取主相机。 */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** 获取相机逻辑组件。它是角色对外暴露的稳定相机宿主入口。 */
	UFUNCTION(BlueprintCallable, Category = "Camera", meta = (ToolTip = "获取英雄角色当前使用的相机逻辑组件。它是稳定宿主解析入口，不会在这里创建新的相机运行态。"))
	UHeroCameraComponent* GetHeroCameraComponent() const { return HeroCameraComponent; }

	/** 获取英雄侧战斗总控组件。它是蓝图和外围系统读取正式战斗状态的首选宿主入口。 */
	UFUNCTION(BlueprintPure, Category = "AbilitySystem", meta = (BlueprintThreadSafe, ToolTip = "获取英雄侧战斗总控组件。它是蓝图和外围系统读取正式战斗状态的首选宿主入口，不建议另造平行战斗状态。"))
	UHeroCombatComponent* GetHeroCombatComponent() const { return HeroCombatComponent; }

	/** 获取角色装配桥组件。它主要服务启动链协调、HUD 桥接和武器表现桥。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取角色装配桥组件。它主要负责启动链协调、HUD 初始化桥和武器表现桥，不直接替代战斗总控或装备状态源。"))
	UHeroAssemblyComponent* GetHeroAssemblyComponent() const { return HeroAssemblyComponent; }

	/** 获取战斗输入运行态组件。它是稳定宿主解析入口，不在角色本体里另持一份输入状态。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取战斗输入运行态组件。它持有缓冲输入、Held 回放和输入门禁运行态，不建议外层系统再维护第二套输入状态。"))
	UHeroCombatInputComponent* GetHeroCombatInputComponent() const { return HeroCombatInputComponent; }

	/** 获取装备组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取装备组件。它负责固定武器槽、武器实例与装备事务宿主，不应把只读 UI 快照误当成它的替代状态源。"))
	UHeroEquipmentComponent* GetHeroEquipmentComponent() const { return HeroEquipmentComponent; }

	/** 获取装备域资源加载、预热与实例生命周期组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取装备域资源加载、预热与实例生命周期组件。它主要服务启动链和装备域运行时装配，不直接表达当前正式已装备结果。"))
	UHeroLoadoutRuntimeComponent* GetHeroLoadoutRuntimeComponent() const
	{
		return HeroLoadoutRuntimeComponent;
	}

	/** 获取装备域属性镜像与发射物标签上下文组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取装备域属性镜像与发射物标签上下文组件。它主要服务上下文拼装和运行时镜像，不替代顶层 WeaponDefinition 语义入口。"))
	UHeroLoadoutContextComponent* GetHeroLoadoutContextComponent() const
	{
		return HeroLoadoutContextComponent;
	}

	/** 获取装备域启动状态机与 UI 快照桥组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取装备域启动状态机与 UI 快照桥组件。它适合读取启动链状态和 UI 快照，但不要把 UI 快照反向当成正式状态源。"))
	UHeroLoadoutStateComponent* GetHeroLoadoutStateComponent() const
	{
		return HeroLoadoutStateComponent;
	}

	/** 获取装备域外部命中效果生命周期组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取装备域外部命中效果生命周期组件。它负责额外命中效果的运行时聚合和生命周期管理，不是新的静态资产入口。"))
	UHeroLoadoutEffectComponent* GetHeroLoadoutEffectComponent() const
	{
		return HeroLoadoutEffectComponent;
	}

	/** 获取攻击组件。它只是稳定宿主解析入口，正式攻击执行期快照仍回到组件自身。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取攻击组件。它负责攻击请求解析、攻击上下文和攻击收尾恢复，是攻击主链的正式宿主之一。"))
	UHeroAttackComponent* GetHeroAttackComponent() const { return HeroAttackComponent; }

	/** 获取防御系输入组件。它只是稳定宿主解析入口。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取防御组件。它负责闪避、防御和非攻击输入放行，不建议在外层另写平行防御状态。"))
	UHeroDefenseComponent* GetHeroDefenseComponent() const { return HeroDefenseComponent; }

	/** 获取英雄侧通用战斗反馈组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取英雄侧通用战斗反馈组件。它主要服务命中反馈、发射物反馈和 HUD 桥接，不持有主战斗状态源。"))
	UHeroCombatFeedbackComponent* GetHeroCombatFeedbackComponent() const { return HeroCombatFeedbackComponent; }

	/** 获取切武组件。它只是稳定宿主解析入口，切武请求 / 事务 / 表现期状态仍回到组件自身。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取切武组件。它负责固定武器槽切换事务和切武演出，不建议把切武请求壳或 UI 快照当成它的替代状态。"))
	UHeroWeaponSwitchComponent* GetHeroWeaponSwitchComponent() const { return HeroWeaponSwitchComponent; }

	/** 获取英雄侧处决协调组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取英雄侧处决协调组件。它负责目标查询、预占、结算和处决后恢复，是处决主链的正式宿主入口。"))
	UHeroExecutionCoordinatorComponent* GetHeroExecutionCoordinatorComponent() const { return HeroExecutionCoordinatorComponent; }

	/** 获取命中来源窗口组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取命中来源窗口组件。它负责命中窗口和窗口内目标去重，不等于单次命中结果快照本身。"))
	UHeroHitSourceComponent* GetHeroHitSourceComponent() const { return HeroHitSourceComponent; }

	/** 获取轻量转向辅助与锁定目标组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取轻量转向辅助与锁定目标组件。它主要服务目标选择和转向辅助，不替代攻击或处决资格本身。"))
	UHeroTargetingComponent* GetHeroTargetingComponent() const { return HeroTargetingComponent; }

	/** 获取属性变化的传递组件。 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取属性变化桥接组件。它主要把正式属性变化传递到 MVVM 和 UI 侧，不持有新的属性状态源。"))
	UHeroAttributeComponent* GetHeroAttributeComponent() const { return HeroAttributeComponent; }

	/** 设置角色当前是否存在移动输入。 */
	void SetHasMoveInput(bool bInHasMoveInput);

	/** 查询角色当前是否存在移动输入。它只读角色层输入镜像，不替代 CombatInputComponent 的正式输入运行态。 */
	bool HasMoveInput() const { return bHasMoveInput; }

	/** 角色当前是否应正式启用武器 LinkedLayer 表现。它只回答表现层是否该启用，不替代正式装备状态。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Weapon", meta = (ToolTip = "查询角色当前是否应正式启用武器 LinkedLayer 表现。它只回答表现层是否该启用，不替代正式已装备状态或动画资产本身。"))
	bool ShouldUseWeaponLinkedLayer() const;

	/** 读取角色当前正式武器层类。它返回的是当前应消费的表现层类，不是新的装备状态源。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Weapon", meta = (ToolTip = "读取角色当前应消费的正式武器 LinkedLayer 类。它主要服务动画层挂接，不是新的装备状态源。"))
	TSubclassOf<UActionHeroLinkedAnimLayer> GetCurrentWeaponLinkedAnimLayerClass() const;

	/** 查询固定武器槽启动链是否已经完整就绪。它是角色层稳定查询入口，底层正式状态仍回到装配桥与 startup 状态机。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup", meta = (ToolTip = "查询固定武器槽启动链是否已经完整就绪。它回答的是正式启动状态，而不是 UI 自己推测的可交互状态。"))
	bool IsHeroSystemsStartupReady() const;

	/** 查询角色固定武器槽启动链是否仍在执行中，可用于加载界面是否继续显示。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup", meta = (ToolTip = "查询角色固定武器槽启动链是否仍在执行中。它适合驱动加载界面是否继续显示，但不返回更细的槽位明细。"))
	bool IsHeroSystemsStartupInProgress() const;

	/** 查询角色固定武器槽启动链是否已经失败。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup", meta = (ToolTip = "查询角色固定武器槽启动链是否已经失败。它只回答是否失败，具体原因继续看 FailureReason。"))
	bool HasHeroSystemsStartupFailed() const;

	/** 查询最近一次启动失败原因，供 UI 直接显示错误文本。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup", meta = (ToolTip = "查询最近一次启动失败原因，适合 UI 直接展示。留空通常表示当前没有记录到失败原因。"))
	FString GetHeroSystemsStartupFailureReason() const;

	/** 查询启动链总体状态枚举，适合在蓝图里做分支判断。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup", meta = (ToolTip = "查询启动链总体状态枚举。它是正式状态机快照，适合在蓝图里做分支判断。"))
	EHeroWeaponLoadoutStartupState GetHeroSystemsStartupState() const;

	/** 查询启动链进度比例，返回值范围为 0 到 1。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup", meta = (ToolTip = "查询启动链进度比例，返回值范围通常为 0 到 1。它是只读进度快照，不会推动启动链继续执行。"))
	float GetHeroSystemsStartupProgressRatio() const;

	/** 查询当前还剩多少固定武器槽没有完成启动预热。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup", meta = (ToolTip = "查询当前还剩多少固定武器槽没有完成启动预热。它只服务 UI 和调试显示。"))
	int32 GetHeroSystemsStartupPendingSlotCount() const;

	/** 查询这次启动一共需要预热多少固定武器槽。 */
	UFUNCTION(BlueprintPure, Category = "Action|Startup", meta = (ToolTip = "查询这次启动一共需要预热多少固定武器槽。它只服务进度显示和调试统计。"))
	int32 GetHeroSystemsStartupTotalSlotCount() const;

	/** 构建当前固定四槽 UI 所需的只读聚合快照。它只是角色层桥接结果，不是新的正式状态源。 */
	UFUNCTION(BlueprintPure, Category = "Action|UI", meta = (ToolTip = "构建当前固定四槽 UI 所需的只读聚合快照。它只服务 HUD 和 MVVM 展示，不反向作为正式运行时状态源。"))
	bool BuildHeroLoadoutUISnapshot(FHeroWeaponLoadoutUISnapshot& OutSnapshot) const;

	/** 供加载失败界面手动重试启动链。它属于恢复入口，不是新的启动状态源。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Startup", meta = (ToolTip = "供加载失败界面手动重试启动链。它会重新尝试固定槽加载、预热和出生装备，不会把 UI 快照当成正式恢复入口。"))
	bool RetryHeroSystemsStartup();

	/** 供加载失败界面重新进入当前关卡。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Startup", meta = (ToolTip = "供加载失败界面重新进入当前关卡。它属于外层恢复入口，会从关卡层整体重建当前角色和启动链。"))
	bool ReloadCurrentLevel();

	/** 打印当前玩家身上实际授予的 Hero 战斗 GA 关系配置摘要，便于核对蓝图默认值是否与 C++ 基线一致。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability|Debug", meta = (ToolTip = "打印当前玩家身上实际授予的 Hero 战斗 GA 关系配置摘要。它只服务调试核对，不会修改任何正式能力状态。"))
	void PrintGrantedHeroCombatAbilityRelationshipAudit() const;

	/** 打印当前玩家身上实际授予的 Hero 战斗 GA 类别审计摘要，便于核对顶层身份标签、类别解析和矩阵规则是否完整。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability|Debug", meta = (ToolTip = "打印当前玩家身上实际授予的 Hero 战斗 GA 类别审计摘要。它只服务调试核对，不会修正或阻断任何 Ability。"))
	void PrintGrantedHeroCombatAbilityCategoryAudit() const;

	/** 按输入标签打印指定 Hero 战斗 GA 的当前调试摘要，便于在 OpenMap 里解释“为什么能放/为什么不能放”。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability|Debug", meta = (ToolTip = "按输入标签打印指定 Hero 战斗 GA 的当前调试摘要。它只解释当前关系门禁和受击上下文，不会推进能力激活。"))
	bool PrintHeroCombatAbilityDebugByInputTag(FGameplayTag AbilityInputTag) const;

	/** 按输入标签打印指定 Hero 战斗 GA 的类别审计摘要，便于快速定位身份标签与类别解析问题。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability|Debug", meta = (ToolTip = "按输入标签打印指定 Hero 战斗 GA 的类别审计摘要。它只解释身份标签、类别解析和矩阵规则，不会推进能力激活。"))
	bool PrintHeroCombatAbilityCategoryAuditByInputTag(FGameplayTag AbilityInputTag) const;

	/** 打印最近几次 Hero 战斗 GA 关系主链失败历史，便于快速回看 residual drift 与尾部激活失败样本。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability|Debug", meta = (ToolTip = "打印最近几次 Hero 战斗 GA 关系主链失败历史。它只服务调试排错，不持久化运行时失败记录。"))
	void PrintHeroCombatAbilityRelationshipFailureHistory(int32 MaxEntries = 8) const;

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
	/** 角色当前是否有移动输入，用于闪避方向与动作选择。它是 Hero 角色侧的输入镜像，不替代 CombatInputComponent 的正式输入运行态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (ToolTip = "角色层当前是否存在移动输入的只读镜像。它主要服务闪避方向、动画选择和角色侧便捷查询，不替代 HeroCombatInputComponent 的正式输入运行态。"))
	bool bHasMoveInput = false;

protected:
	// 总控与装配桥：
	// 这一组组件负责把正式战斗状态、启动链和角色桥接入口串起来。
	/** 输入与状态分发核心组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroCombatComponent* HeroCombatComponent = nullptr;

	/** 角色装配桥组件，承接启动链、HUD 与武器表现桥。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UHeroAssemblyComponent* HeroAssemblyComponent = nullptr;

	// 装备域运行时：
	// 这一组组件负责固定四槽、装备域上下文、启动状态机与外部效果生命周期。
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

	// 主战斗子链：
	// 这一组组件分别承接攻击、防御、反馈、切武、处决与命中来源等正式子链宿主。
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

	// 身体命中来源：
	// 这些组件是角色层稳定存在的内建命中体，不等于某次命中窗口一定已经正式启用。
	/** 身体整体命中体，适合空手默认推掌或整段身体碰撞的简单配置。它只是内建命中体引用，不等于当前窗口已正式开启。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitSource")
	UBoxComponent* CharacterBodyHitBox = nullptr;

	/** 左拳命中体。它只是角色层内建挂点，不是命中源正式激活集。 */
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

	/** 属性变化时的中转组件，负责把正式属性变化桥接到 MVVM/UI 侧。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Attribute, meta = (AllowPrivateAccess = "true"))
	UHeroAttributeComponent* HeroAttributeComponent = nullptr;

	// 资产入口与初始化保护标志：
	// 这一组字段只保护角色初始化/装配边界，不表达新的正式业务状态源。
	/** 英雄默认负载配置资产，包含通用能力与固定武器槽配置。它是角色宿主级静态入口，不是运行时状态。 */
	UPROPERTY(EditDefaultsOnly, Category = "ConfigData|Loadout", meta = (ToolTip = "英雄默认负载配置资产，包含通用能力、固定四槽武器定义和槽位能力模板。"))
	UDataAsset_HeroLoadoutData* HeroLoadoutData = nullptr;

	/** 通用负载能力是否已经授予完成，避免 PossessedBy / OnRep_PlayerState 重入时重复授予。 */
	bool bLoadoutDataApplied = false;

	/** 本地战斗输入是否已经绑定完成，避免角色在重初始化路径里重复绑定同一批输入回调。 */
	bool bCombatInputConfigured = false;

	/** ASC 与 AttributeSet 是否已经完成角色侧初始化，避免重复走 InitAbilitySystem。 */
	bool bHeroASCInitialized = false;

	/** 启动链全部就绪后广播，适合 UI 关闭加载界面并开放交互。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|Startup", meta = (ToolTip = "固定武器槽启动链全部就绪后广播。它主要服务 HUD 关闭加载界面和开放交互，不形成新的启动状态源。"))
	FOnHeroSystemsStartupReady OnHeroSystemsStartupReady;

	/** 启动链失败后广播，适合 UI 显示错误面板或停留在加载界面。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|Startup", meta = (ToolTip = "固定武器槽启动链失败后广播。它主要服务 UI 显示失败槽位和原因，不替代正式启动状态查询接口。"))
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
