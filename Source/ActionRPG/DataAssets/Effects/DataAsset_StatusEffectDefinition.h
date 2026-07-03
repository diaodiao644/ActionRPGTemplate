// 文件说明：声明状态效果定义资产相关接口，用于统一收口状态标签语义与展示数据。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionEffectTypes.h"
#include "DataAsset_StatusEffectDefinition.generated.h"

/**
 * 状态效果定义资产。
 * 设计目标如下：
 * 1. 统一收口“燃烧/中毒/流血”等状态的标签语义与展示信息；
 * 2. 避免后续 UI 直接把中文名、颜色、图标硬编码在 Widget 或蓝图逻辑里；
 * 3. 让 ASC 只返回标签与运行时快照，再由这份资产补全展示层所需的数据。
 *
 * 它本质上是“状态标签语义 + 展示数据”的静态配置模板，
 * 不直接保存任何活跃状态运行态、层数或剩余时间。
 */
UCLASS(BlueprintType)
class ACTIONRPG_API UDataAsset_StatusEffectDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 校验这份状态定义是否满足运行时最小要求。 */
	UFUNCTION(BlueprintPure, Category = "StatusEffect")
	bool IsValidDefinition() const;

	/** 读取状态效果标签。 */
	UFUNCTION(BlueprintPure, Category = "StatusEffect")
	FGameplayTag GetStatusEffectTag() const { return StatusEffectTag; }

	/** 读取状态效果展示数据。 */
	const FActionStatusEffectDisplayData& GetDisplayData() const { return DisplayData; }

public:
	// 资产面板字段：
	// 这份资产只收口状态标签语义和展示数据，不直接保存任何活跃状态运行态。
	/** 当前状态效果的唯一标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "状态效果的唯一正式标签，例如燃烧、中毒或流血。运行时主要用它关联 ASC 返回的状态语义，不在这份资产里保存活跃层数或剩余时间。"))
	FGameplayTag StatusEffectTag;

	/** 当前状态效果对应的展示数据。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "状态效果对应的展示数据，包括名称、描述、图标和颜色。它只服务 HUD / MVVM / 蓝图展示，不承担状态生命周期。"))
	FActionStatusEffectDisplayData DisplayData;
};
