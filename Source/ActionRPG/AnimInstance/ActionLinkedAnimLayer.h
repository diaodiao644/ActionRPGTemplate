// 文件说明：声明 ActionLinkedAnimLayer 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "AnimInstance/ActionBaseAnimInstance.h"
#include "ActionLinkedAnimLayer.generated.h"

class UActionCharacterAnimInstance;

UCLASS()
class ACTIONRPG_API UActionLinkedAnimLayer : public UActionBaseAnimInstance
{
	GENERATED_BODY()

public:
	UActionLinkedAnimLayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

};
