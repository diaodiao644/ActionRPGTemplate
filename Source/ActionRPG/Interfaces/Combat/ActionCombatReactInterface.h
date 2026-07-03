// 文件说明：声明战斗反应接口，供玩家与敌人共用。

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionHitTypes.h"
#include "UObject/Interface.h"
#include "ActionCombatReactInterface.generated.h"

UINTERFACE(BlueprintType)
class ACTIONRPG_API UActionCombatReactInterface : public UInterface
{
	GENERATED_BODY()
};

class ACTIONRPG_API IActionCombatReactInterface
{
	GENERATED_BODY()

public:
	// 处理一个外部战斗反应事件，例如受击、格挡或完美闪避触发。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action|Combat")
	bool HandleCombatReactEvent(FGameplayTag EventTag, const FActionDamagePayload& DamagePayload);
};
