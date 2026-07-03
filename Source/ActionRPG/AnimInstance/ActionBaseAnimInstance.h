// 文件说明：声明 ActionBaseAnimInstance 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ActionBaseAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONRPG_API UActionBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	public:
		UActionBaseAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
};
