// 文件说明：实现状态效果定义资产相关逻辑。

#include "DataAssets/Effects/DataAsset_StatusEffectDefinition.h"

bool UDataAsset_StatusEffectDefinition::IsValidDefinition() const
{
	// 最核心的约束是：每个状态定义都必须有可识别的标签，
	// 这样运行时快照才能稳定映射回对应的展示资产。
	// 它只回答“这份展示资产是否存在稳定标签语义”，
	// 不负责审查 DisplayData 是否足够丰富，也不表达该状态当前是否活跃。
	return StatusEffectTag.IsValid();
}
