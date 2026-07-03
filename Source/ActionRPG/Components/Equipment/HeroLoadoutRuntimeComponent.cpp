// 文件说明：实现英雄装备域资源链 / 实例链运行时组件。

#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"

#include "Characters/ActionHeroCharacter.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Equipment/HeroLoadoutStateComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Items/Weapons/HeroWeaponBase.h"
#include "Kismet/GameplayStatics.h"
#include "System/ActionAssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroLoadoutRuntimeComponent, Log, All);

namespace HeroLoadoutRuntimeLog
{
	static FString GetWeaponLoadoutSlotName(const EHeroWeaponLoadoutSlot InLoadoutSlot)
	{
		if (const UEnum* LoadoutSlotEnum = StaticEnum<EHeroWeaponLoadoutSlot>())
		{
			return LoadoutSlotEnum->GetNameStringByValue(static_cast<int64>(InLoadoutSlot));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InLoadoutSlot));
	}

	static FString GetWeaponCategoryName(const EHeroWeaponCategory InWeaponCategory)
	{
		if (const UEnum* WeaponCategoryEnum = StaticEnum<EHeroWeaponCategory>())
		{
			return WeaponCategoryEnum->GetNameStringByValue(static_cast<int64>(InWeaponCategory));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InWeaponCategory));
	}
}

UHeroLoadoutRuntimeComponent::UHeroLoadoutRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHeroLoadoutRuntimeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRuntimeStateForEndPlay();
	Super::EndPlay(EndPlayReason);
}

void UHeroLoadoutRuntimeComponent::EnsureLoadoutSlotRuntimeState(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	// runtime 组件按槽位懒建最小骨架，避免还没注册到装备域的槽位提前持有多余资源链状态。
	LoadoutRuntimeAssetsBySlot.FindOrAdd(InLoadoutSlot);
	ClearLoadoutDefinitionFailureLogState(InLoadoutSlot);
}

bool UHeroLoadoutRuntimeComponent::IsLoadoutSlotRuntimeRegistered(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	return LoadoutRuntimeAssetsBySlot.Contains(InLoadoutSlot);
}

int32 UHeroLoadoutRuntimeComponent::GetLoadoutSlotLoadRevision(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (const FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot))
	{
		return RuntimeAssetState->LoadRevision;
	}

	return INDEX_NONE;
}

int32 UHeroLoadoutRuntimeComponent::AdvanceLoadoutSlotLoadRevision(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutRuntimeAssetState& RuntimeAssetState = LoadoutRuntimeAssetsBySlot.FindOrAdd(InLoadoutSlot);
	// revision 的职责不是表达业务阶段，而是让旧 definition/runtime assets 回调在结构变更后整体失效。
	ClearLoadoutDefinitionFailureLogState(InLoadoutSlot);
	return RuntimeAssetState.AdvanceLoadRevision();
}

UDataAsset_WeaponDefinition* UHeroLoadoutRuntimeComponent::GetLoadedWeaponDefinitionByLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (const FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot))
	{
		return RuntimeAssetState->LoadedWeaponDefinition;
	}

	return nullptr;
}

AHeroWeaponBase* UHeroLoadoutRuntimeComponent::GetSpawnedWeaponInstanceByLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (const FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot))
	{
		return RuntimeAssetState->SpawnedWeaponInstance;
	}

	return nullptr;
}

bool UHeroLoadoutRuntimeComponent::HasSpawnedWeaponInstanceByLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	return GetSpawnedWeaponInstanceByLoadoutSlot(InLoadoutSlot) != nullptr;
}

bool UHeroLoadoutRuntimeComponent::IsLoadoutSlotRuntimeReady(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	const UDataAsset_WeaponDefinition* LoadedWeaponDefinition = GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot);
	if (!LoadedWeaponDefinition)
	{
		return false;
	}

	const bool bRequiresRuntimeWeaponInstance =
		(LoadedWeaponDefinition->GetWeaponCategory() != EHeroWeaponCategory::Unarmed)
		|| !LoadedWeaponDefinition->WeaponActorClass.IsNull();
	return UActionAssetManager::Get().AreWeaponRuntimeAssetsReady(LoadedWeaponDefinition)
		&& (!bRequiresRuntimeWeaponInstance || HasSpawnedWeaponInstanceByLoadoutSlot(InLoadoutSlot));
}

FString UHeroLoadoutRuntimeComponent::DescribeWeaponRuntimeAssetReadiness(
	const UDataAsset_WeaponDefinition* InWeaponDefinition) const
{
	if (!InWeaponDefinition)
	{
		return TEXT("武器定义为空");
	}

	TArray<FSoftObjectPath> RuntimeAssetPaths;
	UActionAssetManager::Get().CollectWeaponRuntimeAssetPaths(InWeaponDefinition, RuntimeAssetPaths);

	TArray<FString> MissingAssetPaths;
	for (const FSoftObjectPath& AssetPath : RuntimeAssetPaths)
	{
		if (!AssetPath.IsNull() && !AssetPath.ResolveObject())
		{
			MissingAssetPaths.Add(AssetPath.ToString());
		}
	}

	if (MissingAssetPaths.Num() <= 0)
	{
		return TEXT("全部运行时资源已预热");
	}

	return FString::Printf(
		TEXT("仍缺少运行时资源=[%s]"),
		*FString::Join(MissingAssetPaths, TEXT(", ")));
}

void UHeroLoadoutRuntimeComponent::SetPendingEquipWhenReadyRequest(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const int32 InExpectedLoadRevision)
{
	// 这里只登记“这条资源链收尾后是否要回头补一次正式装备”。
	// 真正的 EquippedWeaponState 写回仍由 equipment 宿主在后续收尾里裁决。
	PendingEquipWhenReadyLoadoutSlot = InLoadoutSlot;
	PendingEquipWhenReadyLoadRevision = InExpectedLoadRevision;
}

bool UHeroLoadoutRuntimeComponent::MatchesPendingEquipWhenReadyRequest(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const int32 InExpectedLoadRevision) const
{
	// 挂起自动补装备请求必须同时命中槽位和 revision，
	// 避免旧请求在槽位换定义或重新发起加载后误落到新一轮资源链上。
	return PendingEquipWhenReadyLoadoutSlot == InLoadoutSlot
		&& PendingEquipWhenReadyLoadRevision == InExpectedLoadRevision;
}

void UHeroLoadoutRuntimeComponent::ClearPendingEquipWhenReadyRequest()
{
	// 这里只回收挂起意图壳，不反向修改 equipment 宿主里已经落地的正式装备态。
	PendingEquipWhenReadyLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
	PendingEquipWhenReadyLoadRevision = INDEX_NONE;
}

void UHeroLoadoutRuntimeComponent::RequestLoadoutSlotDefinitionAsync(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const bool bEquipWhenReady,
	const int32 InExpectedLoadRevision)
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetStateForRevision(InLoadoutSlot, InExpectedLoadRevision);
	if (!EquipmentComponent || !RuntimeAssetState)
	{
		return;
	}

	const FHeroWeaponLoadoutRuntimeState* RuntimeState = EquipmentComponent->FindRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return;
	}

	if (RuntimeState->LoadoutDefinition.DefaultWeaponDefinition.IsNull())
	{
		// 空定义不需要真正发起软引用加载，但仍统一走同一条“definition 已完成”收尾口，
		// 保持后续 revision 校验、startup 失败和自动补装备语义一致。
		HandleLoadoutSlotDefinitionLoaded(InLoadoutSlot, bEquipWhenReady, InExpectedLoadRevision);
		return;
	}

	UE_LOG(
		LogHeroLoadoutRuntimeComponent,
		Log,
		TEXT("开始异步加载固定武器槽定义。槽位=%s，LoadRevision=%d，bEquipWhenReady=%d"),
		*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		InExpectedLoadRevision,
		bEquipWhenReady ? 1 : 0);

	ReleaseStreamableHandle(RuntimeAssetState->DefinitionLoadHandle);
	// 发新请求前先释放旧 handle，避免同一槽位同 revision 下残留重复 definition 加载链。
	RuntimeAssetState->DefinitionLoadHandle = UActionAssetManager::Get().RequestSoftObjectAsyncLoad(
		RuntimeState->LoadoutDefinition.DefaultWeaponDefinition.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(
			this,
			&ThisClass::HandleLoadoutSlotDefinitionLoaded,
			InLoadoutSlot,
			bEquipWhenReady,
			InExpectedLoadRevision));
}

void UHeroLoadoutRuntimeComponent::RequestLoadoutSlotRuntimeAssetsAsync(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	UDataAsset_WeaponDefinition* InWeaponDefinition,
	const bool bEquipWhenReady,
	const int32 InExpectedLoadRevision)
{
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetStateForRevision(InLoadoutSlot, InExpectedLoadRevision);
	if (!RuntimeAssetState || !InWeaponDefinition)
	{
		return;
	}

	if (UActionAssetManager::Get().AreWeaponRuntimeAssetsReady(InWeaponDefinition))
	{
		// runtime assets 已经全部就绪时，直接复用统一收尾口，避免再分一条同步特例链。
		HandleLoadoutSlotRuntimeAssetsLoaded(
			InLoadoutSlot,
			InWeaponDefinition->WeaponTag,
			bEquipWhenReady,
			InExpectedLoadRevision);
		return;
	}

	UE_LOG(
		LogHeroLoadoutRuntimeComponent,
		Log,
		TEXT("开始异步预热固定武器槽运行时资源。槽位=%s，武器Tag=%s，LoadRevision=%d，bEquipWhenReady=%d"),
		*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		*InWeaponDefinition->WeaponTag.ToString(),
		InExpectedLoadRevision,
		bEquipWhenReady ? 1 : 0);

	ReleaseStreamableHandle(RuntimeAssetState->RuntimeAssetLoadHandle);
	// 这里和 definition 加载一样，统一在发新请求前释放旧 handle，保证每个槽位同一时刻只有一条正式预热线。
	RuntimeAssetState->RuntimeAssetLoadHandle = UActionAssetManager::Get().RequestWeaponRuntimeAssetsAsyncLoad(
		InWeaponDefinition,
		FStreamableDelegate::CreateUObject(
			this,
			&ThisClass::HandleLoadoutSlotRuntimeAssetsLoaded,
			InLoadoutSlot,
			InWeaponDefinition->WeaponTag,
			bEquipWhenReady,
			InExpectedLoadRevision));
}

void UHeroLoadoutRuntimeComponent::ReleaseLoadoutSlotRuntimeResources(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const bool bClearLoadedWeaponDefinition,
	const bool bDestroySpawnedInstance)
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
	if (!EquipmentComponent || !RuntimeAssetState)
	{
		return;
	}

	ReleaseStreamableHandle(RuntimeAssetState->DefinitionLoadHandle);
	ReleaseStreamableHandle(RuntimeAssetState->RuntimeAssetLoadHandle);
	// runtime 组件只负责收这把武器当前挂在该槽位上的定义附加能力和资源链缓存。
	// 当前是否仍是“手上已装备武器”要由 equipment 宿主在正式装备态层处理。
	EquipmentComponent->RemoveWeaponDefinitionAbilities(InLoadoutSlot);

	if (bDestroySpawnedInstance)
	{
		DestroyLoadoutSlotWeaponInstance(InLoadoutSlot);
	}

	if (bClearLoadedWeaponDefinition)
	{
		// 清 definition 缓存时，同时把 context 里的属性缓存 / 选中发射物等派生上下文一起回零，
		// 避免外部继续消费一份已经失效的槽位定义镜像。
		RuntimeAssetState->LoadedWeaponDefinition = nullptr;
		ClearLoadoutDefinitionFailureLogState(InLoadoutSlot);
		if (UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent())
		{
			LoadoutContextComponent->ResetLoadoutSlotContextRuntime(InLoadoutSlot);
		}
	}
}

void UHeroLoadoutRuntimeComponent::CancelLoadoutSlotAsyncRequestsAndAdvanceRevision(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
	if (!RuntimeAssetState)
	{
		return;
	}

	ReleaseStreamableHandle(RuntimeAssetState->DefinitionLoadHandle);
	ReleaseStreamableHandle(RuntimeAssetState->RuntimeAssetLoadHandle);
	// 这里真正重要的是推进 revision，让已经飞出去的旧回调即便晚到也无法再命中当前槽位状态。
	RuntimeAssetState->AdvanceLoadRevision();
	ClearLoadoutDefinitionFailureLogState(InLoadoutSlot);

	if (PendingEquipWhenReadyLoadoutSlot == InLoadoutSlot)
	{
		ClearPendingEquipWhenReadyRequest();
	}
}

bool UHeroLoadoutRuntimeComponent::EnsureLoadoutSlotWeaponInstance(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
	if (!RuntimeAssetState)
	{
		return false;
	}

	if (!RuntimeAssetState->LoadedWeaponDefinition)
	{
		// 当前槽位已经没有已加载定义时，实例链必须同步收空，避免旧武器实例悬挂在空槽位上。
		DestroyLoadoutSlotWeaponInstance(InLoadoutSlot);
		return true;
	}

	const bool bShouldSpawnRuntimeWeaponInstance =
		(RuntimeAssetState->LoadedWeaponDefinition->GetWeaponCategory() != EHeroWeaponCategory::Unarmed)
		|| !RuntimeAssetState->LoadedWeaponDefinition->WeaponActorClass.IsNull();
	if (!bShouldSpawnRuntimeWeaponInstance)
	{
		// 空手或纯数据型定义不需要实际武器 Actor；这里显式销毁旧实例，保证槽位实例语义和定义一致。
		DestroyLoadoutSlotWeaponInstance(InLoadoutSlot);
		return true;
	}

	if (RuntimeAssetState->SpawnedWeaponInstance
		&& RuntimeAssetState->SpawnedWeaponInstance->GetWeaponDefinition() == RuntimeAssetState->LoadedWeaponDefinition)
	{
		// 已有实例和当前定义完全一致时直接复用，避免无意义重建。
		return true;
	}

	AHeroWeaponBase* PreviousWeaponInstance = RuntimeAssetState->SpawnedWeaponInstance;
	AHeroWeaponBase* SpawnedWeapon = SpawnWeaponFromDefinition(RuntimeAssetState->LoadedWeaponDefinition);
	if (!SpawnedWeapon)
	{
		return false;
	}

	UDataAsset_WeaponDefinition* LoadedWeaponDefinition = RuntimeAssetState->LoadedWeaponDefinition;
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !LoadedWeaponDefinition)
	{
		SpawnedWeapon->Destroy();
		return false;
	}

	const FGameplayTag WeaponSubtypeTag = LoadedWeaponDefinition->GetWeaponSubtypeTag();
	const FName SocketName = OwnerHeroCharacter->GetWeaponSocketBySubtypeTag(WeaponSubtypeTag);
	if (SocketName.IsNone())
	{
		UE_LOG(
			LogHeroLoadoutRuntimeComponent,
			Warning,
			TEXT("武器实例创建后附加失败：未找到武器小类别挂点。槽位=%s，WeaponTag=%s，WeaponSubtypeTag=%s"),
			*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*LoadedWeaponDefinition->WeaponTag.ToString(),
			*WeaponSubtypeTag.ToString());
		SpawnedWeapon->Destroy();
		return false;
	}

	if (UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		// 新实例创建完成后，先让装备宿主把它落成“未装备表现”。
		// 真正是否切到手上，要等后续正式装备写回 EquippedWeaponState 时再决定。
		if (!EquipmentComponent->SetWeaponEquippedState(SpawnedWeapon, false))
		{
			UE_LOG(
				LogHeroLoadoutRuntimeComponent,
				Warning,
				TEXT("武器实例创建后附加失败：角色未能应用未装备表现。槽位=%s，WeaponTag=%s，WeaponSubtypeTag=%s，Socket=%s"),
				*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
				*LoadedWeaponDefinition->WeaponTag.ToString(),
				*WeaponSubtypeTag.ToString(),
				*SocketName.ToString());
			SpawnedWeapon->Destroy();
			return false;
		}
	}
	else
	{
		SpawnedWeapon->ApplyWeaponPresentationState(EActionWeaponPresentationState::Holstered);
		SpawnedWeapon->SetActorHiddenInGame(true);
	}

	if (PreviousWeaponInstance)
	{
		// 只有新实例已经成功创建并挂到角色后，才回收旧实例，
		// 避免槽位在重建过程中出现“旧实例已销毁但新实例也没立住”的真空状态。
		DestroyLoadoutSlotWeaponInstance(InLoadoutSlot);
		RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
		if (!RuntimeAssetState)
		{
			SpawnedWeapon->Destroy();
			return false;
		}
	}

	RuntimeAssetState->SpawnedWeaponInstance = SpawnedWeapon;
	return true;
}

void UHeroLoadoutRuntimeComponent::DestroyLoadoutSlotWeaponInstance(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
	if (!EquipmentComponent || !RuntimeAssetState || !RuntimeAssetState->SpawnedWeaponInstance)
	{
		return;
	}

	AHeroWeaponBase* WeaponInstance = RuntimeAssetState->SpawnedWeaponInstance;
	// 销毁前先停攻击检测并切回未装备表现，避免旧实例在销毁前后继续残留命中检测或可见武器态。
	WeaponInstance->EndAttackDetection();
	EquipmentComponent->SetWeaponEquippedState(WeaponInstance, false);
	WeaponInstance->Destroy();

	if (EquipmentComponent->EquippedWeaponState.EquippedWeaponInstance == WeaponInstance)
	{
		// 如果这把实例当前正被装备宿主引用，也要把宿主里的实例指针一并清掉，
		// 防止“当前生效装备态”继续指向一个已销毁 Actor。
		EquipmentComponent->EquippedWeaponState.EquippedWeaponInstance = nullptr;
	}

	RuntimeAssetState->SpawnedWeaponInstance = nullptr;
}

bool UHeroLoadoutRuntimeComponent::ResolveEquipTargetWeaponInstance(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	UDataAsset_WeaponDefinition* InWeaponDefinition,
	AHeroWeaponBase*& OutWeaponInstance)
{
	OutWeaponInstance = nullptr;

	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
	if (!RuntimeAssetState)
	{
		return false;
	}

	if (!InWeaponDefinition)
	{
		// 空手等不需要实例的定义，目标实例可以合法为空；正式装备写回仍由 equipment 宿主处理。
		return true;
	}

	const bool bShouldSpawnRuntimeWeaponInstance =
		(InWeaponDefinition->GetWeaponCategory() != EHeroWeaponCategory::Unarmed)
		|| !InWeaponDefinition->WeaponActorClass.IsNull();
	if (!bShouldSpawnRuntimeWeaponInstance)
	{
		return true;
	}

	OutWeaponInstance = RuntimeAssetState->SpawnedWeaponInstance;
	if (OutWeaponInstance && OutWeaponInstance->GetWeaponDefinition() == InWeaponDefinition)
	{
		// runtime 组件只负责保证“目标实例可解析”。
		// 只要当前缓存实例已经匹配目标定义，就不再重复触发实例重建。
		return true;
	}

	if (!EnsureLoadoutSlotWeaponInstance(InLoadoutSlot))
	{
		return false;
	}

	RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
	OutWeaponInstance = RuntimeAssetState ? RuntimeAssetState->SpawnedWeaponInstance : nullptr;
	return OutWeaponInstance != nullptr;
}

void UHeroLoadoutRuntimeComponent::ResetRuntimeStateForEndPlay()
{
	TArray<EHeroWeaponLoadoutSlot> RegisteredSlots;
	LoadoutRuntimeAssetsBySlot.GetKeys(RegisteredSlots);

	// 组件退场时统一收资源链、实例链和挂起自动补装备请求，
	// 避免异步回调或外部系统在 EndPlay 后继续命中旧宿主缓存。
	for (const EHeroWeaponLoadoutSlot LoadoutSlot : RegisteredSlots)
	{
		ReleaseLoadoutSlotRuntimeResources(LoadoutSlot, true, true);
	}

	LoadoutRuntimeAssetsBySlot.Reset();
	ClearPendingEquipWhenReadyRequest();
	DefinitionFailureLogStatesBySlot.Reset();
	CachedHeroCharacter.Reset();
	CachedHeroEquipmentComponent.Reset();
	CachedHeroLoadoutContextComponent.Reset();
	CachedHeroLoadoutStateComponent.Reset();
}

AActionHeroCharacter* UHeroLoadoutRuntimeComponent::GetOwningHeroCharacter() const
{
	if (CachedHeroCharacter.IsValid())
	{
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedHeroCharacter.Get();
}

UHeroEquipmentComponent* UHeroLoadoutRuntimeComponent::GetOwningHeroEquipmentComponent() const
{
	if (CachedHeroEquipmentComponent.IsValid())
	{
		return CachedHeroEquipmentComponent.Get();
	}

	CachedHeroEquipmentComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UHeroEquipmentComponent>()
		: nullptr;

	return CachedHeroEquipmentComponent.Get();
}

UHeroLoadoutContextComponent* UHeroLoadoutRuntimeComponent::GetOwningHeroLoadoutContextComponent() const
{
	if (CachedHeroLoadoutContextComponent.IsValid())
	{
		return CachedHeroLoadoutContextComponent.Get();
	}

	CachedHeroLoadoutContextComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UHeroLoadoutContextComponent>()
		: nullptr;

	return CachedHeroLoadoutContextComponent.Get();
}

UHeroLoadoutStateComponent* UHeroLoadoutRuntimeComponent::GetOwningHeroLoadoutStateComponent() const
{
	if (CachedHeroLoadoutStateComponent.IsValid())
	{
		return CachedHeroLoadoutStateComponent.Get();
	}

	CachedHeroLoadoutStateComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UHeroLoadoutStateComponent>()
		: nullptr;

	return CachedHeroLoadoutStateComponent.Get();
}

FHeroLoadoutRuntimeAssetState* UHeroLoadoutRuntimeComponent::FindRuntimeAssetState(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	return LoadoutRuntimeAssetsBySlot.Find(InLoadoutSlot);
}

const FHeroLoadoutRuntimeAssetState* UHeroLoadoutRuntimeComponent::FindRuntimeAssetState(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	return LoadoutRuntimeAssetsBySlot.Find(InLoadoutSlot);
}

FHeroLoadoutRuntimeAssetState* UHeroLoadoutRuntimeComponent::FindRuntimeAssetStateForRevision(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const int32 InExpectedLoadRevision)
{
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
	// 这里只做“这条回调是不是还属于当前正式版本”的判定；
	// 一旦槽位 revision 已推进，旧回调就必须在这里被静默丢弃。
	if (!RuntimeAssetState || RuntimeAssetState->LoadRevision != InExpectedLoadRevision)
	{
		return nullptr;
	}

	return RuntimeAssetState;
}

UDataAsset_WeaponDefinition* UHeroLoadoutRuntimeComponent::ResolveLoadoutSlotWeaponDefinitionAfterLoad(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetState(InLoadoutSlot);
	if (!EquipmentComponent || !RuntimeAssetState)
	{
		return nullptr;
	}

	const FHeroWeaponLoadoutRuntimeState* RuntimeState = EquipmentComponent->FindRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return nullptr;
	}

	RuntimeAssetState->LoadedWeaponDefinition = nullptr;
	// definition 重新解析前，先清掉旧 definition 派生的上下文缓存，
	// 避免属性缓存和选中发射物标签继续挂在一份即将失效的定义上。
	if (UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent())
	{
		LoadoutContextComponent->ClearLoadoutSlotWeaponAttributeCache(InLoadoutSlot);
	}

	if (RuntimeState->LoadoutDefinition.DefaultWeaponDefinition.IsNull())
	{
		// 槽位定义已被清空时，说明这次 definition 加载完成后已经不该再保留任何旧资源链结果。
		ReleaseLoadoutSlotRuntimeResources(InLoadoutSlot, true, true);
		return nullptr;
	}

	UDataAsset_WeaponDefinition* WeaponDefinition = RuntimeState->LoadoutDefinition.DefaultWeaponDefinition.Get();
	if (!WeaponDefinition)
	{
		UE_LOG(
			LogHeroLoadoutRuntimeComponent,
			Warning,
			TEXT("固定武器槽软引用加载完成，但解析到的武器定义为空。槽位=%s"),
			*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return nullptr;
	}

	if (!WeaponDefinition->IsValidDefinition())
	{
		// definition 软引用能解出来，不代表它已经具备正式进入当前槽位运行时的最小入口。
		// 这里继续沿用资产侧校验口径，避免非法定义把后续预热链和实例链拖进半有效状态。
		const FString WeaponActorClassPath = WeaponDefinition->WeaponActorClass.IsNull()
			? TEXT("None")
			: WeaponDefinition->WeaponActorClass.ToSoftObjectPath().ToString();
		LogInvalidLoadoutDefinitionOnce(
			InLoadoutSlot,
			RuntimeAssetState->LoadRevision,
			WeaponDefinition->WeaponTag,
			WeaponDefinition->DescribeValidationFailure(),
			WeaponDefinition->GetWeaponSubtypeTag().ToString(),
			HeroLoadoutRuntimeLog::GetWeaponCategoryName(WeaponDefinition->GetWeaponCategory()),
			WeaponActorClassPath);
		return nullptr;
	}

	if (!EquipmentComponent->DoesWeaponDefinitionMatchLoadoutSlot(WeaponDefinition, InLoadoutSlot))
	{
		// definition 还必须再次命中固定槽位类别约束，防止异步期间槽位定义变化后把错误类别武器落进当前 runtime 壳。
		UE_LOG(
			LogHeroLoadoutRuntimeComponent,
			Warning,
			TEXT("武器类别与固定武器槽要求不匹配。槽位=%s，武器Tag=%s，武器类别=%s，槽位要求类别=%s"),
			*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*WeaponDefinition->WeaponTag.ToString(),
			*HeroLoadoutRuntimeLog::GetWeaponCategoryName(WeaponDefinition->GetWeaponCategory()),
			*HeroLoadoutRuntimeLog::GetWeaponCategoryName(RuntimeState->LoadoutDefinition.AllowedWeaponCategory));
		return nullptr;
	}

	RuntimeAssetState->LoadedWeaponDefinition = WeaponDefinition;
	ClearLoadoutDefinitionFailureLogState(InLoadoutSlot);
	// 只有 definition 本身通过合法性和槽位类别校验后，才允许刷新该槽位的属性缓存镜像。
	if (UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent())
	{
		LoadoutContextComponent->RefreshLoadoutSlotWeaponAttributeCache(InLoadoutSlot, WeaponDefinition);
	}
	EquipmentComponent->BroadcastLoadoutUIStateChanged();
	return WeaponDefinition;
}

void UHeroLoadoutRuntimeComponent::LogInvalidLoadoutDefinitionOnce(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const int32 InLoadRevision,
	const FGameplayTag& InWeaponTag,
	const FString& InFailureReason,
	const FString& InWeaponSubtypeTagText,
	const FString& InWeaponCategoryText,
	const FString& InWeaponActorClassPath)
{
	FHeroLoadoutDefinitionFailureLogState& FailureLogState = DefinitionFailureLogStatesBySlot.FindOrAdd(InLoadoutSlot);
	if (FailureLogState.Matches(InLoadRevision, InWeaponTag, InFailureReason))
	{
		return;
	}

	FailureLogState.LoadRevision = InLoadRevision;
	FailureLogState.WeaponTag = InWeaponTag;
	FailureLogState.FailureReason = InFailureReason;

	UE_LOG(
		LogHeroLoadoutRuntimeComponent,
		Warning,
		TEXT("武器定义非法，忽略该固定武器槽配置。槽位=%s，武器Tag=%s，WeaponSubtypeTag=%s，武器类别=%s，武器类=%s，失败原因=%s"),
		*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		*InWeaponTag.ToString(),
		*InWeaponSubtypeTagText,
		*InWeaponCategoryText,
		*InWeaponActorClassPath,
		*InFailureReason);
}

void UHeroLoadoutRuntimeComponent::ClearLoadoutDefinitionFailureLogState(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	DefinitionFailureLogStatesBySlot.Remove(InLoadoutSlot);
}

void UHeroLoadoutRuntimeComponent::HandleLoadoutSlotDefinitionLoaded(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const bool bEquipWhenReady,
	const int32 InExpectedLoadRevision)
{
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetStateForRevision(InLoadoutSlot, InExpectedLoadRevision);
	if (!RuntimeAssetState)
	{
		return;
	}

	const bool bShouldAutoEquip = bEquipWhenReady
		&& MatchesPendingEquipWhenReadyRequest(InLoadoutSlot, InExpectedLoadRevision);

	ReleaseStreamableHandle(RuntimeAssetState->DefinitionLoadHandle);
	// 到这里 definition 软引用只是“已加载完成”，并不自动等于可正式进入当前槽位。
	// 仍要经过解析、合法性、槽位类别与上下文刷新校验后，才能继续推进 runtime assets 预热。

	UDataAsset_WeaponDefinition* WeaponDefinition = ResolveLoadoutSlotWeaponDefinitionAfterLoad(InLoadoutSlot);
	if (!WeaponDefinition)
	{
		if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
			LoadoutStateComponent && LoadoutStateComponent->IsLoadoutSlotStartupPrewarmPending(InLoadoutSlot))
		{
			LoadoutStateComponent->FailWeaponLoadoutStartup(
				InLoadoutSlot,
				TEXT("固定武器槽定义异步加载完成，但定义解析或校验失败。"));
		}

		if (bShouldAutoEquip)
		{
			if (UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
			{
				EquipmentComponent->BroadcastWeaponLoadoutEquipFailed(InLoadoutSlot);
				EquipmentComponent->BroadcastLoadoutUIStateChanged();
			}
		}
		else if (UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
		{
			EquipmentComponent->BroadcastLoadoutUIStateChanged();
		}

		return;
	}

	// definition 校验通过后，再继续推进 runtime assets 预热。
	// startup 失败、普通 UI 刷新和自动补装备是否继续，都统一从后续收尾口判断。
	RequestLoadoutSlotRuntimeAssetsAsync(InLoadoutSlot, WeaponDefinition, bEquipWhenReady, InExpectedLoadRevision);

	if (UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		EquipmentComponent->BroadcastLoadoutUIStateChanged();
	}
}

void UHeroLoadoutRuntimeComponent::HandleLoadoutSlotRuntimeAssetsLoaded(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FGameplayTag InWeaponTag,
	const bool bEquipWhenReady,
	const int32 InExpectedLoadRevision)
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	FHeroLoadoutRuntimeAssetState* RuntimeAssetState = FindRuntimeAssetStateForRevision(InLoadoutSlot, InExpectedLoadRevision);
	if (!EquipmentComponent
		|| !RuntimeAssetState
		|| !RuntimeAssetState->LoadedWeaponDefinition
		|| RuntimeAssetState->LoadedWeaponDefinition->WeaponTag != InWeaponTag)
	{
		return;
	}

	const bool bShouldAutoEquip = bEquipWhenReady
		&& MatchesPendingEquipWhenReadyRequest(InLoadoutSlot, InExpectedLoadRevision);

	ReleaseStreamableHandle(RuntimeAssetState->RuntimeAssetLoadHandle);
	// 运行时资源预热完成后，正式收尾分成三层：
	// 1. 先确保目标实例存在；
	// 2. 若命中挂起自动补装备请求，再请求 equipment 宿主写回当前生效装备态；
	// 3. 最后收 startup prewarm 和 UI 刷新。

	if (!EnsureLoadoutSlotWeaponInstance(InLoadoutSlot))
	{
		FString InstanceFailureReason = TEXT("运行时资源预热完成后，武器实例创建失败。");
		if (const UDataAsset_WeaponDefinition* LoadedWeaponDefinition = RuntimeAssetState->LoadedWeaponDefinition)
		{
			const FName SocketName = GetOwningHeroCharacter()
				? GetOwningHeroCharacter()->GetWeaponSocketBySubtypeTag(LoadedWeaponDefinition->GetWeaponSubtypeTag())
				: FName();
			if (SocketName.IsNone())
			{
				InstanceFailureReason = FString::Printf(
					TEXT("运行时资源预热完成后，武器实例创建失败：未找到武器小类别挂点。槽位=%s，WeaponTag=%s，WeaponSubtypeTag=%s"),
					*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
					*LoadedWeaponDefinition->WeaponTag.ToString(),
					*LoadedWeaponDefinition->GetWeaponSubtypeTag().ToString());
			}
		}

		if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
			LoadoutStateComponent && LoadoutStateComponent->IsLoadoutSlotStartupPrewarmPending(InLoadoutSlot))
		{
			LoadoutStateComponent->FailWeaponLoadoutStartup(
				InLoadoutSlot,
				InstanceFailureReason);
		}

		if (bShouldAutoEquip)
		{
			EquipmentComponent->BroadcastWeaponLoadoutEquipFailed(InLoadoutSlot);
		}

		EquipmentComponent->BroadcastLoadoutUIStateChanged();
		return;
	}

	if (bShouldAutoEquip)
	{
		// runtime 组件到这里最多只做到“目标定义、资源和实例都准备好”。
		// 真正切到手上的正式装备写回，仍统一走 equipment 宿主的入口。
		if (!EquipmentComponent->EquipResolvedLoadoutSlot(InLoadoutSlot, RuntimeAssetState->LoadedWeaponDefinition))
		{
			const FString EquipFailureReason = FString::Printf(
				TEXT("运行时资源预热完成后，出生槽正式装备失败。槽位=%s，WeaponTag=%s，WeaponSubtypeTag=%s"),
				*HeroLoadoutRuntimeLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
				*RuntimeAssetState->LoadedWeaponDefinition->WeaponTag.ToString(),
				*RuntimeAssetState->LoadedWeaponDefinition->GetWeaponSubtypeTag().ToString());

			if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
				LoadoutStateComponent && LoadoutStateComponent->IsLoadoutSlotStartupPrewarmPending(InLoadoutSlot))
			{
				LoadoutStateComponent->FailWeaponLoadoutStartup(
					InLoadoutSlot,
					EquipFailureReason);
			}

			EquipmentComponent->BroadcastWeaponLoadoutEquipFailed(InLoadoutSlot);
			EquipmentComponent->BroadcastLoadoutUIStateChanged();
			return;
		}
	}

	if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
		LoadoutStateComponent && LoadoutStateComponent->IsLoadoutSlotStartupPrewarmPending(InLoadoutSlot))
	{
		LoadoutStateComponent->MarkLoadoutSlotStartupPrewarmCompleted(InLoadoutSlot);
	}

	EquipmentComponent->BroadcastLoadoutUIStateChanged();
}

void UHeroLoadoutRuntimeComponent::ReleaseStreamableHandle(TSharedPtr<FStreamableHandle>& InOutHandle) const
{
	if (InOutHandle.IsValid())
	{
		// 统一释放入口，避免定义链和 runtime assets 链各自散落 handle 生命周期收尾。
		InOutHandle->ReleaseHandle();
		InOutHandle.Reset();
	}
}

AHeroWeaponBase* UHeroLoadoutRuntimeComponent::SpawnWeaponFromDefinition(UDataAsset_WeaponDefinition* InWeaponDefinition) const
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !InWeaponDefinition)
	{
		return nullptr;
	}

	const TSubclassOf<AHeroWeaponBase> WeaponClass = InWeaponDefinition->WeaponActorClass.Get();
	if (!WeaponClass)
	{
		UE_LOG(
			LogHeroLoadoutRuntimeComponent,
			Warning,
			TEXT("武器类未进入已预热状态或配置为空，已拒绝运行时同步生成。武器Tag=%s"),
			*InWeaponDefinition->WeaponTag.ToString());
		return nullptr;
	}

	AHeroWeaponBase* SpawnedWeapon = OwnerHeroCharacter->GetWorld()->SpawnActorDeferred<AHeroWeaponBase>(
		WeaponClass,
		FTransform(OwnerHeroCharacter->GetActorRotation(), OwnerHeroCharacter->GetActorLocation()),
		OwnerHeroCharacter,
		OwnerHeroCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SpawnedWeapon)
	{
		return nullptr;
	}

	// runtime 组件只负责生成一把待命武器 Actor。
	// 初始就落 holstered/hidden，真正 equipped / unequipped 表现切换继续交给后续装备链。
	SpawnedWeapon->SetWeaponDefinition(InWeaponDefinition);
	SpawnedWeapon->SetActorHiddenInGame(true);
	SpawnedWeapon->ApplyWeaponPresentationState(EActionWeaponPresentationState::Holstered);
	UGameplayStatics::FinishSpawningActor(
		SpawnedWeapon,
		FTransform(OwnerHeroCharacter->GetActorRotation(), OwnerHeroCharacter->GetActorLocation()));
	SpawnedWeapon->SetActorHiddenInGame(true);
	SpawnedWeapon->ApplyWeaponPresentationState(EActionWeaponPresentationState::Holstered);
	return SpawnedWeapon;
}
