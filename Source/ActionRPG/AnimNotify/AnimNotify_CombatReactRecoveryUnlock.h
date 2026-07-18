#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatReactRecoveryUnlock.generated.h"

/**
 * 非处决受击恢复开窗通知。
 * 它只负责在精确帧通知 `ActionCombatReactComponent`：
 * 当前非处决受击已经进入允许恢复白名单输入与移动的阶段。
 * 它只负责精确帧开窗，不形成新的受击或恢复状态源。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_CombatReactRecoveryUnlock : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_CombatReactRecoveryUnlock();

public:
	/** 返回编辑器内显示名，方便资产作者直接看出这是非处决受击恢复开窗桥接通知。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在精确帧通知 `ActionCombatReactComponent` 把当前非处决受击切到恢复开窗阶段。它只桥接开窗结果，不直接决定白名单之外的业务状态。 */
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
