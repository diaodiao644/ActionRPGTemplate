// 文件说明：实现英雄装备域启动/预热状态机与 UI 快照桥组件。

#include "Components/Equipment/HeroLoadoutStateComponent.h"

#include "Characters/ActionHeroCharacter.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroLoadoutStateComponent, Log, All);

namespace HeroEquipmentLoadoutStateLog
{
	static FString GetWeaponLoadoutSlotName(const EHeroWeaponLoadoutSlot InLoadoutSlot)
	{
		if (const UEnum* LoadoutSlotEnum = StaticEnum<EHeroWeaponLoadoutSlot>())
		{
			return LoadoutSlotEnum->GetNameStringByValue(static_cast<int64>(InLoadoutSlot));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InLoadoutSlot));
	}

	static bool ValidateFixedLoadoutRegistration(
		const TMap<EHeroWeaponLoadoutSlot, FHeroWeaponLoadoutRuntimeState>& InRuntimeStates,
		FString& OutFailureReason)
	{
		// startup 状态机只接受完整四固定槽。
		// 这里做的是启动前结构校验，不替代后续 definition/runtime ready 校验。
		const EHeroWeaponLoadoutSlot ExpectedSlots[] =
		{
			EHeroWeaponLoadoutSlot::Unarmed,
			EHeroWeaponLoadoutSlot::MeleeWeapon,
			EHeroWeaponLoadoutSlot::RangedWeapon,
			EHeroWeaponLoadoutSlot::HybridWeapon
		};

		TArray<FString> MissingSlotNames;
		for (const EHeroWeaponLoadoutSlot ExpectedSlot : ExpectedSlots)
		{
			if (!InRuntimeStates.Contains(ExpectedSlot))
			{
				MissingSlotNames.Add(GetWeaponLoadoutSlotName(ExpectedSlot));
			}
		}

		if (MissingSlotNames.Num() <= 0)
		{
			OutFailureReason.Reset();
			return true;
		}

		OutFailureReason = FString::Printf(
			TEXT("固定武器槽初始化不完整，缺少槽位=[%s]。当前装备组件只允许完整的四固定槽启动。"),
			*FString::Join(MissingSlotNames, TEXT(", ")));
		return false;
	}
}

UHeroLoadoutStateComponent::UHeroLoadoutStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHeroLoadoutStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRuntimeStateForHeroStartup();
	Super::EndPlay(EndPlayReason);
}

bool UHeroLoadoutStateComponent::InitializeWeaponLoadoutStartup(const EHeroWeaponLoadoutSlot InSpawnLoadoutSlot)
{
	// 初始化入口只把装备域带入 startup prewarm 状态机。
	// 真正的 definition 加载、runtime assets 预热和出生槽装备写回都由 runtime/equipment 宿主继续推进。
	return RestartWeaponLoadoutStartup(InSpawnLoadoutSlot, false);
}

bool UHeroLoadoutStateComponent::RetryWeaponLoadoutStartup()
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	if (!EquipmentComponent || !EquipmentComponent->bWeaponLoadoutInitialized)
	{
		// 重试只能复用已经注册完成的固定槽配置；如果装备域还没初始化，state 组件没有足够上下文重建 startup 链。
		UE_LOG(LogHeroLoadoutStateComponent, Warning, TEXT("固定武器槽尚未初始化，无法重试启动预热。"));
		return false;
	}

	// 重试入口只复用当前已经稳定注册下来的固定槽结构。
	// 它不是重新初始化装备域，也不会在这里重建四槽配置本身。
	return RestartWeaponLoadoutStartup(StartupRuntimeState.StartupSpawnLoadoutSlot, true);
}

bool UHeroLoadoutStateComponent::BuildLoadoutSlotUISnapshot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FHeroWeaponLoadoutSlotUISnapshot& OutSnapshot) const
{
	OutSnapshot = FHeroWeaponLoadoutSlotUISnapshot();
	OutSnapshot.LoadoutSlot = InLoadoutSlot;
	OutSnapshot.AllowedWeaponCategory = FHeroWeaponLoadoutDefinition::ResolveRequiredWeaponCategory(InLoadoutSlot);

	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
	const UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	if (!EquipmentComponent)
	{
		// UI 快照是只读聚合结果；装备宿主缺失时，只能返回“未注册”占位，不在这里尝试修复装备域状态。
		OutSnapshot.WeaponLabel = TEXT("未注册");
		return false;
	}

	OutSnapshot.bIsEquipped = EquipmentComponent->EquippedWeaponState.EquippedLoadoutSlot == InLoadoutSlot;

	const FHeroWeaponLoadoutRuntimeState* RuntimeState = EquipmentComponent->FindRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		// 槽位没有注册运行时状态时，UI 使用明确 fallback，避免调用方误以为只是资源未加载。
		OutSnapshot.WeaponLabel = TEXT("未注册");
		return false;
	}

	OutSnapshot.AllowedWeaponCategory = RuntimeState->LoadoutDefinition.AllowedWeaponCategory;
	OutSnapshot.bHasAssignedWeaponDefinition = RuntimeState->HasAssignedWeaponDefinition();

	if (!RuntimeState->HasAssignedWeaponDefinition())
	{
		// 槽位已注册但没有武器定义，属于合法的 UI 空槽状态。
		OutSnapshot.WeaponLabel = TEXT("未配置");
		return true;
	}

	if (const UDataAsset_WeaponDefinition* LoadedWeaponDefinition = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot)
		: nullptr)
	{
		// definition 已加载时，UI 快照从 Runtime 取武器定义，从 Context 取选中发射物标签。
		// 这里只聚合展示数据，不把 UI 选择反写成装备域正式状态。
		OutSnapshot.WeaponTag = LoadedWeaponDefinition->WeaponTag;
		OutSnapshot.WeaponPropertyType = LoadedWeaponDefinition->GetWeaponPropertyType();
		OutSnapshot.WeaponLabel = LoadedWeaponDefinition->ResolveDebugName();
		OutSnapshot.bSupportsProjectileSwitching = LoadedWeaponDefinition->SupportsProjectileSwitching();
		if (LoadoutContextComponent)
		{
			LoadoutContextComponent->GetLoadoutSlotSelectedProjectileConfigTag(
				InLoadoutSlot,
				OutSnapshot.SelectedProjectileConfigTag);
		}
		OutSnapshot.bRuntimeReady = LoadoutRuntimeComponent
			&& LoadoutRuntimeComponent->IsLoadoutSlotRuntimeReady(InLoadoutSlot);
		return true;
	}

	const FString FallbackAssetName = RuntimeState->LoadoutDefinition.DefaultWeaponDefinition.ToSoftObjectPath().GetAssetName();
	// 槽位有配置但 definition 尚未加载完成时，UI 保留资产名 fallback，方便玩家和调试层区分“未配置”和“加载中”。
	OutSnapshot.WeaponLabel = FallbackAssetName.IsEmpty() ? TEXT("已配置未加载") : FallbackAssetName;
	return true;
}

void UHeroLoadoutStateComponent::BuildEquipmentLoadoutUISnapshot(FHeroWeaponLoadoutUISnapshot& OutSnapshot) const
{
	// 根快照只是把 startup 状态、当前装备槽、特殊切武能量和四个槽位快照聚合给 UI。
	// 它不反向驱动装备、资源或上下文状态。
	OutSnapshot.CurrentEquippedLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;
	OutSnapshot.StartupState = GetWeaponLoadoutStartupState();
	OutSnapshot.StartupProgressRatio = GetWeaponLoadoutStartupProgressRatio();
	OutSnapshot.StartupPendingSlotCount = GetWeaponLoadoutStartupPendingSlotCount();
	OutSnapshot.StartupTotalSlotCount = GetWeaponLoadoutStartupTotalSlotCount();
	OutSnapshot.StartupFailureReason = GetWeaponLoadoutStartupFailureReason();
	OutSnapshot.bSpecialWeaponSwitchReady = false;

	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	if (EquipmentComponent)
	{
		OutSnapshot.CurrentEquippedLoadoutSlot = EquipmentComponent->GetCurrentEquippedLoadoutSlot();
		OutSnapshot.bSpecialWeaponSwitchReady = EquipmentComponent->IsSpecialWeaponSwitchReady();
	}

	static const EHeroWeaponLoadoutSlot FixedLoadoutSlots[] =
	{
		EHeroWeaponLoadoutSlot::Unarmed,
		EHeroWeaponLoadoutSlot::MeleeWeapon,
		EHeroWeaponLoadoutSlot::RangedWeapon,
		EHeroWeaponLoadoutSlot::HybridWeapon
	};

	OutSnapshot.LoadoutSlots.Reset();
	OutSnapshot.LoadoutSlots.Reserve(UE_ARRAY_COUNT(FixedLoadoutSlots));
	for (const EHeroWeaponLoadoutSlot LoadoutSlot : FixedLoadoutSlots)
	{
		FHeroWeaponLoadoutSlotUISnapshot SlotSnapshot;
		if (BuildLoadoutSlotUISnapshot(LoadoutSlot, SlotSnapshot))
		{
			OutSnapshot.LoadoutSlots.Add(SlotSnapshot);
			continue;
		}

		SlotSnapshot.LoadoutSlot = LoadoutSlot;
		SlotSnapshot.AllowedWeaponCategory = FHeroWeaponLoadoutDefinition::ResolveRequiredWeaponCategory(LoadoutSlot);
		SlotSnapshot.bIsEquipped = OutSnapshot.CurrentEquippedLoadoutSlot == LoadoutSlot;
		SlotSnapshot.WeaponLabel = TEXT("未配置");
		OutSnapshot.LoadoutSlots.Add(SlotSnapshot);
	}
}

float UHeroLoadoutStateComponent::GetWeaponLoadoutStartupProgressRatio() const
{
	if (StartupRuntimeState.TotalStartupPrewarmSlotCount <= 0)
	{
		// 没有 pending 计数时，Ready 视为 100%，其它状态视为 0%，给 UI 一个稳定的进度语义。
		return StartupRuntimeState.StartupState == EHeroWeaponLoadoutStartupState::Ready ? 1.f : 0.f;
	}

	// startup 进度只由 pending/total 槽位数推导，不读取 runtime 资源细节。
	const int32 CompletedSlotCount = FMath::Clamp(
		StartupRuntimeState.TotalStartupPrewarmSlotCount - StartupRuntimeState.PendingStartupPrewarmSlotCount,
		0,
		StartupRuntimeState.TotalStartupPrewarmSlotCount);
	return static_cast<float>(CompletedSlotCount) / static_cast<float>(StartupRuntimeState.TotalStartupPrewarmSlotCount);
}

bool UHeroLoadoutStateComponent::IsLoadoutSlotStartupPrewarmPending(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	// 这里只读 startup 正式状态源里的 pending 集合，不回头推断 runtime 资源细节。
	return StartupRuntimeState.PendingStartupPrewarmSlots.Contains(InLoadoutSlot);
}

void UHeroLoadoutStateComponent::HandleLoadoutSlotDefinitionChanged(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const bool bHasAssignedWeaponDefinition)
{
	if (StartupRuntimeState.StartupState != EHeroWeaponLoadoutStartupState::InProgress)
	{
		return;
	}

	// startup 进行中如果槽位定义发生变化，需要同步更新 pending prewarm 集合：
	// 新增定义要等待预热完成，移除定义则视为该槽位不再阻塞启动。
	if (bHasAssignedWeaponDefinition)
	{
		MarkLoadoutSlotStartupPrewarmPending(InLoadoutSlot);
		return;
	}

	MarkLoadoutSlotStartupPrewarmCompleted(InLoadoutSlot);
}

void UHeroLoadoutStateComponent::MarkLoadoutSlotStartupPrewarmCompleted(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	if (StartupRuntimeState.StartupState != EHeroWeaponLoadoutStartupState::InProgress)
	{
		return;
	}

	if (!StartupRuntimeState.PendingStartupPrewarmSlots.Remove(InLoadoutSlot))
	{
		return;
	}

	// 每个槽位只允许从 pending 集合移除一次；计数减少后再尝试整体 startup 收尾。
	StartupRuntimeState.PendingStartupPrewarmSlotCount =
		FMath::Max(StartupRuntimeState.PendingStartupPrewarmSlotCount - 1, 0);

	UE_LOG(
		LogHeroLoadoutStateComponent,
		Log,
		TEXT("固定武器槽启动预热完成。槽位=%s，剩余待完成槽位数=%d，详情=%s"),
		*HeroEquipmentLoadoutStateLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		StartupRuntimeState.PendingStartupPrewarmSlotCount,
		*BuildLoadoutSlotStartupDebugContext(InLoadoutSlot));

	BroadcastLoadoutUIStateChanged();
	TryFinishWeaponLoadoutStartup();
}

void UHeroLoadoutStateComponent::FailWeaponLoadoutStartup(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FString& InFailureReason)
{
	if (StartupRuntimeState.StartupState == EHeroWeaponLoadoutStartupState::Failed)
	{
		return;
	}

	// 失败态只记录 startup 状态和广播诊断，不回滚 runtime 已加载资源或 equipment 当前装备结果。
	StartupRuntimeState.StartupState = EHeroWeaponLoadoutStartupState::Failed;
	StartupRuntimeState.FailureReason = FString::Printf(
		TEXT("%s | %s"),
		*InFailureReason,
		*BuildLoadoutSlotStartupDebugContext(InLoadoutSlot));

	UE_LOG(
		LogHeroLoadoutStateComponent,
		Warning,
		TEXT("固定武器槽启动预热失败。槽位=%s，原因=%s"),
		*HeroEquipmentLoadoutStateLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		*StartupRuntimeState.FailureReason);

	BroadcastWeaponLoadoutStartupFailed(InLoadoutSlot, StartupRuntimeState.FailureReason);
	BroadcastLoadoutUIStateChanged();
}

void UHeroLoadoutStateComponent::TryFinishWeaponLoadoutStartup()
{
	if (StartupRuntimeState.StartupState != EHeroWeaponLoadoutStartupState::InProgress
		|| StartupRuntimeState.PendingStartupPrewarmSlotCount > 0)
	{
		return;
	}

	// 所有槽位预热完成后，还必须验证出生槽定义、runtime ready 和当前生效装备态都已经落地。
	// 只有这三层一致，startup 才能真正进入 Ready。
	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	const UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	if (!EquipmentComponent)
	{
		return;
	}

	const FHeroWeaponLoadoutRuntimeState* SpawnRuntimeState =
		EquipmentComponent->FindRuntimeState(StartupRuntimeState.StartupSpawnLoadoutSlot);
	const UDataAsset_WeaponDefinition* SpawnLoadedWeaponDefinition = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(StartupRuntimeState.StartupSpawnLoadoutSlot)
		: nullptr;
	if (!SpawnRuntimeState || !SpawnLoadedWeaponDefinition)
	{
		FailWeaponLoadoutStartup(
			StartupRuntimeState.StartupSpawnLoadoutSlot,
			TEXT("出生武器槽预热已结束，但未保留有效武器定义。"));
		return;
	}

	if (!LoadoutRuntimeComponent
		|| !LoadoutRuntimeComponent->IsLoadoutSlotRuntimeReady(StartupRuntimeState.StartupSpawnLoadoutSlot))
	{
		FailWeaponLoadoutStartup(
			StartupRuntimeState.StartupSpawnLoadoutSlot,
			TEXT("出生武器槽预热已结束，但运行时资源或实例尚未进入就绪状态。"));
		return;
	}

	if (EquipmentComponent->EquippedWeaponState.EquippedLoadoutSlot != StartupRuntimeState.StartupSpawnLoadoutSlot
		|| EquipmentComponent->EquippedWeaponState.EquippedWeaponDefinition != SpawnLoadedWeaponDefinition)
	{
		FailWeaponLoadoutStartup(
			StartupRuntimeState.StartupSpawnLoadoutSlot,
			TEXT("出生武器槽预热已结束，但当前装备状态没有正确落到出生槽。"));
		return;
	}

	StartupRuntimeState.StartupState = EHeroWeaponLoadoutStartupState::Ready;
	StartupRuntimeState.FailureReason.Reset();

	UE_LOG(
		LogHeroLoadoutStateComponent,
		Log,
		TEXT("固定武器槽启动预热全部完成，角色战斗资源已进入可操作状态。出生槽=%s"),
		*HeroEquipmentLoadoutStateLog::GetWeaponLoadoutSlotName(StartupRuntimeState.StartupSpawnLoadoutSlot));

	BroadcastWeaponLoadoutStartupReady();
	BroadcastLoadoutUIStateChanged();
}

void UHeroLoadoutStateComponent::BroadcastLoadoutUIStateChanged() const
{
	// 这只是 UI 快照需要重拉的广播，不代表装备状态、资源状态或上下文状态一定发生了业务变化。
	LoadoutUIStateChangedDelegate.Broadcast();
}

void UHeroLoadoutStateComponent::ResetRuntimeStateForHeroStartup()
{
	// 组件退场或重新启动时，清掉 startup 状态和依赖组件缓存。
	// 真正的资源句柄、实例和装备快照仍由各自宿主负责收尾。
	StartupRuntimeState.ResetToNone();
	CachedHeroCharacter.Reset();
	CachedHeroEquipmentComponent.Reset();
	CachedHeroLoadoutContextComponent.Reset();
	CachedHeroLoadoutRuntimeComponent.Reset();
}

AActionHeroCharacter* UHeroLoadoutStateComponent::GetOwningHeroCharacter() const
{
	if (CachedHeroCharacter.IsValid())
	{
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedHeroCharacter.Get();
}

UHeroEquipmentComponent* UHeroLoadoutStateComponent::GetOwningHeroEquipmentComponent() const
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

UHeroLoadoutContextComponent* UHeroLoadoutStateComponent::GetOwningHeroLoadoutContextComponent() const
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

UHeroLoadoutRuntimeComponent* UHeroLoadoutStateComponent::GetOwningHeroLoadoutRuntimeComponent() const
{
	if (CachedHeroLoadoutRuntimeComponent.IsValid())
	{
		return CachedHeroLoadoutRuntimeComponent.Get();
	}

	CachedHeroLoadoutRuntimeComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UHeroLoadoutRuntimeComponent>()
		: nullptr;

	return CachedHeroLoadoutRuntimeComponent.Get();
}

void UHeroLoadoutStateComponent::ResetWeaponLoadoutStartupState(const EHeroWeaponLoadoutSlot InSpawnLoadoutSlot)
{
	// startup 重置只影响本组件持有的启动状态机，不直接释放 runtime 资源。
	StartupRuntimeState.Reset(InSpawnLoadoutSlot);

	if (UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent())
	{
		LoadoutRuntimeComponent->ClearPendingEquipWhenReadyRequest();
	}
}

void UHeroLoadoutStateComponent::MarkLoadoutSlotStartupPrewarmPending(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	if (StartupRuntimeState.PendingStartupPrewarmSlots.Contains(InLoadoutSlot))
	{
		return;
	}

	// pending 集合和计数必须同步维护，UI 进度和 TryFinishWeaponLoadoutStartup 都依赖这两份状态。
	StartupRuntimeState.PendingStartupPrewarmSlots.Add(InLoadoutSlot);
	++StartupRuntimeState.PendingStartupPrewarmSlotCount;
	++StartupRuntimeState.TotalStartupPrewarmSlotCount;
}

bool UHeroLoadoutStateComponent::RestartWeaponLoadoutStartup(
	const EHeroWeaponLoadoutSlot InSpawnLoadoutSlot,
	const bool bReleaseExistingAsyncHandles)
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	if (!EquipmentComponent)
	{
		return false;
	}

	UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	if (!LoadoutRuntimeComponent)
	{
		// startup 状态机本身仍归 state 组件收口，所以 runtime 宿主缺失时先把本地状态机切干净，
		// 再统一走 failed 入口广播诊断，而不是在这里半途遗留旧 startup 状态。
		ResetWeaponLoadoutStartupState(InSpawnLoadoutSlot);
		FailWeaponLoadoutStartup(InSpawnLoadoutSlot, TEXT("装备域 runtime 组件无效，无法推进启动预热。"));
		return false;
	}

	ResetWeaponLoadoutStartupState(InSpawnLoadoutSlot);

	FString FixedLoadoutRegistrationFailureReason;
	// startup 只接受完整四固定槽配置。缺槽时直接失败，避免 UI/输入侧进入半初始化装备域。
	if (!HeroEquipmentLoadoutStateLog::ValidateFixedLoadoutRegistration(
		EquipmentComponent->LoadoutRuntimeStatesBySlot,
		FixedLoadoutRegistrationFailureReason))
	{
		FailWeaponLoadoutStartup(InSpawnLoadoutSlot, FixedLoadoutRegistrationFailureReason);
		return false;
	}

	if (!EquipmentComponent->HasWeaponAssignedToLoadoutSlot(InSpawnLoadoutSlot))
	{
		FailWeaponLoadoutStartup(
			InSpawnLoadoutSlot,
			FString::Printf(TEXT("出生武器槽未配置武器定义。槽位=%d"), static_cast<int32>(InSpawnLoadoutSlot)));
		return false;
	}

	TArray<EHeroWeaponLoadoutSlot> StartupPrewarmSlots;
	for (TPair<EHeroWeaponLoadoutSlot, FHeroWeaponLoadoutRuntimeState>& RuntimePair :
		EquipmentComponent->LoadoutRuntimeStatesBySlot)
	{
		if (!RuntimePair.Value.HasAssignedWeaponDefinition())
		{
			continue;
		}

		if (bReleaseExistingAsyncHandles)
		{
			// 重试时先让旧异步链失效，再重新标记 pending 和发起 definition 加载。
			LoadoutRuntimeComponent->CancelLoadoutSlotAsyncRequestsAndAdvanceRevision(RuntimePair.Key);
		}

		MarkLoadoutSlotStartupPrewarmPending(RuntimePair.Key);
		StartupPrewarmSlots.Add(RuntimePair.Key);
	}

	if (StartupPrewarmSlots.Num() <= 0)
	{
		FailWeaponLoadoutStartup(InSpawnLoadoutSlot, TEXT("未找到任何可参与启动预热的固定武器槽。"));
		return false;
	}

	// startup 调度阶段先广播一次 UI 脏标记，让外部看到 pending 集合和进度已经切入新一轮状态。
	BroadcastLoadoutUIStateChanged();

	for (const EHeroWeaponLoadoutSlot StartupSlot : StartupPrewarmSlots)
	{
		const FHeroWeaponLoadoutRuntimeState* RuntimeState = EquipmentComponent->FindRuntimeState(StartupSlot);
		if (!RuntimeState)
		{
			continue;
		}

		const bool bEquipWhenReady = (StartupSlot == InSpawnLoadoutSlot);
		const int32 LoadRevision = LoadoutRuntimeComponent->GetLoadoutSlotLoadRevision(StartupSlot);
		if (bEquipWhenReady)
		{
			// 出生槽需要在资源就绪后自动补装备；其它槽位只参与预热，不写当前生效装备态。
			// PendingEquipWhenReady 的正式宿主仍在 runtime 组件，state 这里只桥接 startup 期意图。
			LoadoutRuntimeComponent->SetPendingEquipWhenReadyRequest(StartupSlot, LoadRevision);
		}

		// state 组件到这里结束自身职责，后续 definition 加载、runtime assets 预热和实例准备都交给 runtime 宿主推进。
		LoadoutRuntimeComponent->RequestLoadoutSlotDefinitionAsync(
			StartupSlot,
			bEquipWhenReady,
			LoadRevision);
	}

	return true;
}

FString UHeroLoadoutStateComponent::BuildLoadoutSlotStartupDebugContext(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	// 这段只拼接 startup 失败诊断上下文，帮助日志定位槽位、revision、软引用和 runtime assets 状态。
	// 它不参与任何启动成功/失败判定。
	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	if (!EquipmentComponent)
	{
		return TEXT("装备组件无效");
	}

	const FHeroWeaponLoadoutRuntimeState* RuntimeState = EquipmentComponent->FindRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return FString::Printf(TEXT("槽位=%s 未注册"), *HeroEquipmentLoadoutStateLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
	}

	const FString DefinitionAssetPath = RuntimeState->LoadoutDefinition.DefaultWeaponDefinition.IsNull()
		? TEXT("None")
		: RuntimeState->LoadoutDefinition.DefaultWeaponDefinition.ToSoftObjectPath().ToString();
	const UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	const UDataAsset_WeaponDefinition* LoadedWeaponDefinition = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot)
		: nullptr;
	const FString LoadedWeaponTag = LoadedWeaponDefinition
		? LoadedWeaponDefinition->WeaponTag.ToString()
		: TEXT("None");
	const FString RuntimeAssetReadiness = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->DescribeWeaponRuntimeAssetReadiness(LoadedWeaponDefinition)
		: TEXT("运行时组件无效");
	const int32 LoadRevision = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->GetLoadoutSlotLoadRevision(InLoadoutSlot)
		: INDEX_NONE;

	return FString::Printf(
		TEXT("槽位=%s LoadRevision=%d 软引用=%s 已加载武器Tag=%s 运行时资源=%s"),
		*HeroEquipmentLoadoutStateLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		LoadRevision,
		*DefinitionAssetPath,
		*LoadedWeaponTag,
		*RuntimeAssetReadiness);
}

void UHeroLoadoutStateComponent::BroadcastWeaponLoadoutStartupReady() const
{
	// 这里只广播“startup 链已 Ready”，不在广播口追加 runtime/equipment 写回。
	WeaponLoadoutStartupReadyDelegate.Broadcast();
}

void UHeroLoadoutStateComponent::BroadcastWeaponLoadoutStartupFailed(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FString& InFailureReason) const
{
	// 失败广播只同步槽位和诊断文本，外部若要重试或回收资源，仍需走各自正式入口。
	WeaponLoadoutStartupFailedDelegate.Broadcast(InLoadoutSlot, InFailureReason);
}
