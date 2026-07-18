// 文件说明：声明 ActionEnhancedInputComponent 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"

#include "DataAssets/Input/DataAsset_InputConfig.h"

#include "ActionEnhancedInputComponent.generated.h"

/**
 * Enhanced Input 到正式输入标签的批量绑定桥。
 * 它只负责读取静态输入配置资产，并把 InputAction + InputTag
 * 批量绑定到 Pressed / Released / Held 回调；
 * 不持有正式战斗输入状态，也不替代 HeroCombatInputComponent 的输入运行态。
 */
UCLASS()
class ACTIONRPG_API UActionEnhancedInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	UActionEnhancedInputComponent();

	/**
	 * 把输入配置里的静态映射批量绑定到回调函数。
	 * 它只是绑定桥，不推进输入资格判断、缓冲写入或关系裁决。
	 * 运行时透传的 `InputTag` 只是正式语义入口；后续是否消费仍回到外层正式链。
	 */
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(const UDataAsset_InputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc);
	
};

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
inline void UActionEnhancedInputComponent::BindAbilityActions(const UDataAsset_InputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc)
{
	if (!IsValid(InputConfig))
	{
		UE_LOG(LogTemp, Warning, TEXT("Input Is Null"))
			return;
	}

	for (const FActionInputBinding& InputBinding : InputConfig->AbilityInputBindings)
	{
		if (!IsValid(InputBinding.InputAction) || !InputBinding.InputTag.IsValid())
		{
			continue;
		}

		if (PressedFunc)
		{
			// 运行时只把静态 InputTag 透传给回调；后续是否消费、缓存或进入 GAS 仍由外层正式链决定。
			BindAction(InputBinding.InputAction, ETriggerEvent::Started, Object, std::forward<PressedFuncType>(PressedFunc), InputBinding.InputTag);
		}

		if (ReleasedFunc)
		{
			BindAction(InputBinding.InputAction, ETriggerEvent::Completed, Object, std::forward<ReleasedFuncType>(ReleasedFunc), InputBinding.InputTag);
		}

		if (HeldFunc)
		{
			BindAction(InputBinding.InputAction, ETriggerEvent::Triggered, Object, std::forward<HeldFuncType>(HeldFunc), InputBinding.InputTag);
		}
	}
}
