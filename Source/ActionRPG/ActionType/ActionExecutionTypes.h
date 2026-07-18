// 文件说明：声明处决窗口运行时状态。

#pragma once

#include "CoreMinimal.h"
#include "ActionExecutionTypes.generated.h"

class AActor;

/**
 * 目标侧处决窗口公共运行态。
 * 它只承载“窗口是否开启、最近开窗来源、当前正式预占、当前 victim lock 和超时句柄”这组公共 runtime，
 * 供 `ExecutionWindowComponent` 统一读写，不在这里承接执行者侧处决流程、窗口资格裁决或最终收尾语义。
 */
USTRUCT(BlueprintType)
struct FActionExecutionWindowRuntimeState
{
	GENERATED_BODY()

public:
	/** 当前目标是否仍处于可处决窗口。它只回答窗口是否开启，不等于窗口是否已被正式预占或正式消费。 */
	bool bExecutionWindowOpen = false;

	/** 最近一次打开该处决窗口的触发者。它只记录最近开窗来源，不等于当前正式预占者。 */
	TWeakObjectPtr<AActor> LastExecutionInstigator;

	/** 当前这次处决窗口是否已经被某个执行者正式预占。 */
	bool bExecutionWindowReserved = false;

	/** 当前正式预占了这次处决窗口的执行者。只有与 bExecutionWindowReserved 同时成立时，才表达正式预占态。 */
	TWeakObjectPtr<AActor> ReservedExecutionInstigator;

	/** 当前目标是否已经被某个处决流程锁定为受害者。它表达的是正式 victim lock，不等于窗口一定仍开放。 */
	bool bExecutionVictimLocked = false;

	/** 当前锁定这名处决受害者的执行者。只有与 bExecutionVictimLocked 同时成立时，才表达正式 victim lock 归属。 */
	TWeakObjectPtr<AActor> ExecutionVictimLockInstigator;

	/** 处决窗口超时计时器。它只服务窗口生命周期收尾，不表达新的业务状态。 */
	FTimerHandle ExecutionWindowTimerHandle;

public:
	/** 只读当前公共 runtime 里的开窗位。 */
	bool IsWindowOpen() const
	{
		return bExecutionWindowOpen;
	}

	/** 只读最近一次开窗来源，不推断当前是否仍可处决。 */
	AActor* GetLastExecutionInstigator() const
	{
		return LastExecutionInstigator.Get();
	}

	/** 只读当前 runtime 记录的预占者。外层仍需结合 HasReservedExecution 判断正式预占是否成立。 */
	AActor* GetReservedExecutionInstigator() const
	{
		return ReservedExecutionInstigator.Get();
	}

	/** 判断当前公共 runtime 是否已经形成有效正式预占。 */
	bool HasReservedExecution() const
	{
		return bExecutionWindowReserved && ReservedExecutionInstigator.IsValid();
	}

	/** 判断当前正式预占是否来自指定执行者。它只读 runtime，不做资格裁决。 */
	bool IsReservedBy(AActor* InInstigatorActor) const
	{
		return HasReservedExecution() && ReservedExecutionInstigator.Get() == InInstigatorActor;
	}

	/** 只读当前 victim lock 的归属执行者。 */
	AActor* GetExecutionVictimLockInstigator() const
	{
		return ExecutionVictimLockInstigator.Get();
	}

	/** 判断当前公共 runtime 是否已经形成有效 victim lock。 */
	bool HasExecutionVictimLock() const
	{
		return bExecutionVictimLocked && ExecutionVictimLockInstigator.IsValid();
	}

	/** 判断当前 victim lock 是否来自指定执行者。它只读 runtime，不处理演出期收尾。 */
	bool IsExecutionVictimLockedBy(AActor* InInstigatorActor) const
	{
		return HasExecutionVictimLock() && ExecutionVictimLockInstigator.Get() == InInstigatorActor;
	}

	/** 打开目标侧处决窗口并刷新最近开窗来源，同时清掉旧预占。它只是 runtime 局部写入口。 */
	void OpenWindow(AActor* InInstigatorActor)
	{
		bExecutionWindowOpen = true;
		LastExecutionInstigator = InInstigatorActor;
		bExecutionWindowReserved = false;
		ReservedExecutionInstigator.Reset();
	}

	/** 关闭目标侧处决窗口并清掉当前预占。它不负责 victim lock 收尾或窗口消费裁决。 */
	void CloseWindow()
	{
		bExecutionWindowOpen = false;
		bExecutionWindowReserved = false;
		ReservedExecutionInstigator.Reset();
	}

	/** 写入当前正式预占者。它只改 runtime 壳，不替代外层 Reserve/Commit 主链。 */
	void ReserveWindow(AActor* InInstigatorActor)
	{
		bExecutionWindowReserved = true;
		ReservedExecutionInstigator = InInstigatorActor;
	}

	/** 写入当前 victim lock 归属。它只改 runtime 壳，不替代外层硬锁与演出保护收尾。 */
	void LockExecutionVictim(AActor* InInstigatorActor)
	{
		bExecutionVictimLocked = true;
		ExecutionVictimLockInstigator = InInstigatorActor;
	}

	/** 只清当前正式预占，不影响开窗位和最近开窗来源。 */
	void ClearReservation()
	{
		bExecutionWindowReserved = false;
		ReservedExecutionInstigator.Reset();
	}

	/** 只清当前 victim lock，不替代外层受害者保护效果收尾。 */
	void ClearExecutionVictimLock()
	{
		bExecutionVictimLocked = false;
		ExecutionVictimLockInstigator.Reset();
	}

	/** 清最近开窗来源，并顺手清掉当前预占。它通常用于局部收尾，不表示窗口一定已经正式消费。 */
	void ClearLastExecutionInstigator()
	{
		LastExecutionInstigator.Reset();
		ClearReservation();
	}
};
