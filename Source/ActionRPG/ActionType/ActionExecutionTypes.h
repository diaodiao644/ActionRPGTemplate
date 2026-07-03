// 文件说明：声明处决窗口运行时状态。

#pragma once

#include "CoreMinimal.h"
#include "ActionExecutionTypes.generated.h"

class AActor;

/**
 * 处决窗口运行时状态。
 * 作用：
 * 1. 把“当前是否处于可处决窗口”“最近一次是谁打开了该窗口”“窗口预占和受害者锁定”收口到同一份状态；
 * 2. 避免处决窗口组件继续散落 bool、Instigator 与计时器字段；
 * 3. 后续若扩展多阶段处决窗口或不同来源的失衡窗口，也应从这份状态继续演进。
 */
USTRUCT(BlueprintType)
struct FActionExecutionWindowRuntimeState
{
	GENERATED_BODY()

public:
	/** 当前是否处于可处决窗口。 */
	bool bExecutionWindowOpen = false;

	/** 最近一次打开该处决窗口的触发者。 */
	TWeakObjectPtr<AActor> LastExecutionInstigator;

	/** 当前这次处决窗口是否已经被某个施加者预占。 */
	bool bExecutionWindowReserved = false;

	/** 当前预占了这次处决窗口的施加者。 */
	TWeakObjectPtr<AActor> ReservedExecutionInstigator;

	/** 当前目标是否已经被某个处决流程锁定为处决受害者。 */
	bool bExecutionVictimLocked = false;

	/** 当前锁定这名处决受害者的执行者。 */
	TWeakObjectPtr<AActor> ExecutionVictimLockInstigator;

	/** 处决窗口超时计时器。 */
	FTimerHandle ExecutionWindowTimerHandle;

public:
	bool IsWindowOpen() const
	{
		return bExecutionWindowOpen;
	}

	AActor* GetLastExecutionInstigator() const
	{
		return LastExecutionInstigator.Get();
	}

	AActor* GetReservedExecutionInstigator() const
	{
		return ReservedExecutionInstigator.Get();
	}

	bool HasReservedExecution() const
	{
		return bExecutionWindowReserved && ReservedExecutionInstigator.IsValid();
	}

	bool IsReservedBy(AActor* InInstigatorActor) const
	{
		return HasReservedExecution() && ReservedExecutionInstigator.Get() == InInstigatorActor;
	}

	AActor* GetExecutionVictimLockInstigator() const
	{
		return ExecutionVictimLockInstigator.Get();
	}

	bool HasExecutionVictimLock() const
	{
		return bExecutionVictimLocked && ExecutionVictimLockInstigator.IsValid();
	}

	bool IsExecutionVictimLockedBy(AActor* InInstigatorActor) const
	{
		return HasExecutionVictimLock() && ExecutionVictimLockInstigator.Get() == InInstigatorActor;
	}

	void OpenWindow(AActor* InInstigatorActor)
	{
		bExecutionWindowOpen = true;
		LastExecutionInstigator = InInstigatorActor;
		bExecutionWindowReserved = false;
		ReservedExecutionInstigator.Reset();
	}

	void CloseWindow()
	{
		bExecutionWindowOpen = false;
		bExecutionWindowReserved = false;
		ReservedExecutionInstigator.Reset();
	}

	void ReserveWindow(AActor* InInstigatorActor)
	{
		bExecutionWindowReserved = true;
		ReservedExecutionInstigator = InInstigatorActor;
	}

	void LockExecutionVictim(AActor* InInstigatorActor)
	{
		bExecutionVictimLocked = true;
		ExecutionVictimLockInstigator = InInstigatorActor;
	}

	void ClearReservation()
	{
		bExecutionWindowReserved = false;
		ReservedExecutionInstigator.Reset();
	}

	void ClearExecutionVictimLock()
	{
		bExecutionVictimLocked = false;
		ExecutionVictimLockInstigator.Reset();
	}

	void ClearLastExecutionInstigator()
	{
		LastExecutionInstigator.Reset();
		ClearReservation();
	}
};
