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
 * 只维护固定武器槽切换请求、切武事务与切武表现期状态，
 * 具体战斗判断与输入恢复仍交给 HeroCombatComponent 总控。
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

	/** 由固定武器槽反向得到它的输入 Tag。*/
	FGameplayTag GetInputTagForWeaponLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	// 切武输入门禁：
	// 这里负责判断某个输入是不是切武输入，以及当前是否因战斗态或冷却而禁止切武。
	/** 判断一个输入 Tag 是否属于切武输入。*/
	bool IsWeaponSwitchInputTag(FGameplayTag InputTag) const;

	/** 判断当前是否因战斗状态阻止切武。*/
	bool IsWeaponSwitchBlockedByCombatState(FGameplayTag InputTag) const;

	/** 判断当前是否因切武冷却阻止切武。*/
	bool IsWeaponSwitchBlockedByCooldown() const;

	/** 判断当前这次输入是否允许正式激活切武能力。*/
	bool CanActivateWeaponSwitchAbility(FGameplayTag InputTag) const;

	/** 提交切武 GA 输入。*/
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
	/** 初始化当前武器快照。*/
	void InitializeCurrentWeaponStateFromEquipment();

	/** 解析当前排队的切武请求。它只从 WeaponSwitchRequest 读目标，不平行维护第二份目标槽缓存。 */
	bool ResolveWeaponSwitchTargetLoadoutSlot(EHeroWeaponLoadoutSlot& OutLoadoutSlot, bool bConsumeQueuedRequest);

	// 特殊切武表现查询：
	// 这里只判断目标槽位是否满足特殊切武条件，以及该槽位应该播哪条特殊切武蒙太奇。
	/** 判断目标槽位当前是否满足特殊切武表现条件。这里只决定是否能进特殊演出，不推进真实切武事务。 */
	bool CanUseSpecialWeaponSwitchForLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 读取目标槽位配置的特殊切武蒙太奇。 */
	UAnimMontage* GetSpecialWeaponSwitchMontageForLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 读取目标槽位配置的普通切武蒙太奇。 */
	UAnimMontage* GetNormalWeaponSwitchMontageForLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 当前是否处于切武表现播放期。*/
	bool IsWeaponSwitchPresentationActive() const { return WeaponSwitchPresentationState.bPresentationActive; }

	/** 当前是否处于特殊切武表现期。*/
	bool IsSpecialWeaponSwitchPresentationActive() const
	{
		return WeaponSwitchPresentationState.bPresentationActive && WeaponSwitchPresentationState.bSpecialPresentation;
	}

	/** 当前是否存在进行中的切武事务。*/
	bool IsWeaponSwitchTransactionInProgress() const { return WeaponSwitchTransactionState.bSwitchInProgress; }

	/** 当前排队请求是否需要在表现期结束后消费。*/
	bool ShouldConsumeQueuedWeaponSwitchAfterPresentation() const
	{
		return WeaponSwitchRequest.ShouldConsumeAfterPresentation();
	}

	// 运行时状态清理：
	// 这些接口主要用于表现期结束、启动链重置和缓冲过期后的统一回零。
	/** 清空当前排队的切武请求。*/
	void ClearQueuedWeaponLoadoutSlotSwitch() { WeaponSwitchRequest.Clear(); }

	/** 清空延迟消费标记。*/
	void ClearDeferredQueuedWeaponSwitchConsumeRequest() { bDeferredQueuedWeaponSwitchConsumeRequested = false; }

	/** 开始切武表现期。它只打开 Presentation 运行态，不代表切武事务尚未完成。 */
	void BeginWeaponSwitchPresentation(bool bIsSpecialPresentation);

	/** 结束切武表现期。它只负责演出门禁收尾与后续延迟消费/输入恢复调度。 */
	void EndWeaponSwitchPresentation();

	/** 启动链重置时回零切武运行时状态。*/
	void ResetRuntimeStateForHeroStartup();

	/** 处理一条已过期的切武缓冲输入。*/
	void HandleBufferedInputExpired();

private:
	// 切武请求排队与延迟消费：
	// 负责记录最后一次有效切武请求，并在特殊切武表现期后择机消费。
	/** 记录一条新的切武请求。*/
	bool QueueWeaponLoadoutSlotSwitch(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 处理表现期后的延迟切武消费。优先级高于普通输入恢复。 */
	bool TryConsumeQueuedWeaponSwitchAfterPresentation();

	/** 下一帧再次尝试消费表现期后的切武请求，避免和本帧刚结束的表现期收尾打架。 */
	void RequestConsumeQueuedWeaponSwitchAfterPresentationOnNextTick();

	/** 在切武表现期真正结束后的下一帧恢复战斗输入，避免首帧恢复又被切武活跃标签反挡。 */
	void RequestRecoverCombatInputAfterPresentationOnNextTick();

	// 装备组件事件回调：
	// 切武组件通过这些回调感知“装备是否真的切成功了”，并据此收尾事务与表现期。
	/** 装备组件通知：当前装备状态已变化。*/
	void HandleEquippedWeaponStateChanged(const FHeroEquippedWeaponState& InEquippedWeaponState);

	/** 装备组件通知：目标槽位装备失败。*/
	void HandleWeaponLoadoutEquipFailed(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 下一帧处理表现期后的切武消费。*/
	void HandleDeferredQueuedWeaponSwitchAfterPresentation();

	/** 下一帧处理表现期结束后的输入恢复。*/
	void HandleDeferredRecoverCombatInputAfterPresentation();

	/** 表现期结束且事务已完成后，补跑一次正式切武运行态收尾。*/
	void FinalizeWeaponSwitchRuntimeAfterPresentationIfReady();

	// 依赖组件绑定与访问：
	// 统一管理与装备组件的事件绑定，以及对角色 / 战斗组件 / 装备组件 / ASC 的读取入口。
	/** 绑定装备组件事件。*/
	void BindEquipmentStateDelegates();

	/** 解绑装备组件事件。*/
	void UnbindEquipmentStateDelegates();

	/** 获取拥有者角色。*/
	AActionHeroCharacter* GetOwningHeroCharacter() const;

	/** 获取拥有者战斗组件。*/
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;

	/** 获取拥有者输入运行态组件。*/
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;

	/** 获取拥有者装备组件。*/
	UHeroEquipmentComponent* GetOwningHeroEquipmentComponent() const;

	/** 获取拥有者 ASC。*/
	UActionAbilitySystemComponent* GetOwningActionAbilitySystemComponent() const;

private:
	// 切武运行时三层状态：
	// 1. Request 记录“玩家想切去哪”；
	// 2. Transaction 记录“逻辑切武是否还在处理中”；
	// 3. Presentation 记录“演出是否还在播放”。
	/** 当前待处理的切武请求。*/
	UPROPERTY()
	FHeroQueuedWeaponSwitchRequest WeaponSwitchRequest;

	/** 当前切武事务状态。*/
	UPROPERTY()
	FHeroWeaponSwitchTransactionState WeaponSwitchTransactionState;

	/** 当前切武表现期状态。*/
	UPROPERTY()
	FHeroWeaponSwitchPresentationState WeaponSwitchPresentationState;

	// 下一帧延迟任务标记：
	// 用于避开“本帧表现刚结束、本帧又立即恢复输入或消费请求”带来的时序冲突。
	/** 下一帧是否要再次尝试消费表现期后的切武请求。*/
	bool bDeferredQueuedWeaponSwitchConsumeRequested = false;

	/** 下一帧是否要在切武表现期结束后恢复战斗输入。*/
	bool bDeferredCombatInputRecoveryAfterPresentationRequested = false;

	// 外部事件绑定句柄：
	// 保存委托句柄，保证 BeginPlay / EndPlay 期间能安全绑定与解绑。
	/** 装备状态变化委托句柄。*/
	FDelegateHandle EquippedWeaponStateChangedHandle;

	/** 装备失败委托句柄。*/
	FDelegateHandle WeaponLoadoutEquipFailedHandle;
};
