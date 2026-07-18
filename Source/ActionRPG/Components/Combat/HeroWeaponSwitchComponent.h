#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "HeroWeaponSwitchComponent.generated.h"

class AActionHeroCharacter;
class UAnimMontage;
class UActionAbilitySystemComponent;
class UHeroCombatComponent;
class UHeroCombatInputComponent;
class UHeroEquipmentComponent;

/**
 * 英雄切武组件。
 * 当前只维护固定槽输入解析、最小切武请求壳、同步切武事务、特殊切武表现期状态，
 * 以及普通切武完成后的最小下一帧轻攻击首段 handoff。
 * 普通切武不再保留独立表现壳或表现期后专用重试链。
 * 当前装备正式结果仍只认装备组件广播出的 EquippedWeaponState。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroWeaponSwitchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroWeaponSwitchComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 外部切武入口：
	// 角色输入、GA 与上层战斗总控都应优先通过这一层发起固定武器槽切换。
	/** 请求装备指定固定武器槽。*/
	bool EquipWeaponByLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 由固定武器槽反向得到它的输入 Tag。它只服务输入解析，不形成第二份槽位状态源。 */
	FGameplayTag GetInputTagForWeaponLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	// 切武输入门禁：
	// 这里负责判断某个输入是不是切武输入，以及当前是否因战斗态或冷却而禁止切武。
	// 这层只回答“当前能不能推进切武请求”，不等于事务或表现期已经开始。
	/** 判断一个输入 Tag 是否属于切武输入。*/
	bool IsWeaponSwitchInputTag(FGameplayTag InputTag) const;

	/** 判断当前是否因战斗状态阻止切武。*/
	bool IsWeaponSwitchBlockedByCombatState(FGameplayTag InputTag) const;

	/** 判断当前是否因切武冷却阻止切武。*/
	bool IsWeaponSwitchBlockedByCooldown() const;

	/** 判断当前这次输入是否允许正式进入切武 Ability 链。它只看共享硬门禁与冷却，不替代 ASC 关系裁决。 */
	bool CanActivateWeaponSwitchAbility(FGameplayTag InputTag) const;

	/** 提交一条切武 GA 输入，请求正式进入切武能力链。 */
	bool TryCommitWeaponSwitchAbilityInput(UActionAbilitySystemComponent* InActionASC, FGameplayTag InputTag);

	// 固定槽输入到切武请求：
	// 这一层负责把武器槽输入翻译成真正的切武请求或切武能力激活。
	/** 处理固定武器槽输入。*/
	bool HandleWeaponLoadoutSlotInput(
		UActionAbilitySystemComponent* InActionASC,
		FGameplayTag InputTag,
		EHeroWeaponLoadoutSlot InLoadoutSlot);

	// 当前状态同步与切武目标解析：
	// 负责把装备组件当前结果同步到本地快照，并解析当前排队请求最终想切到哪个槽。
	// 当前正式已装备结果和槽位资源状态最终仍以 HeroEquipmentComponent 为准。
	/** 初始化当前武器快照。*/
	void InitializeCurrentWeaponStateFromEquipment();

	/** 解析当前排队的切武请求。它只从 WeaponSwitchRequest 读目标，不平行维护第二份目标槽缓存。 */
	bool ResolveWeaponSwitchTargetLoadoutSlot(EHeroWeaponLoadoutSlot& OutLoadoutSlot, bool bConsumeQueuedRequest);

	// 特殊切武表现查询：
	// 这里只判断目标槽位是否满足特殊切武条件，以及该槽位应该播哪条特殊切武蒙太奇。
	// 它们只读当前配置 / 表现资格，不推进真实切武事务。
	/** 判断目标槽位当前是否满足特殊切武表现条件。这里只决定是否能进特殊演出，不推进真实切武事务。 */
	bool CanUseSpecialWeaponSwitchForLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 读取目标槽位配置的特殊切武蒙太奇。 */
	UAnimMontage* GetSpecialWeaponSwitchMontageForLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 判断指定目标槽位当前是否满足同步切武所需的 ready 条件。 */
	bool CanSynchronouslyEquipLoadoutSlot(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FString* OutFailureReason = nullptr) const;

	/** 当前是否处于特殊切武表现期。*/
	bool IsSpecialWeaponSwitchPresentationActive() const { return SpecialWeaponSwitchPresentationState.bPresentationActive; }

	/** 当前是否存在进行中的切武事务。*/
	bool IsWeaponSwitchTransactionInProgress() const { return WeaponSwitchTransactionState.bSwitchInProgress; }

    // 运行时状态清理：
    // 这些接口主要用于表现期结束、启动链重置和缓冲过期后的统一回零。
    // 清掉请求或延迟标记，不等于回滚已经完成的真实装备切换。
    /** 清空当前排队的切武请求。*/
    void ClearQueuedWeaponLoadoutSlotSwitch() { WeaponSwitchRequest.Clear(); }

    /** 清空切武请求壳。 */
    void ClearDeferredQueuedWeaponSwitchConsumeRequest() { WeaponSwitchRequest.Clear(); }

	/** 开始特殊切武表现期。它只打开 Presentation 运行态，不代表切武事务尚未完成。 */
	void BeginSpecialWeaponSwitchPresentation();

	/** 结束特殊切武表现期。它只负责演出门禁收尾与后续延迟消费/输入恢复调度。 */
	void EndSpecialWeaponSwitchPresentation();

	/** 普通切武真实换装完成后，登记一笔“下一帧起轻攻击首段”的最小 handoff 请求。 */
	void ArmPendingNormalWeaponSwitchAttackHandoff(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 启动链重置时回零切武运行时状态。*/
	void ResetRuntimeStateForHeroStartup();

	/** 处理一条已过期的切武缓冲输入。*/
	void HandleBufferedInputExpired();

private:
    // 切武请求壳：
    // 这里只保留“当前这次 WeaponSwitch GA 想切去哪一槽”这一份最小目标信息，
    // 不再继续承接普通切武表现期后的专用重试或普通切武 deferred 行为。
    /** 记录一条新的切武请求。*/
    bool QueueWeaponLoadoutSlotSwitch(EHeroWeaponLoadoutSlot InLoadoutSlot);

	// 装备组件事件回调：
    // 切武组件通过这些回调感知“装备是否真的切成功了”，并据此收尾事务与表现期。
	/** 装备组件通知：当前装备状态已变化。*/
	void HandleEquippedWeaponStateChanged(const FHeroEquippedWeaponState& InEquippedWeaponState);

    /** 装备组件通知：目标槽位装备失败。*/
    void HandleWeaponLoadoutEquipFailed(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 特殊切武表现结束后的下一帧恢复输入。 */
	void HandleDeferredRecoverCombatInputAfterPresentation();

	/** 普通切武 Ability 退场后的下一帧，尝试正式 handoff 到轻攻击首段。 */
	void HandleDeferredNormalWeaponSwitchAttackHandoff();

	/** 清掉当前挂着的普通切武后轻攻击首段 handoff 请求。 */
	void ClearPendingNormalWeaponSwitchAttackHandoff();

    // 依赖组件绑定与访问：
    // 统一管理与装备组件的事件绑定，以及对角色 / 战斗组件 / 装备组件 / ASC 的读取入口。
	/** 绑定装备组件事件。*/
	void BindEquipmentStateDelegates();

	/** 解绑装备组件事件。*/
	void UnbindEquipmentStateDelegates();

	/** 获取拥有者角色。它是本组件向上解析宿主的稳定入口，不会在这里创建新的角色级状态。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;

	/** 获取拥有者战斗组件。切武组件只维护请求 / 事务 / 表现期，高层门禁和输入恢复仍回到总控。 */
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;

	/** 获取拥有者输入运行态组件。它只提供正式输入状态与缓冲读取，不替代切武事务状态源。 */
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;

	/** 获取拥有者装备组件。当前正式已装备结果和槽位资源状态最终都以它为准。 */
	UHeroEquipmentComponent* GetOwningHeroEquipmentComponent() const;

	/** 获取拥有者 ASC。切武 Ability 的正式激活仍经由 GAS。 */
	UActionAbilitySystemComponent* GetOwningActionAbilitySystemComponent() const;

private:
    // 切武运行时最小状态：
    // 1. Request 只保存“本次 WeaponSwitch GA 的目标槽位”；
    // 2. Transaction 只表达“同步真实换装是否还没收口”；
    // 3. Presentation 只继续服务特殊切武演出。
    /** 当前待处理的切武请求。*/
    UPROPERTY()
    FHeroQueuedWeaponSwitchRequest WeaponSwitchRequest;

	/** 当前切武事务状态。*/
	UPROPERTY()
	FHeroWeaponSwitchTransactionState WeaponSwitchTransactionState;

    /** 当前特殊切武表现期状态。*/
    UPROPERTY()
    FHeroSpecialWeaponSwitchPresentationState SpecialWeaponSwitchPresentationState;

	/** 下一帧是否要在特殊切武表现结束后恢复输入。 */
	bool bDeferredCombatInputRecoveryAfterPresentationRequested = false;

	/** 下一帧是否要在普通切武 GA 退场后尝试起轻攻击首段。 */
	bool bDeferredNormalWeaponSwitchAttackHandoffRequested = false;

	/** 当前挂起的普通切武首段 handoff 目标槽位。 */
	EHeroWeaponLoadoutSlot DeferredNormalWeaponSwitchAttackTargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	/** 当前挂起的普通切武首段 handoff 请求序号。它只用于调试和防止旧请求串入新切武。 */
	int32 DeferredNormalWeaponSwitchAttackHandoffToken = 0;

	/** 生成普通切武首段 handoff 请求序号。 */
	int32 NextNormalWeaponSwitchAttackHandoffToken = 0;

    // 外部事件绑定句柄：
	// 保存委托句柄，保证 BeginPlay / EndPlay 期间能安全绑定与解绑。
	/** 装备状态变化委托句柄。*/
	FDelegateHandle EquippedWeaponStateChangedHandle;

	/** 装备失败委托句柄。*/
	FDelegateHandle WeaponLoadoutEquipFailedHandle;
};
