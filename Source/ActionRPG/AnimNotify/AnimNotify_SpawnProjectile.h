// 文件说明：声明发射物生成动画通知。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnProjectile.generated.h"

/**
 * 发射物生成通知。
 * 把它放到攻击蒙太奇真正应生成发射物的那一帧，
 * 通知会去查当前角色的 HeroAttackComponent，并请求生成当前攻击段的发射物。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_SpawnProjectile : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_SpawnProjectile();

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
