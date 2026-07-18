// 文件说明：声明发射物生成动画通知。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnProjectile.generated.h"

/**
 * 发射物生成通知。
 * 把它放到攻击蒙太奇真正应生成发射物的那一帧，
 * 通知会去查当前角色的 HeroAttackComponent，并请求生成当前攻击段的发射物。
 * 它只负责出弹时机桥接，不替代攻击条目、发射物静态模板、攻击段运行态或发射物实例状态本身。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_SpawnProjectile : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_SpawnProjectile();

public:
	/** 返回编辑器内显示名，方便资产作者直接看出这是发射物生成时机桥接通知。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在精确帧请求 `HeroAttackComponent` 生成当前攻击段已经解析好的正式发射物。它只桥接“何时出弹”，不自己解析伤害、发射物配置或命中逻辑。 */
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
