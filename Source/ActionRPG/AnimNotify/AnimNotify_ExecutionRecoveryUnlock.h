#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ExecutionRecoveryUnlock.generated.h"

/**
 * 处决恢复开窗通知。
 * 它只负责在精确动画帧通知目标侧 ExecutionWindowComponent：
 * 当前处决硬锁已经允许正式解除。
 * 让目标从这一帧开始恢复可受伤、可触发主动 GA、可被普通战斗链接管；
 * 它不是目标侧窗口本体，也不负责关窗、恢复 Poise 或裁决窗口是否已正式消费。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_ExecutionRecoveryUnlock : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ExecutionRecoveryUnlock();

public:
	/** 返回编辑器内显示名，方便资产作者直接看出这是目标侧处决恢复开窗桥接通知。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在精确帧通知目标侧 `ExecutionWindowComponent` 解除处决硬锁。它只桥接这一帧的解锁时机，不负责恢复输入或结束整个目标侧窗口生命周期。 */
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
