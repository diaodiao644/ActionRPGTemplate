#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "ActionType/ActionCombatRuntimeTypes.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "ActionType/ActionEffectTypes.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "HeroCombatComponent.generated.h"

class AActionHeroCharacter;
class AActionPlayerController;
class AHeroWeaponBase;
class UActionAbilitySystemComponent;
class UActionAttributeSetBase;
class UActionCombatReactComponent;
class UActionHeroLinkedAnimLayer;
class UAnimInstance;
class UExecutionWindowComponent;
class UAnimMontage;
class UDataAsset_InputConfig;
class UHeroAttackComponent;
class UHeroDefenseComponent;
class UHeroCombatInputComponent;
class UHeroExecutionCoordinatorComponent;
class UHeroHitSourceComponent;
class UHeroLoadoutStateComponent;
class UHeroTargetingComponent;
class UHeroWeaponSwitchComponent;
class UInputMappingContext;
class UHeroEquipmentComponent;

DECLARE_MULTICAST_DELEGATE(FOnHeroCombatUIStateChanged);

/**
 * 英雄战斗组件。
 * 主要职责如下：
 * 1. 作为角色战斗输入的统一入口，负责按输入事件驱动攻击、闪避、防御、处决与切武。
 * 2. 维护战斗窗口、切武事务、当前武器快照等运行时状态。
 * 3. 作为装备组件与战斗表现层之间的桥接层，把武器定义数据转成可直接执行的战斗逻辑。
 */
UCLASS()
class ACTIONRPG_API UHeroCombatComponent : public UPawnCombatComponent
{
	GENERATED_BODY()

	friend class UHeroAttackComponent;
	friend class UHeroCombatInputComponent;
	friend class UHeroDefenseComponent;
	friend class UHeroExecutionCoordinatorComponent;
	friend class UHeroWeaponSwitchComponent;
	friend class UActionCombatReactComponent;

public:
	UHeroCombatComponent();

public:
	/** 高层武器上下文与切武门面。*/

	/**
	 * 初始化固定武器槽，并同步当前武器快照。
	 * 这里是战斗组件介入装备链的起点：
	 * 1. 把角色预设的固定四槽定义正式交给装备组件；
	 * 2. 让装备组件完成默认武器准备、异步加载与实例初始化；
	 * 3. 在加载结果稳定后，把“当前真正装备的是哪把武器”同步回战斗组件缓存。
	 * 之后攻击、闪避、防御、处决等所有数据驱动查询，都会建立在这次初始化结果之上。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat|Weapon")
	void InitializeWeaponLoadout(const TArray<FHeroWeaponLoadoutDefinition>& InWeaponLoadoutDefinitions);

	/**
	 * 请求装备指定固定武器槽。
	 * 这个入口面向“我要切到哪个固定槽位”的高层语义，不关心底层是同步完成还是异步加载完成：
	 * 1. 如果目标槽位资源已经就绪，可能会立刻完成装备；
	 * 2. 如果目标槽位仍在加载，则会进入装备组件的异步装备链；
	 * 3. 只要请求已被受理并会继续推进，就返回 true。
	 * 它通常会被切武 GA、启动默认装备或其它需要显式换槽的流程调用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat|Weapon")
	bool EquipWeaponByLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 读取当前已装备的武器实例。*/
	UFUNCTION(BlueprintCallable, Category = "Action|Combat|Weapon")
	AHeroWeaponBase* GetCurrentEquippedWeapon() const;

	/** 读取当前已装备武器对应的武器定义。*/
	UFUNCTION(BlueprintCallable, Category = "Action|Combat|Weapon")
	UDataAsset_WeaponDefinition* GetCurrentWeaponDefinition() const;

	/** 当前是否应正式启用武器 LinkedLayer 表现。*/
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Weapon")
	bool HasEquippedWeaponLinkedLayerPresentation() const;

	/** 当前已装备武器是否应挂在手持 WeaponSocket。它只表达表现挂点，不等价于重新装备武器。*/
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Weapon")
	bool ShouldCurrentEquippedWeaponUseWeaponSocketPresentation() const;

	/** 读取当前正式应挂接的武器 LinkedLayer 类。*/
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Weapon")
	TSubclassOf<UActionHeroLinkedAnimLayer> GetCurrentWeaponLinkedAnimLayerClass() const;

	/** Idle 下主动切入普通战斗姿态，并按当前武器配置推进正式 Combat 表现过渡。*/
	bool TryEnterCombatModeFromIdle();

	/** 主动链已正式起手后，立即把公共战斗姿态切到 Combo，并复用现有武器挂点表现语义。*/
	void EnterComboModeImmediatelyForActivePresentation();

	/** 供 Combat 表现过渡 Notify 精确驱动当前已装备武器切挂点。它只负责精确时机，不承担 Combat 状态机职责。*/
	void NotifyCombatWeaponPresentationSwitchFrame(bool bAttachToWeaponSocket);

	/** 切武收尾时统一修复残留战斗态，并把镜头刷新回当前真实表现态。 */
	void FinalizeWeaponSwitchRuntimeState();

	/** 供摄像机与高层状态判断读取：当前是否仍处于切武表现播放期。*/
	bool IsWeaponSwitchPresentationActive() const;

	/** 供摄像机与高层状态判断读取：当前是否处于特殊切武表现播放期。*/
	bool IsSpecialWeaponSwitchPresentationActive() const;

	/** 供表现层诊断读取：当前是否仍处于 CombatMode 过渡壳。*/
	bool IsCombatModeTransitionPresentationActive() const;

	/** 开始一段正式攻击侧共享伤害上下文。 */
	void BeginActiveDamageContext(
		int32 InAbilityLevel,
		const FGameplayTag& InSourceAbilityTag);

	/** 清空当前活跃的攻击侧共享伤害上下文。 */
	void ClearActiveDamageContext();

	/** 读取当前是否存在一条活跃的正式伤害上下文。 */
	bool TryGetActiveDamageContext(FActionDamageContextRuntimeState& OutDamageContext) const;

	/** 让切武子组件从装备组件拉取一次当前武器状态，用于初始化本组件的武器快照。*/
	void InitializeCurrentWeaponStateFromEquipment();

	/** 启动链重置时清空会影响首次攻击的战斗运行时状态。*/
	void ResetRuntimeStateForHeroStartup();

public:
	/**
	 * 请求在处决结束后的下一帧恢复战斗输入处理。
	 * 处决链同时涉及执行者保护、受害者锁定、命中提交与演出收尾，
	 * 恢复输入必须放在整条链彻底收尾之后，避免玩家在处决尾帧误触发下一段动作。
	 */
	void RequestRecoverCombatInputAfterExecution();

public:
	/** 武器数据驱动查询接口。*/

	/** 读取当前正在播放的攻击蒙太奇。*/
	virtual UAnimMontage* GetCurrentRunningAnimMontage() const override;

	/** 读取当前武器在指定战斗模式下的切入蒙太奇。*/
	virtual UAnimMontage* GetCombatModeTransitionAnimMontage() override;

public:
	/** 所有者与依赖组件访问接口。*/

	/**
	 * 获取拥有者英雄角色。
	 * HeroCombatComponent 不直接创建或拥有角色对象，
	 * 而是把角色当作所有外部依赖系统的统一入口。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	AActionHeroCharacter* GetOwningHeroCharacter() const;

	/**
	 * 获取拥有者玩家控制器。
	 * 主要服务于输入、移动状态与部分本地玩家战斗语义查询。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	AActionPlayerController* GetOwningHeroController() const;

	/**
	 * 获取拥有者 ASC。
	 * 战斗输入最终提交给 AbilitySystem、战斗事件也会经由 ASC 继续流转，
	 * 因此这里是组件侧接入 GAS 的核心访问点。
	 * 该入口与其它依赖访问口保持一致：优先使用本地弱引用缓存，缓存失效时再回到角色取值。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	UActionAbilitySystemComponent* GetOwningActionAbilitySystemComponent() const;

	/**
	 * 获取拥有者装备组件。
	 * 当前武器定义、固定武器槽状态与切武真实落地结果，都以装备组件为权威来源。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	UHeroEquipmentComponent* GetOwningHeroEquipmentComponent() const;

	/**
	 * 获取拥有者切武组件。
	 * 固定武器槽切换请求、切武事务和切武演出状态统一由该组件维护。
	 * HeroCombatComponent 只保留高层门面与跨链判断，不再直接持有其内部事务细节。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	UHeroWeaponSwitchComponent* GetOwningHeroWeaponSwitchComponent() const;

	/**
	 * 获取拥有者攻击组件。
	 * 攻击请求解析、攻击命中上下文与攻击收尾恢复统一由该组件维护。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	UHeroAttackComponent* GetOwningHeroAttackComponent() const;

	/**
	 * 获取拥有者防御组件。
	 * 闪避、防御与非攻击输入放行判断统一由该组件维护。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	UHeroDefenseComponent* GetOwningHeroDefenseComponent() const;

	/**
	 * 获取拥有者英雄侧处决协调组件。
	 * 处决目标查询、目标预占、处决伤害结算与处决后输入恢复统一由该组件维护。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	UHeroExecutionCoordinatorComponent* GetOwningHeroExecutionCoordinatorComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	UHeroHitSourceComponent* GetOwningHeroHitSourceComponent() const;

	/**
	 * 获取拥有者属性集。
	 * 攻击力、能量、体力等战斗数值最终都以属性集为准，
	 * 战斗组件只读取和组合这些结果，不复制一套本地数值状态。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	UActionAttributeSetBase* GetOwningActionAttributeSet() const;

public:
	/** 输入系统与输入缓冲接口。*/

	/**
	 * 绑定战斗输入映射与输入回调。
	 * 组件启动后需要通过这里把输入配置资产里的 Tag、InputAction 与本组件内部处理函数接起来，
	 * 这样后续 Pressed / Held / Released 三种输入阶段，才能统一进入战斗输入状态机。
	 */
	void HandleCombatInputConfig();

	/**
	 * 写入一条新的输入缓冲记录。
	 * 当当前帧因为攻击窗口、受击恢复、切武演出等原因不能立刻执行输入时，
	 * 会把那次输入先保存为一份“待稍后重放”的运行时快照。
	 * 其中不仅保存 InputTag 和输入阶段，还会尽量把当时已解析出的攻击请求标签一起保存，
	 * 以减少后续延迟消费时上下文已经变化导致的分支漂移。
	 */
	void QueueBufferedInput(
		FGameplayTag InputTag,
		EActionInputEvent InputEvent,
		FGameplayTag ResolvedAttackRequestTag = FGameplayTag(),
		int32 BufferedInputOrder = 0);

public:
	/** 对外战斗事件、受击与状态窗口接口。*/

	/** 向 ASC 与外部监听方广播一条战斗事件。*/
	void BroadcastCombatEvent(FGameplayTag EventTag) const;

	/** 处理外部发来的通用战斗事件。*/
	virtual bool HandleIncomingCombatEvent(FGameplayTag InCombatEventTag, AActor* InstigatorActor) override;

	/** 处理一份已经构造好的受击载荷。*/
	virtual bool TryHandleIncomingDamage(const FActionDamagePayload& InDamagePayload, FActionHitResolveResult& OutResult) override;

	/** 当前是否处于防御状态。*/
	bool IsDefenseActive() const { return WindowRuntimeState.IsDefenseActive(); }

	/** 当前精确格挡窗口是否开启。*/
	bool IsParryWindowActive() const { return WindowRuntimeState.IsParryWindowActive(); }

	/** 当前是否仍有任意防御链状态未回收。 */
	bool HasAnyDefenseStateActive() const { return IsDefenseActive() || IsParryWindowActive(); }

	/** 写入防御主状态是否激活。 */
	void SetDefenseStateActive(bool bActive) { WindowRuntimeState.SetDefenseActive(bActive); }

	/** 写入精确格挡窗口是否激活。 */
	void SetParryWindowStateActive(bool bActive) { WindowRuntimeState.SetParryWindowActive(bActive); }

	/** 当前是否处于闪避状态。*/
	bool IsDodgeActive() const { return WindowRuntimeState.IsDodgeActive(); }

	/** 当前完美闪避窗口是否开启。*/
	bool IsPerfectDodgeWindowActive() const { return WindowRuntimeState.IsPerfectDodgeWindowActive(); }

	/** 当前是否拥有闪避反击资格。 */
	bool IsDodgeCounterAvailable() const { return WindowRuntimeState.IsDodgeCounterAvailable(); }

	/** 当前是否仍有任意闪避链状态未回收。 */
	bool HasAnyDodgeStateActive() const { return IsDodgeActive() || IsPerfectDodgeWindowActive(); }

	/** 写入闪避主状态是否激活。 */
	void SetDodgeStateActive(bool bActive) { WindowRuntimeState.SetDodgeActive(bActive); }

	/** 写入完美闪避窗口是否激活。 */
	void SetPerfectDodgeWindowStateActive(bool bActive) { WindowRuntimeState.SetPerfectDodgeWindowActive(bActive); }

	/** 写入闪避反击资格是否可用。 */
	void SetDodgeCounterStateAvailable(bool bAvailable) { WindowRuntimeState.SetDodgeCounterAvailable(bAvailable); }

	/** 当前是否正在执行“受击链接管前的战斗状态强制回收”。 */
	bool IsCombatReactStateResetInProgress() const { return bCombatReactStateResetInProgress; }

	/**
	 * 读取攻击衔接窗口是否处于开启状态。
	 * 这个窗口用于约束“当前演出中的下一段攻击输入”能否正式接招，
	 * 当前既服务普通攻击连段，也服务切武表现期里的攻击接段，
	 * 但不负责普通动作取消，也不等价于任意战斗输入都可以立刻放行。
	 */
	bool IsAbilityChainWindowActive() const { return AbilityWindowRuntimeState.IsAbilityChainWindowActive(); }

	/** 判断当前攻击衔接窗口是否明确接受指定输入。 */
	bool CanAcceptAbilityChainInput(const FGameplayTag& InputTag) const
	{
		return AbilityWindowRuntimeState.IsAbilityChainWindowActive()
			&& AbilityWindowRuntimeState.AcceptsChainInput(InputTag);
	}

	/**
	 * 读取通用动作取消窗口是否处于开启状态。
	 * 当攻击、处决等能力仍处于正式演出阶段时，后续是否允许插入闪避、防御、切武等动作，
	 * 由这个窗口和其输入白名单共同决定。
	 */
	bool IsAbilityCancelWindowActive() const { return AbilityWindowRuntimeState.IsAbilityCancelWindowActive(); }

	/**
	 * 打开攻击衔接窗口，并登记本次允许接招的输入白名单。
	 * InAllowedInputTags 只表示哪些输入可以作为“下一段攻击”被接受，
	 * bConsumeBufferedInputImmediately 为真时，会在窗口刚打开时立即尝试消费已缓冲的预输入，
	 * 以支持玩家提前按下下一段攻击但仍能在合法衔接帧进入连段或切武接段。
	 */
	void OpenAbilityChainWindow(const FGameplayTagContainer& InAllowedInputTags, bool bConsumeBufferedInputImmediately);

	/** 关闭攻击衔接窗口，阻止当前连段继续接受新的接招输入。 */
	void CloseAbilityChainWindow();

	/**
	 * 打开通用动作取消窗口，并登记本次允许插入的输入白名单。
	 * 该窗口主要服务于“攻击中插闪避 / 攻击中接防御 / 处决中允许后续动作”等取消链，
	 * 用于把哪些输入能打断当前演出控制在明确范围内。
	 */
	void OpenAbilityCancelWindow(const FGameplayTagContainer& InAllowedInputTags);

	/** 关闭通用动作取消窗口，恢复为当前演出不再接受取消插入的状态。 */
	void CloseAbilityCancelWindow();

	/**
	 * 按组件侧持久运行态，解析这次 Spirit 输入真正应该播放的技能段。
	 * 这个入口只回答“本次从哪一段开始”，不负责推进索引，也不决定是否进入待命。
	 */
	int32 ResolveSpiritSkillClipIndexToPlay(
		const FGameplayTag& SpiritInputTag,
		const FActionSpiritSkillConfig& SpiritSkillConfig,
		int32 SkillClipCount) const;

	/** 读取当前这条 Spirit 连段是否已经正式提交过一次成本。 */
	bool HasCommittedSpiritSkillChainCost(const FGameplayTag& SpiritInputTag) const;

	/**
	 * 当前 Spirit 技能段成功起手后，同步组件侧持久状态。
	 * 这里会统一记录成本是否已提交、下一次待命索引、总段数快照与超时配置，
	 * 让 GA 不必长期持有这些跨激活状态。
	 */
	void HandleSpiritSkillClipStarted(
		const FGameplayTag& SpiritInputTag,
		const FActionSpiritSkillConfig& SpiritSkillConfig,
		int32 StartedClipIndex,
		int32 SkillClipCount);

	/**
	 * 当前 Spirit 技能段未直接接上下一段时，转入组件侧待命态并启动超时。
	 * 这里只建立“稍后还能否续段”的正式状态，不负责任何 GA 生命周期控制。
	 */
	bool BeginWaitingForNextSpiritSkillClip(const FGameplayTag& SpiritInputTag);

	/** 重置指定 Spirit 输入对应的持久连段状态，并清理其超时计时器与残留待命态。 */
	void ResetSpiritSkillComboState(const FGameplayTag& SpiritInputTag);

	/** 重置全部 Spirit 输入对应的持久连段状态。用于切武、启动重置或全局清战斗态。 */
	void ResetAllSpiritSkillComboStates();

public:
	/** 处决接口。*/

	/**
	 * 判断指定目标当前是否完整满足处决资格。
	 * 这个入口会同时判断：
	 * 1. 目标是否存在可消费的处决窗口；
	 * 2. 当前执行者是否是窗口允许的那一方；
	 * 3. 当前是否仍处于处决有效范围内；
	 * 4. 当前武器与目标侧被处决演出配置是否完整。
	 * 它不负责搜索目标，也不负责真正写入预占状态。
	 */
	bool CanExecuteTarget(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;

	/**
	 * 判断当前是否能激活处决能力，并返回可处决目标。
	 * 这是 Hero 侧发起处决前最完整的一次前置资格查询：
	 * 1. 先检查自己当前战斗状态是否允许进入处决；
	 * 2. 再搜索并筛选一个当前最合适的目标；
	 * 3. 最后把可用目标回传给处决 Ability，避免 Ability 自己再重复走一遍搜索链。
	 * 它只负责“现在能不能开始处决”，不负责真正预占或结算处决。
	 */
	bool CanActivateExecutionAbility(AActor*& OutTargetActor, FString* OutFailureReason = nullptr) const;

	/**
	 * 处决 Ability 已经正式激活后的二次运行时复核。
	 * 这里只重新确认最新目标、范围、窗口和配置仍然有效，
	 * 不再重复经过非攻击输入门禁，避免处决在自己刚进入 Active 状态后被反向挡回去。
	 */
	bool RevalidateExecutionAbilityAfterActivation(
		AActor*& OutTargetActor,
		FString* OutFailureReason = nullptr) const;

	/**
	 * 搜索一个当前可用的处决目标。
	 * 这里负责解决“面前可能有多个潜在目标时，当前应该选谁”的问题，
	 * 通常会结合距离、朝向、窗口状态与目标资格做统一筛选。
	 */
	bool TryFindExecutionTarget(AActor*& OutTargetActor, FString* OutFailureReason = nullptr) const;

	/**
	 * 预占一个处决目标。
	 * 预占成功后，表示本次处决演出已经正式拿到了目标的窗口使用权：
	 * 1. 其它执行者不能再抢这个目标；
	 * 2. 目标会进入受害者锁定保护；
	 * 3. 后续正式命中时可以继续走提交与伤害结算链。
	 */
	bool TryReserveExecutionTarget(AActor* InTargetActor) const;

	/**
	 * 正式提交一个处决目标。
	 * 这一步对应“处决已经进入命中 / 结算阶段”，
	 * 用于把之前的预占窗口真正消费掉，避免同一窗口被重复使用。
	 * 和 TryReserveExecutionTarget 的区别是：
	 * 预占只拿资格，提交才意味着这次处决已经不可逆地进入正式结算链。
	 */
	bool TryCommitExecutionTarget(AActor* InTargetActor) const;

	/**
	 * 取消对处决目标的预占。
	 * 常见场景包括：
	 * 1. 处决 Ability 中途取消；
	 * 2. 演出打断后需要把窗口资格还回目标；
	 * 3. 已经拿到目标，但最终没有进入正式命中阶段。
	 */
	void CancelReservedExecutionTarget(AActor* InTargetActor) const;

	/**
	 * 释放处决受害者锁定。
	 * 这个入口只负责“目标不再受当前执行者处决保护约束”，
	 * 通常在取消、异常中断或整条处决链结束后的收尾阶段调用。
	 */
	void ReleaseExecutionTargetVictimLock(AActor* InTargetActor) const;

	/** 处决已进入双边演出但未命中时，关闭目标演出链并恢复目标韧性。 */
	void AbortConsumedExecutionTargetPresentation(AActor* InTargetActor) const;

	/**
	 * 对已预占的处决目标结算处决伤害。
	 * 这里只处理“正式处决命中”的伤害载荷构建与受击解析，
	 * 不再负责资格搜索或窗口预占，避免一条链里把搜索、锁定、提交、伤害混在一起。
	 * 也就是说，这个入口被调用时，默认当前已经拥有目标窗口，并且正处于处决命中帧。
	 */
	bool TryExecuteReservedExecutionTarget(AActor* InTargetActor, FActionHitResolveResult& OutResolveResult) const;

	/**
	 * 读取当前武器的处决蒙太奇。
	 * 这样处决演出仍然保持武器数据驱动：
	 * 1. 不同武器槽可播放不同处决动画；
	 * 2. HeroCombatComponent 只负责统一查询，不把具体动画写死在能力里。
	 */
	UAnimMontage* GetExecutionMontageForCurrentWeapon() const;

	/** 读取当前武器要求目标在正式开播前转向执行者的时长。 */
	float GetExecutionVictimTurnDurationForCurrentWeapon() const;

	/** 读取当前武器要求双边处决开播前保持的水平距离。 */
	float GetExecutionStartDistanceForCurrentWeapon() const;

	/** 判断已预占目标当前是否仍允许继续推进到正式处决演出。 */
	bool CanStartReservedExecutionPresentation(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;

	/** 通知目标侧开始“处决前转向 + 被处决演出准备”链。 */
	bool TryBeginExecutionTargetPreparation(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;

	/** 查询目标侧的处决前准备是否已经完成。 */
	bool IsExecutionTargetPreparationReady(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;

	/** 在目标侧准备完成后，正式启动被处决演出。 */
	bool TryStartExecutionTargetPresentation(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;

	/** 输出当前已预占目标的处决前准备运行态说明。 */
	bool DescribeExecutionTargetPreparationState(
		AActor* InTargetActor,
		FString& OutDescription,
		FString* OutFailureReason = nullptr) const;

public:
	/** 战斗资源接口。*/

	/** 消耗一次特殊切武所需能量。*/
	bool ConsumeSpecialWeaponSwitchEnergy();

	/** 战斗侧 UI 状态变化事件。 */
	FOnHeroCombatUIStateChanged& OnCombatUIStateChanged() { return CombatUIStateChangedDelegate; }

	/** 供 HUD / MVVM 查询当前切武冷却是否仍在阻挡。 */
	bool IsWeaponSwitchBlockedByCooldownForUI() const { return IsWeaponSwitchBlockedByCooldown(); }

protected:
	/** 生命周期。*/

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void SetCombatMode(EHeroCombatMode InCombatMode) override;

protected:
	/** 原始输入入口。*/

	/**
	 * 接收一次 Pressed 阶段的战斗输入回调。
	 * 这里是增强输入系统进入战斗组件后的第一站，
	 * 主要负责把“刚按下”这一次输入写进统一输入状态机。
	 */
	UFUNCTION()
	void HandleCombatInputPressed(const FGameplayTag InputTag);

	/**
	 * 接收一次 Held 阶段的战斗输入回调。
	 * 这一层不直接假设 Held 一定成立，
	 * 而是需要结合按住时长与当前输入状态来决定是否正式升级成长按语义。
	 */
	UFUNCTION()
	void HandleCombatInputHeld(const FGameplayTag InputTag);

	/**
	 * 接收一次 Released 阶段的战斗输入回调。
	 * Released 不仅是“松开了键”，
	 * 也承担一次输入生命周期最终清理与部分能力收尾通知的职责。
	 */
	UFUNCTION()
	void HandleCombatInputReleased(const FGameplayTag InputTag);

protected:
	/**
	 * 输入总分发链。
	 * 这一层是 HeroCombatComponent 真正的输入语义核心：
	 * 1. 上游只把 Pressed / Held / Released + InputTag 交进来；
	 * 2. 这里决定它应走攻击、闪避、防御、切武还是处决分支；
	 * 3. 如果当前不能立刻执行，再决定是否转成输入缓冲等待后续窗口消费。
	 */

	/** 处理一次 Pressed 阶段的完整分发逻辑。*/
	bool HandleAllPressedLogic(
		UActionAbilitySystemComponent* InActionASC,
		FGameplayTag InputTag);

	/** 处理一次 Held 阶段的完整分发逻辑。*/
	bool HandleAllHeldLogic(
		UActionAbilitySystemComponent* InActionASC,
		FGameplayTag InputTag);

	/** 处理一次 Released 阶段的完整分发逻辑。*/
	bool HandleAllReleasedLogic(
		UActionAbilitySystemComponent* InActionASC,
		FGameplayTag InputTag);

	/**
	 * 处理输入状态机中的 Pressed 记录。
	 * Pressed 代表“刚按下”的那个时刻，通常用于进入动作、开窗口或记录首帧请求。
	 */
	void HandlePressedInputState(FGameplayTag InputTag);

	/**
	 * 处理输入状态机中的 Held 记录。
	 * Held 代表“玩家仍在持续按住”，常用于长按重击、持续防御或切武后输入重放这类语义。
	 */
	void HandleHeldInputState(FGameplayTag InputTag);

	/**
	 * 处理输入状态机中的 Released 记录。
	 * Released 主要服务于需要在松开时收尾或结算的输入语义，
	 * 例如长按释放判定、某些防御结束链或能力侧的输入结束通知。
	 */
	void HandleReleasedInputState(FGameplayTag InputTag);

	/**
	 * 统一处理一次战斗输入事件。
	 * 这是战斗输入在组件层的总入口：
	 * 1. 先检查基础前置与当前是否允许处理输入；
	 * 2. 再把输入按事件类型路由到攻击、闪避、防御、处决、切武等分支；
	 * 3. 如果当前不适合立刻执行，再决定是否写入输入缓冲。
	 * 因此它既是“立即执行”入口，也是“决定是否延迟执行”的统一闸门。
	 */
	bool ProcessAbilityInput(
		FGameplayTag InputTag,
		EActionInputEvent InputEvent,
		bool bCanBuffer,
		FGameplayTag ResolvedAttackRequestTag = FGameplayTag());

	/**
	 * 兜底修复被打断后残留的公共战斗运行时状态。
	 * 如果权威 Ability / 受击 / 切武锁都已结束，但总控仍残留旧窗口、旧姿态或旧输入门禁，
	 * 就在正式处理下一次输入前先把这些残留状态收干净，避免出现“所有 GA 都无法响应”的假死。
	 */
	void RepairStaleCombatRuntimeIfNeeded();

	/** 判断当前是否仍存在会主导公共战斗运行时状态的激活态 Ability。 */
	bool HasAnyAuthoritativeActiveCombatAbility() const;

	/** 判断当前是否仍存在会主导公共战斗运行时状态的战斗姿态过渡壳。 */
	bool HasActiveCombatModeTransitionRuntime() const;

	/**
	 * 当前是否允许处理战斗输入。
	 * 这里只回答“战斗组件这一层是否应接管这次输入”，
	 * 例如启动链锁定、角色失效或关键组件缺失都应在这里尽早截断。
	 */
	bool CanProcessCombatInput() const;

public:
	/**
	 * 输出当前非攻击输入门禁的只读调试摘要。
	 * 这层只解释“当前是哪一层在允许 / 阻止这次输入”，
	 * 不改任何运行时状态，也不替代正式门禁判断。
	 */
	FString DescribeNonAttackInputGateForDebug(const FGameplayTag& InputTag) const;

protected:
	/** 读取拥有者身上的受击组件。*/
	UActionCombatReactComponent* GetOwningCombatReactComponent() const;

	/** 当前是否处于会阻断主动战斗输入的主受击阶段。*/
	bool IsInBlockingCombatReactPrimaryPhase() const;

	/** 当前恢复阶段是否仍在阻断这次输入。*/
	bool IsInBlockingCombatReactRecoveryPhase(FGameplayTag InputTag) const;

	/** 当前是否因受击阶段而禁止攻击输入。*/
	bool IsAttackInputBlockedByCombatReact(FGameplayTag InputTag) const;

	/** 当前是否因受击阶段而禁止非攻击输入。*/
	bool IsNonAttackInputBlockedByCombatReact(FGameplayTag InputTag) const;

	/** 当前恢复阶段是否已通过取消窗口正式放行这次输入。 */
	bool IsCombatReactRecoveryCancelInputAllowed(FGameplayTag InputTag) const;

	/** 当前是否因普通空中状态而禁止非攻击输入。*/
	bool IsNonAttackInputBlockedByAirborneState() const;

	/** 当前角色是否处于普通 Falling 状态。*/
	bool IsInNormalFallingState() const;

	/** 当前输入是否应该写入输入缓冲。*/
	bool ShouldBufferInput(FGameplayTag InputTag) const;

	/** 当前输入在本次条件下是否应该入队为缓冲输入。*/
	bool ShouldQueueBufferedInput(FGameplayTag InputTag, bool bCanBuffer) const;

	/** 判断某个输入是否属于攻击输入。*/
	bool IsAttackInputTag(FGameplayTag InputTag) const;

	/**
	 * 判断当前是否处于“只有取消窗口明确放行时，后续动作才允许插入”的阶段。
	 * 主要覆盖攻击演出、处决演出以及战斗锁状态下的输入打断链。
	 */
	bool IsAbilityCancelContextActive() const;

	/** 当前切武表现期是否已经正式进入“只认链窗白名单接攻击”的链上下文。 */
	bool IsWeaponSwitchPresentationChainContextActive() const;

	/** 当前切武表现期是否已经正式进入“只认取消窗口白名单”的取消上下文。 */
	bool IsWeaponSwitchPresentationCancelContextActive() const;

	/**
	 * 判断取消窗口当前是否允许这个输入立即生效。
	 * 输入必须有效、取消窗口必须处于开启状态、并且输入 Tag 在本次白名单内，三者缺一不可。
	 */
	bool IsAbilityCancelInputAllowedNow(FGameplayTag InputTag) const;

	/** 当前切武表现期是否已通过现有 AbilityChainWindow 正式放行这次攻击输入。 */
	bool IsWeaponSwitchPresentationChainInputAllowed(FGameplayTag InputTag) const;

	/** 当前切武表现期是否已通过现有 AbilityCancelWindow 正式放行这次输入。 */
	bool IsWeaponSwitchPresentationCancelInputAllowed(FGameplayTag InputTag) const;

	/**
	 * 判断当前条件下这个输入是否允许被写入输入缓冲。
	 * 这个入口只负责“是否值得先记下来等待后续消费”，
	 * 不代表该输入现在就一定能激活；真正消费时仍需再次通过上下文校验。
	 */
	bool CanBufferCombatInputNow(FGameplayTag InputTag) const;

	/**
	 * 判断这条缓冲输入在当前时刻是否允许被真正消费。
	 * 这里会综合检查按钮实时状态、受击阻断、空中限制、切武状态、当前武器定义、
	 * 攻击衔接窗口与取消窗口等条件，用于避免过期预输入或错误上下文中的输入被误执行。
	 */
	bool CanConsumeBufferedInputNow(const FActionBufferedInput& BufferedInput) const;

protected:
	/**
	 * 输入路由与武器槽映射辅助。
	 * 这层专门负责把“通用输入语义 / 固定槽输入语义 / 当前已装备槽位”三者连接起来，
	 * 避免具体玩法分支到处重复写槽位判断。
	 */

	/** 由输入 Tag 解析出目标固定武器槽。*/
	bool TryResolveWeaponLoadoutSlotFromInputTag(FGameplayTag InputTag, EHeroWeaponLoadoutSlot& OutLoadoutSlot) const;

	/** 由固定武器槽反向得到它的输入 Tag。*/
	FGameplayTag GetInputTagForWeaponLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 判断一个输入 Tag 是否属于切武输入。*/
	bool IsWeaponSwitchInputTag(FGameplayTag InputTag) const;

	/** 读取当前战斗中的有效固定武器槽。*/
	EHeroWeaponLoadoutSlot GetCurrentCombatLoadoutSlot() const;

	/** 把通用输入 Tag 改写成当前固定武器槽作用域下的输入 Tag。*/
	FGameplayTag ResolveLoadoutScopedCombatInputTag(FGameplayTag InputTag) const;

	/** 具体战斗动作分发。*/

	/**
	 * 判断当前是否因战斗状态而阻止切武。
	 * 这里收口的是“即使玩家按下了切武输入，也不应该进入切武事务”的硬性前置条件，
	 * 例如受击主阶段、处决演出、攻击不可取消阶段，或其它明确禁止切武的战斗语境。
	 */
	bool IsWeaponSwitchBlockedByCombatState(FGameplayTag InputTag) const;

	/**
	 * 判断当前是否因切武冷却而阻止切武。
	 * 这个入口只关注冷却层，不混入战斗状态、输入窗口或目标槽位合法性判断，
	 * 这样切武失败时可以明确区分是“状态不允许”还是“冷却未结束”。
	 */
	bool IsWeaponSwitchBlockedByCooldown() const;

	/**
	 * 判断当前这次输入是否允许正式激活切武能力。
	 * 它会综合切武冷却、战斗状态、当前输入槽位、当前装备情况与事务状态做最终放行，
	 * 是切武链进入 Ability 层前的统一闸门。
	 */
	bool CanActivateWeaponSwitchAbility(FGameplayTag InputTag) const;

	/**
	 * 在需要时专门取消当前已激活的攻击能力。
	 * 这个细分入口用于处理“新动作只需要中断攻击，不一定要中断其它能力”的情况，
	 * 让取消范围尽量精确，避免把不相关的状态链一并打断。
	 */
	void CancelActiveAttackAbilityIfNeeded(UActionAbilitySystemComponent* InActionASC) const;

	/**
	 * 处理一次处决输入。
	 * 这一步不会直接假设处决一定成功，
	 * 而是负责把输入层与目标资格查询、窗口预占、Ability 提交链串起来，
	 * 让真正的处决演出只在前置条件完整成立时才启动。
	 */
	bool HandleExecutionLogic(UActionAbilitySystemComponent* InActionASC, FGameplayTag InputTag);

	/**
	 * 尝试把本次切武输入正式提交给切武 GA。
	 * 进入这里时，组件层已经完成了大部分前置资格判断；
	 * 这个入口更像是“最后一步提交”，负责把合格输入真正送入 AbilitySystem。
	 */
	bool TryCommitWeaponSwitchAbilityInput(UActionAbilitySystemComponent* InActionASC, FGameplayTag InputTag);

	/**
	 * 尝试把当前处决输入正式提交给处决 GA。
	 * 在调用前，目标筛选与资格判断通常已经完成；
	 * 这里的重点是把已经确认可执行的结果稳定地交给 AbilitySystem，而不是重新做一遍搜索。
	 */
	bool TryCommitExecutionAbilityInput(UActionAbilitySystemComponent* InActionASC);

	/**
	 * 处理一次固定武器槽切换输入。
	 * 它负责把“玩家按了某个槽位键”转换成明确的目标固定槽位请求，
	 * 并继续交由切武资格判断、事务创建与 Ability 提交链处理。
	 */
	bool HandleWeaponLoadoutSlotInput(UActionAbilitySystemComponent* InActionASC, FGameplayTag InputTag, EHeroWeaponLoadoutSlot InLoadoutSlot);

	/**
	 * 按输入事件类型分发一次战斗动作输入。
	 * 这是这组入口里最外层的动作路由器：
	 * 1. 先根据 InputEvent 区分 Pressed / Held / Released；
	 * 2. 再根据输入 Tag 把请求送到攻击、闪避、防御、处决、切武等对应分支；
	 * 3. 保证每类动作都在统一的分发框架下运行，而不是各自散落在输入回调里。
	 */
	bool HandleCombatActionInputByEvent(
		UActionAbilitySystemComponent* InActionASC,
		FGameplayTag InputTag,
		EActionInputEvent InputEvent,
		FGameplayTag ResolvedAttackRequestTag = FGameplayTag());

protected:
	/** 计时器、战斗窗口与通用事件辅助。*/

	/**
	 * 清空输入缓冲、关闭窗口并回零延迟恢复标记。
	 * 这里只收输入恢复链自己的短生命周期状态，不负责攻击演出、Spirit 索引或受击收尾。
	 */
	void ResetCombatInputRecoveryRuntime();

	/**
	 * 重置攻击 / 防御 / 处决等子链的短生命周期运行时状态。
	 * 这里只要求各子组件回到干净运行态，不替它们决定武器缓存、输入窗口或全局 UI 镜像。
	 */
	void ResetOwnedCombatSubcomponentRuntimeStates();

	/**
	 * 清掉攻击演出、命中配置与 Attack 连段状态。
	 * 这里只处理攻击表现层与命中层残留，不顺手重置防御、受击或输入恢复链。
	 */
	void ResetAttackPresentationRuntime(bool bRestoreAttackEnabled);

	/**
	 * 清理一个战斗链内部使用的计时器。
	 * 统一走这个入口，是为了避免不同窗口、不同恢复链各自重复写清理逻辑，
	 * 也方便保证“窗口提前关闭时，对应计时器一定同步失效”，防止过期回调晚到。
	 */
	void ClearCombatTimer(FTimerHandle& TimerHandle);

	/**
	 * 启动一个战斗链内部使用的计时器，并在到期时回调指定成员函数。
	 * 这里主要服务于各种短时窗口，例如精确格挡、完美闪避、闪反资格等，
	 * 让这些窗口的开启、到期关闭与事件广播都走统一的生命周期管理。
	 */
	void StartCombatTimer(FTimerHandle& TimerHandle, void (UHeroCombatComponent::*ExpireCallback)(), float Duration);

	/**
	 * 只有在条件满足时才广播一条战斗事件。
	 * 这样可以把“窗口是否真的从关闭变为打开”或“状态是否真的从有效变为失效”的判断
	 * 留在调用点，然后由这里统一负责对 ASC 与外部系统的事件分发。
	 */
	void BroadcastCombatEventIf(bool bShouldBroadcast, FGameplayTag EventTag) const;

	/** Spirit 多段链等待超时后，正式提交流却并回收持久索引状态。 */
	void HandleSpiritSkillComboTimeoutByInputTag(FGameplayTag SpiritInputTag);

	/**
	 * 打开一个会自动计时关闭的战斗窗口。
	 * 这个辅助入口会把三件事收在一起：
	 * 1. 启动计时器；
	 * 2. 保持窗口存活到指定时长；
	 * 3. 在窗口成功开启时广播对应事件。
	 * 这样可以避免每种窗口都手写一套近似的“开窗 + 启计时 + 发事件”模板代码。
	 */
	void OpenTimedCombatWindow(
		FTimerHandle& TimerHandle,
		void (UHeroCombatComponent::*ExpireCallback)(),
		float Duration,
		FGameplayTag OpenEventTag);

	/**
	 * 关闭一个带计时器的战斗窗口。
	 * bWasActive 用于表明关闭前窗口是否真的处于激活态，
	 * 只有之前确实开着时，才有必要发“关闭”事件，避免重复关闭造成外部状态抖动。
	 */
	void CloseTimedCombatWindow(FTimerHandle& TimerHandle, bool bWasActive, FGameplayTag CloseEventTag);

	/**
	 * 在组件侧应用一份临时战斗修正效果。
	 * 这类效果一般用于窗口性、状态性或短时战斗修正，
	 * 例如临时抗性、短时资源修正或某些受击/处决期间的保护效果。
	 * 统一从这里应用，可以把句柄管理与重复覆盖策略收口到一处。
	 */
	bool ApplyComponentCombatModifierEffect(
		FActiveGameplayEffectHandle& InOutEffectHandle,
		const FActionCombatModifierEffectSpec& EffectTemplate,
		float OverrideDuration = -1.f);

	/**
	 * 移除一份此前由组件侧持有的临时战斗修正效果。
	 * 该入口与 ApplyComponentCombatModifierEffect 配套使用，
	 * 负责在窗口结束、状态解除或链路异常终止时做统一收尾。
	 */
	void RemoveComponentCombatModifierEffect(FActiveGameplayEffectHandle& InOutEffectHandle);

protected:
	/** 受击反应与战斗事件落地。*/

	/**
	 * 尝试处理一条已经被解析完成的战斗反应事件。
	 * 到这里时，外层通常已经知道本次结果是普通受击、崩防、削韧崩解、击飞或击倒中的哪一种；
	 * 这个入口负责把该结果真正落到组件运行时状态与后续演出链上。
	 */
	bool TryHandleResolvedCombatReactEvent(FGameplayTag InCombatEventTag, AActor* InstigatorActor);

	/** 把战斗反应事件继续转发给 ASC 与外部系统。*/

	/**
	 * 清空当前战斗反应相关的运行时状态。
	 * 受击链经常会临时接管输入、动作状态、窗口与保护效果，
	 * 因此在一条反应链结束或被中断后，必须统一从这里做状态回收，避免旧受击状态残留。
	 */
	void ResetCombatReactionState();

	/**
	 * 落地一次普通受击事件。
	 * 这里处理的是没有进一步升级为崩防、击飞、击倒等更强控制结果时的基础受击链，
	 * 一般会衔接受击蒙太奇、输入暂时阻断与受击恢复流程。
	 */
	void HandleHitReactEvent(AActor* InstigatorActor);

	/**
	 * 落地一次防御崩解事件。
	 * 当角色虽然处于防御链中，但本次冲击超出了防御可承受范围时，
	 * 会从这里进入崩防状态，并触发比普通防御失败更重的硬直与窗口后果。
	 */
	void HandleGuardBreakEvent(AActor* InstigatorActor);

	/**
	 * 落地一次削韧崩解事件。
	 * 这条链通常表示目标韧性被彻底打空，
	 * 后续可能衔接失衡窗口、处决窗口开启或其它更高阶的战斗收益链。
	 */
	void HandlePoiseBreakEvent(AActor* InstigatorActor);

	/**
	 * 落地一次击飞事件。
	 * 击飞通常会把角色带入更强的受控空中状态，
	 * 因此它不只是播放一个受击动作，还会影响空中失控、落地恢复与输入恢复时机。
	 */
	void HandleLaunchEvent(AActor* InstigatorActor);

	/**
	 * 落地一次击倒事件。
	 * 这条链一般覆盖倒地、躺地、起身保护等更完整的受击阶段，
	 * 是普通受击与击飞之上的另一类强控制结果。
	 */
	void HandleKnockdownEvent(AActor* InstigatorActor);

protected:
	/** 切武事务与表现期状态机。*/

	/**
	 * 在正式切武前，重置那些会干扰切武链的战斗状态。
	 * 例如旧攻击、旧防御、旧受击接管或其它互斥中的演出状态；
	 * 这样可以保证切武事务总是在一套可控、可预测的战斗上下文里开始。
	 */
	void ResetCombatStateForWeaponSwitch();
	/** 组件侧唯一正式的 Combat -> Idle 退出判断。 */
	void UpdateCombatModeIdleExitTransition(float DeltaTime);
	/** 进入退出 Combat 表现过渡；若无退出蒙太奇则直接写回 Idle。 */
	bool TryBeginExitCombatModeTransition();
	bool TryPlayCombatModeTransitionMontage(UAnimMontage* TransitionMontage, bool bToIdle);
	void FinishCombatModeTransitionMontage(UAnimMontage* TransitionMontage, bool bInterrupted);
	void ApplyCombatModeTransitionMoveLock(bool bLocked);
	void ClearCombatModeTransitionRuntime();
	void StopActiveCombatModeTransitionMontage(bool bRestoreSocketForCurrentCombatMode);
	void RefreshCurrentEquippedWeaponSocketPresentation() const;
	void RequestCurrentEquippedWeaponPresentationToCombatSocket();
	void RequestCurrentEquippedWeaponPresentationToHolster();
	void UpdateWeaponSwitchCooldownUIState();
	void BroadcastCombatUIStateChanged() const;

protected:
	/** 当前武器上下文缓存同步辅助。*/

	/**
	 * 把最新装备状态应用到战斗组件缓存。
	 * 这一步是战斗组件内部真正切换“当前武器上下文”的落地点，
	 * 后续攻击配置查询、闪避/防御动画查询、处决蒙太奇查询等都会依赖这里同步好的缓存结果。
	 */
	void ApplyEquippedWeaponState(const FHeroEquippedWeaponState& InWeaponState);

	/**
	 * 判断一份装备状态是否足以回填当前武器缓存。
	 * 这里的“有效”不等价于“必须持有武器实例”，
	 * 只要它已经能明确表达当前武器上下文，例如空手槽、有效武器定义或非空武器标签，就允许写回缓存。
	 */
	bool IsEquippedWeaponStateMeaningfulForCombatCache(const FHeroEquippedWeaponState& InWeaponState) const;

	/** 判断当前缓存是否已经表达与目标快照相同的武器上下文，避免只读兜底重复刷同步日志。 */
	bool IsCombatCacheAlreadySynchronizedToEquippedState(const FHeroEquippedWeaponState& InWeaponState) const;

	/**
	 * 必要时从装备组件补做一次当前武器同步。
	 * 某些查询入口可能在组件初始化早期、事件尚未回推完成时就被调用；
	 * 这里用于兜底保证战斗组件的当前武器缓存，不会长时间落后于装备组件真实状态。
	 */
	void EnsureCurrentWeaponStateSynchronizedFromEquipment() const;

	/**
	 * 按当前武器定义刷新当前可用的连段上限。
	 * 不同武器、不同武器槽可能拥有不同的攻击分支与连段数量，
	 * 因此在武器切换后，需要及时把战斗组件内部引用的连段上限同步到最新状态。
	 */
	void RefreshCurrentWeaponComboLimit();

	/**
	 * 把一笔特殊切武能量增量转发给装备组件。
	 * 战斗组件负责判定何时获得奖励，例如完美格挡、完美闪避或后续其它战斗收益；
	 * 真正的能量持有与累加规则则由装备组件统一维护。
	 */
	void AddSpecialWeaponSwitchEnergy(float DeltaEnergy) const;
	UAnimInstance* GetOwnerAnimInstance() const;
	void HandleCombatModeTransitionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

protected:
	/** 输入与窗口配置。*/

	/** 战斗输入配置资产。*/
	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|InputData")
	UDataAsset_InputConfig* CombatInputConfig = nullptr;

	/** 战斗输入映射表。*/
	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|InputData")
	UInputMappingContext* CombatMappingContext = nullptr;

	/** 轻按与长按的分界阈值。*/
	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig")
	float ShortPressThreshold = 0.4f;

	/** 输入缓冲有效时长。*/
	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig")
	float InputBufferDuration = 0.25f;

	/** Combo 状态静止多久后，正式尝试退出 Combat 表现态。*/
	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig")
	float CombatModeIdleExitDelaySeconds = 3.f;

protected:
	/** 运行时战斗状态。*/

	/** 精确格挡、完美闪避、闪反等战斗窗口运行时状态。*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FHeroCombatWindowRuntimeState WindowRuntimeState;

	/** 攻击衔接 / 通用取消窗口运行时状态。*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FHeroAbilityWindowRuntimeState AbilityWindowRuntimeState;

	/** 近战命中、发射物模板和 Spirit Offensive 共用的正式伤害上下文。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FActionDamageContextRuntimeState DamageContextRuntimeState;

	/**
	 * Spirit 多段技能的正式持久状态源。
	 * Key 固定是 SpiritSkill1~4 这类真实输入标签，Value 记录该输入当前待命段、是否仍在等待续段、
	 * 这条链是否已经扣过一次成本以及对应的超时计时器。
	 * 这样同一个 Spirit 输入跨多次 GA 激活仍能回到同一份权威状态，而不会把持久语义塞回 GA 局部字段。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	TMap<FGameplayTag, FHeroSpiritSkillComboRuntimeState> SpiritSkillComboRuntimeStates;

	/**
	 * 当前已装备武器定义的本地缓存。
	 * 绝大多数攻击配置、闪避/防御动画、处决蒙太奇与战斗参数查询都以它为第一入口，
	 * 这样可以避免每次都反复向装备组件追问当前武器定义。
	 */
	UPROPERTY(Transient)
	TObjectPtr<UDataAsset_WeaponDefinition> CurrentEquippedWeaponDefinition = nullptr;

	/**
	 * 当前已装备武器实例的本地缓存。
	 * 当战斗链需要访问实际武器 Actor，例如命中检测、实例侧状态或表现层数据时，
	 * 会优先从这里读取当前正在生效的实例引用。
	 */
	UPROPERTY(Transient)
	TObjectPtr<AHeroWeaponBase> CurrentEquippedWeaponInstance = nullptr;

	/**
	 * 受击链接管前，是否正在强制回收防御 / 闪避链留下的运行时状态。
	 * 这个标记主要用于阻止旧 Ability 在“受击重置正在发生”的过程中再次回写窗口、恢复输入或广播旧事件，
	 * 以免主动战斗链和受击链在交接帧互相覆盖状态。
	 */
	UPROPERTY(Transient)
	bool bCombatReactStateResetInProgress = false;

private:
	/**
	 * 依赖对象缓存。
	 * 这些引用本质上都能从 Owner 或组件层级重新拿到，
	 * 但做成本地弱引用缓存后，可以减少高频查询时的重复查找开销，同时避免形成强引用环。
	 */

	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	mutable TWeakObjectPtr<AActionPlayerController> CachedHeroController;
	mutable TWeakObjectPtr<UActionAbilitySystemComponent> CachedActionAbilitySystemComponent;
	mutable TWeakObjectPtr<UActionAttributeSetBase> CachedActionAttributeSet;
	mutable TWeakObjectPtr<UHeroEquipmentComponent> CachedHeroEquipmentComponent;
	mutable TWeakObjectPtr<UHeroLoadoutStateComponent> CachedHeroLoadoutStateComponent;
	mutable TWeakObjectPtr<UHeroWeaponSwitchComponent> CachedHeroWeaponSwitchComponent;
	mutable TWeakObjectPtr<UHeroAttackComponent> CachedHeroAttackComponent;
	mutable TWeakObjectPtr<UHeroDefenseComponent> CachedHeroDefenseComponent;
	mutable TWeakObjectPtr<UHeroCombatInputComponent> CachedHeroCombatInputComponent;
	mutable TWeakObjectPtr<UHeroExecutionCoordinatorComponent> CachedHeroExecutionCoordinatorComponent;
	mutable TWeakObjectPtr<UHeroHitSourceComponent> CachedHeroHitSourceComponent;
	mutable TWeakObjectPtr<UHeroTargetingComponent> CachedHeroTargetingComponent;

	/** 当前切武冷却阻挡态的 UI 镜像缓存。 */
	bool bLastWeaponSwitchCooldownBlocked = false;

	/** 当前正式武器层开关镜像。 */
	bool bHasEquippedWeaponLinkedLayerPresentation = false;

	/** 当前正式武器层类镜像。 */
	UPROPERTY(Transient)
	TSubclassOf<UActionHeroLinkedAnimLayer> CurrentWeaponLinkedAnimLayerClass;

	/** 当前已装备武器是否应挂在手持 WeaponSocket。 */
	bool bCurrentEquippedWeaponShouldUseWeaponSocketPresentation = false;

	/** 当前武器同步日志最近一次已打印的正式装备快照。它只服务日志去重，不参与玩法状态。 */
	FHeroEquippedWeaponState LastLoggedEquippedWeaponState;

	/** 当前武器同步日志是否已经落过第一笔正式快照。 */
	bool bHasLoggedEquippedWeaponState = false;

	/** 当前武器同步日志最近一次已打印的武器层开关镜像。 */
	bool bLastLoggedWeaponLinkedLayerPresentation = false;

	/** 当前武器同步日志最近一次已打印的武器层类镜像。 */
	TSubclassOf<UActionHeroLinkedAnimLayer> LastLoggedWeaponLinkedAnimLayerClass;

	/** 当前 Combo 静止累计时长。 */
	float CurrentCombatIdleElapsedSeconds = 0.f;

	/** 当前是否正在播放 Combat 过渡蒙太奇。 */
	bool bCombatModeTransitionActive = false;

	/** 当前过渡蒙太奇的目标是否为退出到 Idle。 */
	bool bCombatModeTransitionTargetIdle = false;

	/** 当前过渡蒙太奇是否已经收到专用切挂点 Notify。 */
	bool bCombatModeTransitionNotifyReceived = false;

	/** 当前 Combat 表现过渡是否已经正式锁定移动输入。 */
	bool bCombatModeTransitionMoveInputLocked = false;

	/** 当前活跃的 Combat 过渡蒙太奇。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveCombatModeTransitionMontage = nullptr;

	/** 战斗侧 UI 状态变化委托。 */
	FOnHeroCombatUIStateChanged CombatUIStateChangedDelegate;

private:
	bool ResolveWeaponLinkedLayerPresentationState(
		const UDataAsset_WeaponDefinition* InWeaponDefinition,
		TSubclassOf<UActionHeroLinkedAnimLayer>* OutLinkedAnimLayerClass = nullptr) const;

	FHeroSpiritSkillComboRuntimeState* FindMutableSpiritSkillComboRuntimeState(const FGameplayTag& SpiritInputTag);
	const FHeroSpiritSkillComboRuntimeState* FindSpiritSkillComboRuntimeState(const FGameplayTag& SpiritInputTag) const;
	void ClearSpiritSkillComboTimer(FHeroSpiritSkillComboRuntimeState& RuntimeState);
	void StartSpiritSkillComboTimer(const FGameplayTag& SpiritInputTag, FHeroSpiritSkillComboRuntimeState& RuntimeState);
	void ClearBufferedSpiritSkillInputByTag(const FGameplayTag& SpiritInputTag);

	UHeroLoadoutStateComponent* GetOwningHeroLoadoutStateComponent() const;
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;
	UHeroTargetingComponent* GetOwningHeroTargetingComponent() const;

};
