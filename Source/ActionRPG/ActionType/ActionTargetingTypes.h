// 文件说明：声明轻量转向辅助与完整目标锁定共享使用的公共枚举和结构体。
#pragma once

#include "CoreMinimal.h"
#include "ActionTargetingTypes.generated.h"

class AActor;

/** 一次性轻量转向辅助本次是被哪条正式战斗链触发的，不表达持续锁定目标状态。 */
UENUM(BlueprintType)
enum class EActionSimpleTurnAssistTriggerSource : uint8
{
	Attack,
	Execution,
	SpiritOffensive
};

/**
 * 一次性轻量转向辅助的最小静态配置模板。
 * 它只描述搜索距离、搜索宽度和允许转向的最大夹角，
 * 不负责保存持续目标，也不等价于完整目标锁定系统。
 */
USTRUCT(BlueprintType)
struct FActionSimpleTurnAssistConfig
{
	GENERATED_BODY()

public:
	/** 当前这份一次性轻量转向辅助配置是否启用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnAssist")
	bool bEnabled = true;

	/** 前向搜索距离。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnAssist", meta = (ClampMin = "0.0"))
	float SearchDistance = 240.f;

	/** 搜索起点相对角色原点的高度偏移。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnAssist")
	float SearchHeightOffset = 50.f;

	/** 允许搜索目标时的左右半角。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnAssist", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxSearchHalfAngleDegrees = 70.f;

	/** 扇形多射线搜索时，相邻两条射线之间的角度步进。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnAssist", meta = (ClampMin = "0.1", ClampMax = "180.0"))
	float RayAngleStepDegrees = 5.f;

public:
	/** 只校验这份静态配置模板本身是否具备最小可用条件。 */
	bool IsEnabledConfig() const
	{
		return bEnabled && SearchDistance > 0.f && RayAngleStepDegrees > 0.f;
	}
};

/**
 * 最近一次轻量转向辅助的只读调试快照。
 * 它只服务日志、蓝图观察和运行时排查，不反向作为正式目标状态源。
 */
USTRUCT(BlueprintType)
struct FActionSimpleTurnAssistDebugSnapshot
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurnAssist")
	bool bAttempted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurnAssist")
	bool bFoundCandidate = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurnAssist")
	bool bAppliedTurn = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurnAssist")
	EActionSimpleTurnAssistTriggerSource TriggerSource =
		EActionSimpleTurnAssistTriggerSource::Attack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurnAssist")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurnAssist")
	float Distance = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurnAssist")
	float FacingDot = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurnAssist")
	float TargetAngleDegrees = 0.f;

public:
	/** 只回收这份调试快照，不清理任何正式目标锁定状态。 */
	void Reset()
	{
		bAttempted = false;
		bFoundCandidate = false;
		bAppliedTurn = false;
		TriggerSource = EActionSimpleTurnAssistTriggerSource::Attack;
		TargetActor = nullptr;
		Distance = 0.f;
		FacingDot = 0.f;
		TargetAngleDegrees = 0.f;
	}
};

/** 完整目标锁定被解除时的高层失锁原因分层，不是 targeting 组件内部阶段机。 */
UENUM(BlueprintType)
enum class EActionTargetLockBreakReason : uint8
{
	None,
	ManualRelease,
	AcquireFailed,
	TargetInvalid,
	TargetDead,
	OutOfRange,
	RuntimeReset
};

/**
 * 完整目标锁定的最小静态配置模板。
 * 它只描述怎么搜、何时超距失锁、角色和控制器如何对齐目标，
 * 正式锁定状态本体仍由独立 targeting 组件持有。
 */
USTRUCT(BlueprintType)
struct FActionTargetLockConfig
{
	GENERATED_BODY()

public:
	/** 当前这份完整目标锁定配置是否启用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TargetLock")
	bool bEnabled = true;

	/** 获取锁定目标时的前向搜索距离。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TargetLock", meta = (ClampMin = "0.0"))
	float AcquireSearchDistance = 900.f;

	/** 获取锁定目标时的前向扫描球半径。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TargetLock", meta = (ClampMin = "0.0"))
	float AcquireSearchRadius = 120.f;

	/** 获取锁定目标时的搜索高度偏移。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TargetLock")
	float AcquireSearchHeightOffset = 50.f;

	/** 获取锁定目标时允许的最大夹角。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TargetLock", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAcquireAngleDegrees = 80.f;

	/** 目标超出该距离后会正式失锁。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TargetLock", meta = (ClampMin = "0.0"))
	float BreakDistance = 1200.f;

	/** 锁定期间角色朝向目标的插值速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TargetLock", meta = (ClampMin = "0.0"))
	float ActorYawInterpSpeed = 12.f;

	/** 锁定期间控制器朝向目标的插值速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TargetLock", meta = (ClampMin = "0.0"))
	float ControllerYawInterpSpeed = 10.f;

public:
	/** 只判断这份静态配置模板是否具备最小启用条件。 */
	bool IsEnabledConfig() const
	{
		return bEnabled
			&& AcquireSearchDistance > 0.f
			&& AcquireSearchRadius > 0.f
			&& BreakDistance > 0.f;
	}
};

/**
 * 最近一次完整目标锁定状态的只读调试快照。
 * 它只服务日志、蓝图观察和运行时排查，不反向作为正式锁定状态源。
 */
USTRUCT(BlueprintType)
struct FActionTargetLockDebugSnapshot
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetLock")
	bool bLockActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetLock")
	bool bLastAcquireSucceeded = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetLock")
	TObjectPtr<AActor> LockedTargetActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetLock")
	float Distance = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetLock")
	float FacingDot = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TargetLock")
	EActionTargetLockBreakReason BreakReason = EActionTargetLockBreakReason::None;

public:
	/** 只清空这份调试快照，不直接解除正式锁定状态。 */
	void Reset()
	{
		bLockActive = false;
		bLastAcquireSucceeded = false;
		LockedTargetActor = nullptr;
		Distance = 0.f;
		FacingDot = 0.f;
		BreakReason = EActionTargetLockBreakReason::None;
	}
};
