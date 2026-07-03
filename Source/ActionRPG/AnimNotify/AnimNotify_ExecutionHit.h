// 文件说明：声明处决命中帧动画通知，用于在精确帧回调当前激活的处决 Ability。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ExecutionHit.generated.h"

/**
 * 处决命中帧通知。
 * 使用方式：
 * 1. 把它放到处决蒙太奇真正应当命中的那一帧；
 * 2. Notify 触发时会查找当前角色身上正在激活的处决 Ability；
 * 3. 找到后调用 Ability 的命中帧入口，让处决伤害在精确帧落地。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_ExecutionHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ExecutionHit();

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
