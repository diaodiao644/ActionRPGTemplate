// 文件说明：实现项目自定义资源管理器。

#include "System/ActionAssetManager.h"

#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Engine/Engine.h"

UActionAssetManager& UActionAssetManager::Get()
{
	// 正常运行时应当直接使用项目自定义的 AssetManager。
	if (GEngine && GEngine->AssetManager)
	{
		return *CastChecked<UActionAssetManager>(GEngine->AssetManager);
	}

	// 这里保留一层启动期保护：
	// 如果引擎里的 AssetManager 指针还没完成替换，就先从全局入口取实例，避免初始化阶段直接空指针崩溃。
	return *CastChecked<UActionAssetManager>(&UAssetManager::Get());
}

TSharedPtr<FStreamableHandle> UActionAssetManager::RequestSoftObjectAsyncLoad(
	const FSoftObjectPath& AssetPath,
	const FStreamableDelegate& OnLoadedDelegate,
	TAsyncLoadPriority Priority)
{
	if (AssetPath.IsNull())
	{
		if (OnLoadedDelegate.IsBound())
		{
			OnLoadedDelegate.Execute();
		}

		return nullptr;
	}

	// 统一通过 StreamableManager 处理软引用加载，避免业务组件自己直接同步 LoadSynchronous。
	return GetStreamableManager().RequestAsyncLoad(
		AssetPath,
		OnLoadedDelegate,
		Priority,
		false,
		false,
		TEXT("ActionSoftObjectAsyncLoad"));
}

TSharedPtr<FStreamableHandle> UActionAssetManager::RequestSoftObjectListAsyncLoad(
	const TArray<FSoftObjectPath>& AssetPaths,
	const FStreamableDelegate& OnLoadedDelegate,
	TAsyncLoadPriority Priority)
{
	if (AssetPaths.Num() <= 0)
	{
		if (OnLoadedDelegate.IsBound())
		{
			OnLoadedDelegate.Execute();
		}

		return nullptr;
	}

	return GetStreamableManager().RequestAsyncLoad(
		AssetPaths,
		OnLoadedDelegate,
		Priority,
		false,
		false,
		TEXT("ActionSoftObjectListAsyncLoad"));
}

TSharedPtr<FStreamableHandle> UActionAssetManager::RequestWeaponRuntimeAssetsAsyncLoad(
	const UDataAsset_WeaponDefinition* InWeaponDefinition,
	const FStreamableDelegate& OnLoadedDelegate,
	TAsyncLoadPriority Priority)
{
	// 武器资源预热入口只负责把 WeaponDefinition 收口好的软资源依赖汇总出来，再统一交给异步加载器。
	// 它不生成武器实例，也不推动切槽或装备链状态前进。
	TArray<FSoftObjectPath> WeaponRuntimeAssetPaths;
	CollectWeaponRuntimeAssetPaths(InWeaponDefinition, WeaponRuntimeAssetPaths);
	return RequestSoftObjectListAsyncLoad(WeaponRuntimeAssetPaths, OnLoadedDelegate, Priority);
}

bool UActionAssetManager::AreWeaponRuntimeAssetsReady(const UDataAsset_WeaponDefinition* InWeaponDefinition) const
{
	TArray<FSoftObjectPath> WeaponRuntimeAssetPaths;
	CollectWeaponRuntimeAssetPaths(InWeaponDefinition, WeaponRuntimeAssetPaths);

	// 这里判断的只是“当前武器定义依赖的软资源是否都已经能解析成已加载对象”。
	// 它不等价于当前槽位武器已装备完成，也不代表武器 Actor 已经生成并挂到角色身上。
	// 只要其中任何一个资源还无法解析成已加载对象，就说明这把武器还没完成运行时预热。
	for (const FSoftObjectPath& AssetPath : WeaponRuntimeAssetPaths)
	{
		if (!AssetPath.IsNull() && !AssetPath.ResolveObject())
		{
			return false;
		}
	}

	return true;
}

void UActionAssetManager::CollectWeaponRuntimeAssetPaths(const UDataAsset_WeaponDefinition* InWeaponDefinition, TArray<FSoftObjectPath>& OutAssetPaths) const
{
	OutAssetPaths.Reset();

	if (!InWeaponDefinition)
	{
		return;
	}

	// 这里是 WeaponDefinition 运行时依赖的统一软资源收集出口。
	// 只消费武器定义本体已经收口过的配置入口，不在 AssetManager 里重复发明另一套资源来源。

	// 收集武器 Actor 类。
	if (!InWeaponDefinition->WeaponActorClass.IsNull())
	{
		OutAssetPaths.AddUnique(InWeaponDefinition->WeaponActorClass.ToSoftObjectPath());
	}

	// 收集武器默认发射物资源。
	// 发射物相关依赖仍然来自 WeaponDefinition 的正式配置模板；这里只负责转成待预热的软引用路径。
	InWeaponDefinition->GetDefaultProjectileConfig().CollectSoftObjectPaths(OutAssetPaths);

	// 收集处决专用配置依赖的执行者蒙太奇与命中表现资源。
	InWeaponDefinition->GetExecutionConfig().CollectSoftObjectPaths(OutAssetPaths);

	// 收集可切换发射物资源。
	for (const FActionSwitchableProjectileConfigEntry& Entry : InWeaponDefinition->SwitchableProjectileConfigs)
	{
		Entry.CollectSoftObjectPaths(OutAssetPaths);
	}

	// 收集武器定义里集中管理的动画资源。
	InWeaponDefinition->GetAnimationConfig().CollectSoftObjectPaths(OutAssetPaths);

	// 收集灵武器一体化 Ability 条目依赖的蒙太奇、命中与发射物资源。
	// AssetManager 只桥接这些静态依赖，不成为 Spirit 技能运行态宿主。
	for (const FActionSpiritAbilityEntryConfig& SpiritAbilityEntry : InWeaponDefinition->GetSpiritAbilityEntryConfigs())
	{
		SpiritAbilityEntry.CollectSoftObjectPaths(OutAssetPaths);
	}
}
