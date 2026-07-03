// 文件说明：实现 ActionDebugHelper 相关逻辑。


#include "Debug/ActionDebugHelper.h"

DEFINE_LOG_CATEGORY(ActionRPG);


void Debug::Print(const FString& Message, const FColor& Color, const float Duration, const int32 InKey)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(InKey, Duration, Color, Message);

		UE_LOG(ActionRPG, Log, TEXT("%s"), *Message);

	}
}
