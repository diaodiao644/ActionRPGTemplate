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
 * 当前粒度已经稳定，后续默认冻结，不再拆成“轻转向组件 + 锁定组件”两块。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroTargetingComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroTargetingComponent();

public:
	bool TryResolveSimpleTurnAssistTarget(
		const FActionSimpleTurnAssistConfig& InTurnAssistConfig,
		EActionSimpleTurnAssistTriggerSource InTriggerSource,
		AActor*& OutTargetActor);

	/** 主动攻击起手正式转向入口：优先目标，其次输入方向。 */
	bool TryApplyOffensiveFacing(EActionSimpleTurnAssistTriggerSource InTriggerSource);

	bool TryApplySimpleTurnAssist(
		const FActionSimpleTurnAssistConfig& InTurnAssistConfig,
		EActionSimpleTurnAssistTriggerSource InTriggerSource);

	bool TryApplySimpleTurnAssistTowardTarget(
		AActor* InTargetActor,
		EActionSimpleTurnAssistTriggerSource InTriggerSource);

	const FActionSimpleTurnAssistConfig& GetAttackTurnAssistConfig() const
	{
		return AttackTurnAssistConfig;
	}

	const FActionSimpleTurnAssistConfig& GetSpiritOffensiveTurnAssistConfig() const
	{
		return SpiritOffensiveTurnAssistConfig;
	}

	FActionSimpleTurnAssistDebugSnapshot GetLastSimpleTurnAssistDebugSnapshot() const
	{
		return LastSimpleTurnAssistDebugSnapshot;
	}

	bool TryAcquireTargetLock();
	bool ToggleTargetLock();
	void ClearTargetLock(EActionTargetLockBreakReason InReason);

	bool IsTargetLockActive() const
	{
		return bTargetLockActive && LockedTargetActor.IsValid();
	}

	AActor* GetLockedTargetActor() const
	{
		return LockedTargetActor.Get();
	}

	FActionTargetLockDebugSnapshot GetLastTargetLockDebugSnapshot() const
	{
		return LastTargetLockDebugSnapshot;
	}

	const FActionTargetLockConfig& GetTargetLockConfig() const
	{
		return TargetLockConfig;
	}

	void ResetRuntimeStateForHeroStartup();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	AActionPlayerController* GetOwningHeroController() const;
	const FActionSimpleTurnAssistConfig* ResolveTurnAssistConfig(
		EActionSimpleTurnAssistTriggerSource InTriggerSource) const;
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

	bool IsValidSimpleTurnAssistTarget(AActor* InTargetActor) const;
	bool TryResolveSimpleTurnAssistCenterFacing(
		const FVector& InSearchStart,
		const FVector& InSearchForward,
		AActor* InTargetActor,
		float& OutFacingDot,
		float& OutAngleDegrees,
		float& OutDistanceSquared) const;
	bool IsValidTargetLockTarget(AActor* InTargetActor) const;
	bool TryApplyInputFacingFallback(EActionSimpleTurnAssistTriggerSource InTriggerSource);
	bool TryResolveTargetLockCandidate(AActor*& OutTargetActor);
	void UpdateTargetLockRuntime(float DeltaTime);
	void UpdateTargetLockFacing(float DeltaTime);
	void RefreshTargetLockSnapshotFromTarget(AActor* InTargetActor);
	void RefreshMovementOrientationFromLockState() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TurnAssist")
	FActionSimpleTurnAssistConfig AttackTurnAssistConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TurnAssist")
	FActionSimpleTurnAssistConfig SpiritOffensiveTurnAssistConfig;

	/** 是否绘制轻量转向辅助的搜索范围、角度边界和最终目标。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TurnAssist|Debug")
	bool bDrawSimpleTurnAssistDebug = false;

	/** 轻量转向辅助调试图在世界中保留的时间。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TurnAssist|Debug", meta = (ClampMin = "0.0"))
	float SimpleTurnAssistDebugDrawDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatConfig|TargetLock")
	FActionTargetLockConfig TargetLockConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FActionSimpleTurnAssistDebugSnapshot LastSimpleTurnAssistDebugSnapshot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FActionTargetLockDebugSnapshot LastTargetLockDebugSnapshot;

	UPROPERTY(Transient)
	bool bTargetLockActive = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LockedTargetActor;

private:
	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	mutable TWeakObjectPtr<AActionPlayerController> CachedHeroController;
};
