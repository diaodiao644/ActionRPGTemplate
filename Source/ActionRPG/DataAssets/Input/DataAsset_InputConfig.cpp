// 文件说明：实现 DataAsset_InputConfig 相关逻辑。

#include "DataAssets/Input/DataAsset_InputConfig.h"

#include "InputAction.h"

const UInputAction* UDataAsset_InputConfig::FindAbilityInputActionForTag(const FGameplayTag& Tag, bool bLogNotFound) const
{
	for (const FActionInputBinding& InputBindingInfo : AbilityInputBindings)
	{
		// 这里只做“正式输入标签 -> 输入动作资源”的静态映射查询。
		// MatchesTag 允许调用侧用统一根标签回查资源，但不会在这里推进输入消费或运行时资格判断。
		if (InputBindingInfo.InputTag.MatchesTag(Tag) && IsValid(InputBindingInfo.InputAction))
		{
			return InputBindingInfo.InputAction;
		}
	}

	if (bLogNotFound)
	{
		// 查不到资源时只输出调试日志，帮助资产作者发现映射缺口。
		// 这里不兜底生成默认输入动作，也不把“没查到”升级成运行时输入状态。
		UE_LOG(LogTemp, Warning, TEXT("Not Found Tag [%s] From [%s] or Null InputAction in DataAsset"), *Tag.ToString(), *GetName());
	}

	return nullptr;
}
