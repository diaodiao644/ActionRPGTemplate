// 文件说明：实现 PawnExtensionComponentBase 相关逻辑。

#include "Components/PawnExtensionComponentBase.h"

UPawnExtensionComponentBase::UPawnExtensionComponentBase()
	: Super()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}
