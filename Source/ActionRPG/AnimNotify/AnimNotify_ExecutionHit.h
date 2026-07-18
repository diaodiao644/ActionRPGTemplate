// 文件说明：声明处决命中帧动画通知，用于在精确帧回调当前激活的处决 Ability。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ExecutionHit.generated.h"

/**
 * 处决命中帧通知。
 * 它只负责在精确帧把“现在该命中了”这件事桥接给当前激活的 `HeroGA_Execution`，
 * 不负责处决资格判断、victim lock 状态维护或命中结果缓存。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_ExecutionHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ExecutionHit();

public:
	/** 返回编辑器内显示名，方便资产作者直接看出这是处决命中帧桥接通知。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在精确帧查找当前激活的 `HeroGA_Execution`，并把正式处决命中桥接给它。它只负责单帧通知，不重建伤害载荷或第二套处决状态。 */
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
