// 文件说明：声明 ActionEnemyCharacter 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Characters/ActionCharacterBase.h"
#include "ActionEnemyCharacter.generated.h"

class UEnemyAttributeComponent;

/**
 * 敌方角色宿主壳。
 * 这一层负责在 ActionCharacterBase 之上挂接敌方专用组件，
 * 并提供敌方最小启动入口；正式属性、ASC、CombatReact、ExecutionWindow 等仍回到基础角色链与对应组件。
 */
UCLASS()
class ACTIONRPG_API AActionEnemyCharacter : public AActionCharacterBase
{
	GENERATED_BODY()

public:
	AActionEnemyCharacter();

protected:
	/** 敌人出生时的最小初始化入口。通常在这里串属性桥、AI 依赖或敌人侧只读镜像，不承担 Hero 装配链职责，也不是敌方战斗总控。 */
	virtual void BeginPlay() override;

protected:
	/** 敌人侧附加属性桥组件。它只负责把权威 AttributeSet 的结果桥给敌方 UI / 蓝图 / 调试消费，不替代基础 AttributeSet 本体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Enemy", meta = (ToolTip = "敌方属性变化广播桥与派生镜像入口。权威属性本体仍在 ActionAttributeSetBase；这里不形成第二套正式属性状态源。"))
	UEnemyAttributeComponent* EnemyAttributeComponent = nullptr;
};
