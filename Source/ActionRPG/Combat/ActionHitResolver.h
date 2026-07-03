// 文件说明：声明单人版受击解析器相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionHitResolver.generated.h"

/**
 * 单人版受击解析器。
 * 当前版本只负责本地命中流程：攻击载荷 -> 受击窗口判定 -> 属性结算。
 */
UCLASS()
class ACTIONRPG_API UActionHitResolver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 解析一次命中并返回最终结果。
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	static FActionHitResolveResult ResolveHit(AActor* TargetActor, const FActionDamagePayload& DamagePayload);

	/**
	 * 单独把命中载荷中的 DOT 效果条目附着到目标。
	 * 适用场景：
	 * 1. 武器命中后只想附着 DOT 效果；
	 * 2. 某些技能没有首段直伤，但会直接施加中毒/灼烧；
	 * 3. 后续蓝图或 GameplayAbility 想复用同一套 DOT 资产入口时，不需要自己重复组装 Spec。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	static bool ApplyDamageOverTime(AActor* TargetActor, const FActionDamagePayload& DamagePayload);
};
