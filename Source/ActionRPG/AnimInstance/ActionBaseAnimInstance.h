// 文件说明：声明 ActionBaseAnimInstance 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ActionBaseAnimInstance.generated.h"

/**
 * 所有主 ABP 与 linked layer 的最薄公共基类。
 * 它只承接 Unreal 动画实例继承入口，不在这里自持角色、战斗、输入或武器的正式业务状态。
 */
UCLASS()
class ACTIONRPG_API UActionBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** 构造最薄公共动画实例基类。它只负责继承层初始化，不在这里建立业务镜像。 */
	UActionBaseAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
