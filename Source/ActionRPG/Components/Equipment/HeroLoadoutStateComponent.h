// 文件说明：声明英雄装备域启动/预热状态机与 UI 快照桥组件。

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "Components/Equipment/HeroEquipmentRuntimeTypes.h"
#include "HeroLoadoutStateComponent.generated.h"

class AActionHeroCharacter;
class UHeroEquipmentComponent;
class UHeroLoadoutContextComponent;
class UHeroLoadoutRuntimeComponent;

DECLARE_MULTICAST_DELEGATE(FOnHeroWeaponLoadoutStartupReady);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHeroWeaponLoadoutStartupFailed, EHeroWeaponLoadoutSlot, const FString&);
DECLARE_MULTICAST_DELEGATE(FOnHeroLoadoutUIStateChanged);

/**
 * 英雄装备域启动/预热状态机与 UI 快照桥组件。
 * 负责 startup prewarm 正式状态、统一 UI 广播和槽位/根快照构建。
 * 它只桥接 runtime / equipment / context 三个宿主当前结果，不反向成为资源链或正式装备态宿主。
 * 后续若继续优化，只收它对 Equipment 内部运行态的直接读取耦合，不再拆成 startup / UI 两个宿主。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroLoadoutStateComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroLoadoutStateComponent();

public:
	/** 启动装备域 startup prewarm 流程。它只进入启动状态机，不直接持有资源加载或装备写回。 */
	bool InitializeWeaponLoadoutStartup(EHeroWeaponLoadoutSlot InSpawnLoadoutSlot);

	/** 基于既有固定槽配置重启 startup prewarm，用于启动失败后的手动重试。 */
	bool RetryWeaponLoadoutStartup();

	/** 构建单个固定槽位的 UI 只读快照。它只聚合 Equipment / Runtime / Context 当前结果。 */
	bool BuildLoadoutSlotUISnapshot(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FHeroWeaponLoadoutSlotUISnapshot& OutSnapshot) const;

	/** 构建装备域根 UI 快照。它只服务 UI 展示，不反向驱动装备域状态。 */
	void BuildEquipmentLoadoutUISnapshot(FHeroWeaponLoadoutUISnapshot& OutSnapshot) const;

	/** startup ready 广播：只表示启动链已进入可操作状态。 */
	FOnHeroWeaponLoadoutStartupReady& OnWeaponLoadoutStartupReady() { return WeaponLoadoutStartupReadyDelegate; }
	/** startup failed 广播：只同步失败槽位和诊断文本。 */
	FOnHeroWeaponLoadoutStartupFailed& OnWeaponLoadoutStartupFailed() { return WeaponLoadoutStartupFailedDelegate; }
	/** UI 脏标记广播：通知外部重新拉取 UI 快照。 */
	FOnHeroLoadoutUIStateChanged& OnLoadoutUIStateChanged() { return LoadoutUIStateChangedDelegate; }

	/** 只读判断 startup 状态机当前是否已进入 Ready。 */
	bool IsWeaponLoadoutStartupReady() const { return StartupRuntimeState.StartupState == EHeroWeaponLoadoutStartupState::Ready; }
	/** 只读判断 startup 状态机当前是否仍在 InProgress。 */
	bool IsWeaponLoadoutStartupInProgress() const { return StartupRuntimeState.StartupState == EHeroWeaponLoadoutStartupState::InProgress; }
	/** 只读判断 startup 状态机当前是否已进入 Failed。 */
	bool HasWeaponLoadoutStartupFailed() const { return StartupRuntimeState.StartupState == EHeroWeaponLoadoutStartupState::Failed; }
	/** 读取 startup 状态机当前正式阶段。 */
	EHeroWeaponLoadoutStartupState GetWeaponLoadoutStartupState() const { return StartupRuntimeState.StartupState; }
	/** 读取当前 startup 失败诊断文本。 */
	FString GetWeaponLoadoutStartupFailureReason() const { return StartupRuntimeState.FailureReason; }
	/** 读取当前仍处于 pending 集合中的槽位数量。 */
	int32 GetWeaponLoadoutStartupPendingSlotCount() const { return StartupRuntimeState.PendingStartupPrewarmSlotCount; }
	/** 读取本次 startup prewarm 总共统计过的槽位数量。 */
	int32 GetWeaponLoadoutStartupTotalSlotCount() const { return StartupRuntimeState.TotalStartupPrewarmSlotCount; }
	/** 只按 pending/total 集合推导 startup UI 进度。 */
	float GetWeaponLoadoutStartupProgressRatio() const;

	/** 查询指定槽位是否仍处于 startup prewarm 待完成集合中。 */
	bool IsLoadoutSlotStartupPrewarmPending(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** startup 进行中时响应槽位定义变化，刷新 pending prewarm 计数。 */
	void HandleLoadoutSlotDefinitionChanged(EHeroWeaponLoadoutSlot InLoadoutSlot, bool bHasAssignedWeaponDefinition);

	/** 标记一个槽位 startup prewarm 已完成，并尝试推进整体 startup 收尾。 */
	void MarkLoadoutSlotStartupPrewarmCompleted(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 将 startup 状态机切入失败态并广播失败原因。它不回滚 runtime 或 equipment 结果。 */
	void FailWeaponLoadoutStartup(EHeroWeaponLoadoutSlot InLoadoutSlot, const FString& InFailureReason);

	/** 在所有 pending 槽位完成后验证出生槽与当前装备态，并尝试切入 Ready。 */
	void TryFinishWeaponLoadoutStartup();

	/** 广播 UI 快照需要刷新。它只是 UI 脏标记，不表示装备状态必然变化。 */
	void BroadcastLoadoutUIStateChanged() const;

	/** 重置 startup 运行态和组件缓存。 */
	void ResetRuntimeStateForHeroStartup();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 缓存读取当前角色宿主。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	/** 缓存读取正式装备态宿主。 */
	UHeroEquipmentComponent* GetOwningHeroEquipmentComponent() const;
	/** 缓存读取发射物标签 / 属性缓存上下文宿主。 */
	UHeroLoadoutContextComponent* GetOwningHeroLoadoutContextComponent() const;
	/** 缓存读取资源链 / 实例链运行时宿主。 */
	UHeroLoadoutRuntimeComponent* GetOwningHeroLoadoutRuntimeComponent() const;

	/** 重置 startup 状态机到指定出生槽。只影响 StartupRuntimeState。 */
	void ResetWeaponLoadoutStartupState(EHeroWeaponLoadoutSlot InSpawnLoadoutSlot);

	/** 把指定槽位加入 startup prewarm 待完成集合。 */
	void MarkLoadoutSlotStartupPrewarmPending(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 重新启动 startup prewarm，负责四槽校验、pending 标记和 runtime 预热请求调度。 */
	bool RestartWeaponLoadoutStartup(EHeroWeaponLoadoutSlot InSpawnLoadoutSlot, bool bReleaseExistingAsyncHandles);

	/** 构建 startup 失败诊断文本。它只服务日志和失败原因，不参与业务判定。 */
	FString BuildLoadoutSlotStartupDebugContext(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 广播 startup ready，不附带额外业务写回。 */
	void BroadcastWeaponLoadoutStartupReady() const;
	/** 广播 startup failed，只同步失败槽位和诊断文本。 */
	void BroadcastWeaponLoadoutStartupFailed(EHeroWeaponLoadoutSlot InLoadoutSlot, const FString& InFailureReason) const;

protected:
	UPROPERTY()
	/** 当前 startup prewarm 正式状态源。 */
	FHeroWeaponLoadoutStartupRuntimeState StartupRuntimeState;

private:
	friend class UHeroEquipmentComponent;

	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	mutable TWeakObjectPtr<UHeroEquipmentComponent> CachedHeroEquipmentComponent;
	mutable TWeakObjectPtr<UHeroLoadoutContextComponent> CachedHeroLoadoutContextComponent;
	mutable TWeakObjectPtr<UHeroLoadoutRuntimeComponent> CachedHeroLoadoutRuntimeComponent;

	/** startup ready 广播宿主。 */
	FOnHeroWeaponLoadoutStartupReady WeaponLoadoutStartupReadyDelegate;
	/** startup failed 广播宿主。 */
	FOnHeroWeaponLoadoutStartupFailed WeaponLoadoutStartupFailedDelegate;
	/** UI 快照脏标记广播宿主。 */
	FOnHeroLoadoutUIStateChanged LoadoutUIStateChangedDelegate;
};
