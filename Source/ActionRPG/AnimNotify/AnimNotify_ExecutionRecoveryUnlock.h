#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ExecutionRecoveryUnlock.generated.h"

/**
 * 处决恢复开窗通知。
 * 它只负责在精确动画帧回调当前激活的处决恢复 GA，
 * 让目标从这一帧开始恢复可受伤、可触发主动 GA、可被普通战斗链接管。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_ExecutionRecoveryUnlock : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ExecutionRecoveryUnlock();

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
