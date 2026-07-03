// 文件说明：声明 ANS_SetMoveMode 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_SetMoveMode.generated.h"

enum EMovementMode;

UCLASS()
class ACTIONRPG_API UANS_SetMoveMode : public UAnimNotifyState
{
	GENERATED_BODY()
public:

	UANS_SetMoveMode();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere)
	TEnumAsByte<EMovementMode> BeginMovementMode;	

	UPROPERTY(EditAnywhere)
	TEnumAsByte<EMovementMode> EndMovementMode;
};
