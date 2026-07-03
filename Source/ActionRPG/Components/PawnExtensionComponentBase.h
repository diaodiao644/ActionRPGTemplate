// 文件说明：声明 PawnExtensionComponentBase 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PawnExtensionComponentBase.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONRPG_API UPawnExtensionComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:
	// 构造函数：初始化组件默认值。
	UPawnExtensionComponentBase();

protected:
	// 生命周期：游戏开始时调用。

	// 获取拥有该组件的 Pawn，并转换成指定类型。
	template<class T>
	T* GetOwningPawn() const
	{
		static_assert(TIsDerivedFrom<T, APawn>::Value, "T must be derived from APawn");
		return Cast<T>(GetOwner());
	}

	// 获取拥有该组件的原始 Pawn 指针。
	APawn* GetOwningPawn() const
	{
		return Cast<APawn>(GetOwner());
	}

	// 获取拥有该组件的控制器，并转换成指定类型。
	template<class T>
	T* GetOwningController()
	{
		static_assert(TIsDerivedFrom<T, AController>::Value, "T must be derived from AController");
		if (APawn* OwningPawn = GetOwningPawn())
		{
			return OwningPawn->GetController<T>();
		}

		return nullptr;
	}
};
