// 文件说明：声明 ActionDebugHelper 相关类型与接口。

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(ActionRPG, Log, All);

namespace Debug
{
	ACTIONRPG_API void Print(const FString& Message, const FColor& Color = FColor::Yellow, const float Duration = 5.0f, const int32 InKey = -1);
}
