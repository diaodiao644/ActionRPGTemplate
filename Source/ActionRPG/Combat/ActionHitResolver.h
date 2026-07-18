// 文件说明：声明单人版受击解析器相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionHitResolver.generated.h"

/**
 * 单人版受击解析器。
 * 当前版本只负责本地命中流程：外部已组装 DamagePayload -> 前置判定 -> 属性结算 -> 结果回传。
 * 它是统一命中解析入口，不自持角色、武器、命中窗口、发射物、处决窗口或输入恢复状态；
 * 真正的正式状态源仍分别回到 CombatReact / Defense / Execution / Attribute / Equipment / HitSource 等宿主。
 */
UCLASS()
class ACTIONRPG_API UActionHitResolver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 解析一次命中并返回最终结果。
	 * 这里读取的是外部已经组装好的 `FActionDamagePayload`，并把命中是否生效、
	 * 命中事件和结算结果统一收口成 `FActionHitResolveResult`。
	 * 它负责“判”和“结”，不负责“开窗”“登记命中”“恢复输入”或维护受击运行态本体。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat", meta = (ToolTip = "统一解析一次命中。它消费外部已构建好的 DamagePayload 和目标当前正式状态，返回命中是否生效及结算结果，但不创建新的命中窗口、命中源或受击状态宿主。"))
	static FActionHitResolveResult ResolveHit(AActor* TargetActor, const FActionDamagePayload& DamagePayload);

	/**
	 * 单独把命中载荷中的 DOT 效果条目附着到目标。
	 * 适用场景：
	 * 1. 武器命中后只想附着 DOT 效果；
	 * 2. 某些技能没有首段直伤，但会直接施加中毒/灼烧；
	 * 3. 后续蓝图或 GameplayAbility 想复用同一套 DOT 资产入口时，不需要自己重复组装 Spec。
	 * 这个入口只处理 DOT 效果应用，不补做一次完整命中解析，
	 * 也不生成新的命中窗口、命中结果或受击状态源。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat", meta = (ToolTip = "只把 DamagePayload 里的 DOT 效果条目附着到目标。它不补做一次完整命中解析，也不创建新的命中窗口或命中状态。"))
	static bool ApplyDamageOverTime(AActor* TargetActor, const FActionDamagePayload& DamagePayload);
};
