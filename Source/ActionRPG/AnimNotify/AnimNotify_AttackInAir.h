// 文件说明：声明 AnimNotify_AttackInAir 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttackInAir.generated.h"

/**
 * 空中攻击段落跳转通知。
 * 它只负责在精确帧根据角色是否仍处于 Falling，
 * 把当前攻击蒙太奇跳到 Loop 或 End 段。
 * 这里只处理空中攻击表现分流，不承担攻击资格判断、连段状态源或正式空中状态源职责。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_AttackInAir : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_AttackInAir();

	/** 返回编辑器内显示名，方便资产作者直接看出这是空中攻击分流桥接通知。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在精确帧检查当前角色是否仍在空中，并把蒙太奇切到 Loop / End 段。它只返回当前表现分流结果，不承担攻击资格或正式空中状态裁决。 */
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	
};
