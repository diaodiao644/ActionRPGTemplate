#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatReactRecoveryUnlock.generated.h"

/**
 * 非处决受击开窗通知。
 * 它只负责在精确动画帧回调 CombatReact，
 * 让输入白名单与移动限制在这一帧正式恢复，尾段蒙太奇继续只保留表现语义。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_CombatReactRecoveryUnlock : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_CombatReactRecoveryUnlock();

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
