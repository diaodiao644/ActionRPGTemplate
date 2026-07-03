// 文件说明：声明 ActionEnhancedInputComponent 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"

#include "DataAssets/Input/DataAsset_InputConfig.h"

#include "ActionEnhancedInputComponent.generated.h"

UCLASS()
class ACTIONRPG_API UActionEnhancedInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	UActionEnhancedInputComponent();

	//将传入的函数与输入配置中的IA绑定
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
		if (!IsValid(InputBinding.InputAction) || !InputBinding.InputTag.IsValid())continue;

		if (PressedFunc)
		{
			//最后的可变参数将在 触发回调时 传入 回调函数  AB->Delegate.BindDelegate<UserClass>(Object, Func, Vars...);
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
