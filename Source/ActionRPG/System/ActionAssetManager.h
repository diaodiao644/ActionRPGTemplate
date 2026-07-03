// 文件说明：声明项目自定义资源管理器，统一承接武器定义到运行时资源预热的桥接逻辑。

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "ActionAssetManager.generated.h"

class UDataAsset_WeaponDefinition;

/**
 * 项目自定义 AssetManager。
 * 当前版本主要负责两类资源请求：
 * 1. 软引用武器定义资产；
 * 2. 某把武器定义所依赖的运行时资源，如武器 Actor 类、动画层、特殊切武动画。
 *
 * 在武器资源链里，它的职责始终是“静态武器定义桥接 + 软资源预热”：
 * 读取 WeaponDefinition 的软资源引用、发起异步预热，并判断这些运行时资产是否就绪。
 * 它不是武器定义本体，也不是装备域正式运行态宿主。
 */
UCLASS()
class ACTIONRPG_API UActionAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	/** 获取项目级 AssetManager 实例。*/
	static UActionAssetManager& Get();

	/** 异步加载一个软引用对象。*/
	TSharedPtr<FStreamableHandle> RequestSoftObjectAsyncLoad(
		const FSoftObjectPath& AssetPath,
		const FStreamableDelegate& OnLoadedDelegate = FStreamableDelegate(),
		TAsyncLoadPriority Priority = 0);

	/** 异步加载一组软引用对象。*/
	TSharedPtr<FStreamableHandle> RequestSoftObjectListAsyncLoad(
		const TArray<FSoftObjectPath>& AssetPaths,
		const FStreamableDelegate& OnLoadedDelegate = FStreamableDelegate(),
		TAsyncLoadPriority Priority = 0);

	/** 异步预热某把武器定义依赖的运行时资源。只负责资源层，不负责生成武器实例或推进装备链。*/
	TSharedPtr<FStreamableHandle> RequestWeaponRuntimeAssetsAsyncLoad(
		const UDataAsset_WeaponDefinition* InWeaponDefinition,
		const FStreamableDelegate& OnLoadedDelegate = FStreamableDelegate(),
		TAsyncLoadPriority Priority = 0);

	/** 判断某把武器定义依赖的运行时资源是否已经就绪。这里只回答资源层，不等价于武器已装备完成。*/
	bool AreWeaponRuntimeAssetsReady(const UDataAsset_WeaponDefinition* InWeaponDefinition) const;

	/** 收集武器定义依赖的运行时软引用路径。作为统一软资源收集出口，不重复发明另一套来源。*/
	void CollectWeaponRuntimeAssetPaths(const UDataAsset_WeaponDefinition* InWeaponDefinition, TArray<FSoftObjectPath>& OutAssetPaths) const;
};
