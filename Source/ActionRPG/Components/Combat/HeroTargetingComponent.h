// 文件说明：声明英雄角色的轻量转向辅助与完整锁定目标组件。

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "ActionType/ActionTargetingTypes.h"
#include "HeroTargetingComponent.generated.h"

class AActionHeroCharacter;
class AActionPlayerController;

/**
 * 英雄 Targeting 组件。
 * 负责 `SimpleTurnAssist + TargetLock` 一体能力组的正式状态与正式读取入口。
 * 它是轻量转向与目标锁定的正式宿主，不是相机状态源，也不是输入状态源。
 * 当前粒度已经稳定，后续默认冻结，不再拆成“轻转向组件 + 锁定组件”两块。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroTargetingComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroTargetingComponent();

public:
	/** 搜索一次轻量转向辅助目标。它只解析本次应面向谁，不直接改写角色朝向。 */
	bool TryResolveSimpleTurnAssistTarget(
		const FActionSimpleTurnAssistConfig& InTurnAssistConfig,
		EActionSimpleTurnAssistTriggerSource InTriggerSource,
		AActor*& OutTargetActor);

	/** 主动攻击起手正式转向入口：优先目标，其次输入方向。 */
	bool TryApplyOffensiveFacing(EActionSimpleTurnAssistTriggerSource InTriggerSource);

	/** 尝试执行一次轻量转向辅助。它只服务本次起手面向，不形成新的持续锁定状态。 */
	bool TryApplySimpleTurnAssist(
		const FActionSimpleTurnAssistConfig& InTurnAssistConfig,
		EActionSimpleTurnAssistTriggerSource InTriggerSource);

	/** 直接朝指定目标执行一次轻量转向辅助。它只改这一次起手面向，不写入持续锁定态。 */
	bool TryApplySimpleTurnAssistTowardTarget(
		AActor* InTargetActor,
		EActionSimpleTurnAssistTriggerSource InTriggerSource);

	const FActionSimpleTurnAssistConfig& GetAttackTurnAssistConfig() const
	{
		return AttackTurnAssistConfig;
	}

	/** 读取 Spirit Offensive 起手默认消费的轻量转向辅助静态配置。 */
	const FActionSimpleTurnAssistConfig& GetSpiritOffensiveTurnAssistConfig() const
	{
		return SpiritOffensiveTurnAssistConfig;
	}

	/** 读取最近一次轻量转向辅助调试快照。它只服务调试和诊断，不作为正式状态源。 */
	FActionSimpleTurnAssistDebugSnapshot GetLastSimpleTurnAssistDebugSnapshot() const
	{
		return LastSimpleTurnAssistDebugSnapshot;
	}

	/** 尝试正式进入目标锁定状态。 */
	bool TryAcquireTargetLock();
	/** 切换目标锁定开关：有锁则解锁，无锁则尝试新锁定。 */
	bool ToggleTargetLock();
	/** 主动清空当前目标锁定，并记录这次中断原因。 */
	void ClearTargetLock(EActionTargetLockBreakReason InReason);

	/** 当前是否仍处于正式目标锁定态。 */
	bool IsTargetLockActive() const
	{
		return bTargetLockActive && LockedTargetActor.IsValid();
	}

	/** 读取当前正式锁定的目标 Actor。 */
	AActor* GetLockedTargetActor() const
	{
		return LockedTargetActor.Get();
	}

	/** 读取最近一次目标锁定调试快照。它只服务调试和诊断，不替代正式锁定态。 */
	FActionTargetLockDebugSnapshot GetLastTargetLockDebugSnapshot() const
	{
		return LastTargetLockDebugSnapshot;
	}

	/** 读取目标锁定静态配置。它是配置入口，不是当前运行态镜像。 */
	const FActionTargetLockConfig& GetTargetLockConfig() const
	{
		return TargetLockConfig;
	}

	/** Hero 启动链重置时回零轻量转向和目标锁定运行态，不保留旧锁定结果或调试镜像。 */
	void ResetRuntimeStateForHeroStartup();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** 目标锁定是持续运行态，因此需要 Tick 维护锁定资格和朝向；轻量转向本身不是持续 Tick 状态。 */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 解析拥有者 Hero 角色。它只是稳定宿主入口，不在这里创建第二套角色运行态。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	/** 解析拥有者玩家控制器。它主要服务输入方向回退和锁定态朝向更新。 */
	AActionPlayerController* GetOwningHeroController() const;
	/** 按触发来源解析这次应消费哪套轻量转向辅助配置。它只读静态配置，不返回运行态镜像。 */
	const FActionSimpleTurnAssistConfig* ResolveTurnAssistConfig(
		EActionSimpleTurnAssistTriggerSource InTriggerSource) const;
	/** 绘制最近一次轻量转向辅助的调试射线、角度边界和命中目标。 */
	void DrawSimpleTurnAssistDebug(
		const FActionSimpleTurnAssistConfig& InTurnAssistConfig,
		EActionSimpleTurnAssistTriggerSource InTriggerSource,
		const FVector& InSearchStart,
		const FVector& InSearchEnd,
		AActor* InResolvedTargetActor,
		bool bInResolvedFromTargetLock = false,
		const TCHAR* InDebugResultText = TEXT("Ray"),
		const TArray<FVector>* InRayDirections = nullptr,
		const TArray<int32>* InHitRayIndices = nullptr) const;

	/** 判断某个候选是否满足轻量转向辅助目标的最小资格。 */
	bool IsValidSimpleTurnAssistTarget(AActor* InTargetActor) const;
	/** 用中心向前方向解析某个目标是否落在当前轻量转向辅助允许范围内。 */
	bool TryResolveSimpleTurnAssistCenterFacing(
		const FVector& InSearchStart,
		const FVector& InSearchForward,
		AActor* InTargetActor,
		float& OutFacingDot,
		float& OutAngleDegrees,
		float& OutDistanceSquared) const;
	/** 判断某个候选是否满足正式目标锁定资格。 */
	bool IsValidTargetLockTarget(AActor* InTargetActor) const;
	/** 当没有可靠目标时，按当前输入方向做一次朝向回退。它只服务本次起手，不进入持续锁定。 */
	bool TryApplyInputFacingFallback(EActionSimpleTurnAssistTriggerSource InTriggerSource);
	/** 搜索一个当前最适合进入目标锁定的候选。它只解析候选，不直接落锁。 */
	bool TryResolveTargetLockCandidate(AActor*& OutTargetActor);
	/** 每帧维护目标锁定有效性，例如距离、朝向和目标存活状态。它只维护正式锁定运行态。 */
	void UpdateTargetLockRuntime(float DeltaTime);
	/** 每帧在锁定态下刷新角色朝向或镜头面向。 */
	void UpdateTargetLockFacing(float DeltaTime);
	/** 从当前锁定目标刷新一份对外可读的调试快照。它只服务诊断，不反推正式锁定状态。 */
	void RefreshTargetLockSnapshotFromTarget(AActor* InTargetActor);
	/** 按当前锁定状态刷新角色移动朝向策略。它只消费锁定结果，不创建新的移动状态源。 */
	void RefreshMovementOrientationFromLockState() const;

protected:
	/** 主动攻击起手默认消费的轻量转向辅助静态配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TurnAssist")
	FActionSimpleTurnAssistConfig AttackTurnAssistConfig;

	/** Spirit Offensive 起手默认消费的轻量转向辅助静态配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TurnAssist")
	FActionSimpleTurnAssistConfig SpiritOffensiveTurnAssistConfig;

	/** 是否绘制轻量转向辅助的搜索范围、角度边界和最终目标。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TurnAssist|Debug")
	bool bDrawSimpleTurnAssistDebug = false;

	/** 轻量转向辅助调试图在世界中保留的时间。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TurnAssist|Debug", meta = (ClampMin = "0.0"))
	float SimpleTurnAssistDebugDrawDuration = 1.5f;

	/** 目标锁定静态配置入口。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TargetLock")
	FActionTargetLockConfig TargetLockConfig;

	/** 最近一次轻量转向辅助的调试快照。它只记录诊断结果，不形成新的正式运行态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FActionSimpleTurnAssistDebugSnapshot LastSimpleTurnAssistDebugSnapshot;

	/** 最近一次目标锁定调试快照。它只服务排错和悬停查看。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FActionTargetLockDebugSnapshot LastTargetLockDebugSnapshot;

	/** 当前是否处于正式目标锁定态。它必须与 LockedTargetActor 一起读取。 */
	UPROPERTY(Transient)
	bool bTargetLockActive = false;

	/** 当前正式锁定的目标。只有它与 bTargetLockActive 组合后才表达完整锁定态。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LockedTargetActor;

private:
	/** 拥有者 Hero 的弱引用缓存。 */
	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	/** 拥有者玩家控制器的弱引用缓存。 */
	mutable TWeakObjectPtr<AActionPlayerController> CachedHeroController;
};
