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
	/** 所有 linked layer 的最薄公共基类。它只承接表现层继承入口，不自持角色或战斗正式状态。 */
	UActionLinkedAnimLayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
