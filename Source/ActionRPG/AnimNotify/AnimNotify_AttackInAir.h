// 文件说明：声明 AnimNotify_AttackInAir 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttackInAir.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_AttackInAir : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_AttackInAir();

	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	
};
