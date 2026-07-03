// 文件说明：实现英雄装备组件的固定武器槽、异步加载、预实例化与能量逻辑。

#include "Components/Equipment/HeroEquipmentComponent.h"

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_SpiritSkill.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Animation/AnimMontage.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Character/HeroAssemblyComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Equipment/HeroLoadoutStateComponent.h"
#include "Components/Equipment/HeroLoadoutEffectComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Items/Weapons/HeroWeaponBase.h"
#include "System/ActionAssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroEquipmentComponent, Log, All);

namespace HeroEquipmentComponentLog
{
	static FString GetWeaponCategoryName(const EHeroWeaponCategory InWeaponCategory)
	{
		if (const UEnum* WeaponCategoryEnum = StaticEnum<EHeroWeaponCategory>())
		{
			return WeaponCategoryEnum->GetNameStringByValue(static_cast<int64>(InWeaponCategory));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InWeaponCategory));
	}

	static FString GetWeaponLoadoutSlotName(const EHeroWeaponLoadoutSlot InLoadoutSlot)
	{
		if (const UEnum* LoadoutSlotEnum = StaticEnum<EHeroWeaponLoadoutSlot>())
		{
			return LoadoutSlotEnum->GetNameStringByValue(static_cast<int64>(InLoadoutSlot));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InLoadoutSlot));
	}

	static FString GetWeaponPropertyTypeName(const EActionWeaponPropertyType InWeaponPropertyType)
	{
		if (const UEnum* WeaponPropertyTypeEnum = StaticEnum<EActionWeaponPropertyType>())
		{
			return WeaponPropertyTypeEnum->GetNameStringByValue(static_cast<int64>(InWeaponPropertyType));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InWeaponPropertyType));
	}

	static bool ValidateFixedLoadoutRegistration(
		const TMap<EHeroWeaponLoadoutSlot, FHeroWeaponLoadoutRuntimeState>& InRuntimeStates,
		FString& OutFailureReason)
	{
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

	static FString GetExternalHitEffectApplyFailureReasonName(
		const EHeroExternalHitEffectSourceApplyFailureReason InFailureReason)
	{
		if (const UEnum* FailureReasonEnum = StaticEnum<EHeroExternalHitEffectSourceApplyFailureReason>())
		{
			return FailureReasonEnum->GetNameStringByValue(static_cast<int64>(InFailureReason));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InFailureReason));
	}

	static void SetExternalHitEffectSourceApplyResult(
		FHeroExternalHitEffectSourceApplyResult& OutResult,
		const bool bInSuccess,
		const EHeroWeaponLoadoutSlot InResolvedLoadoutSlot,
		const EHeroExternalHitEffectSourceApplyFailureReason InFailureReason)
	{
		OutResult.bSuccess = bInSuccess;
		OutResult.ResolvedLoadoutSlot = InResolvedLoadoutSlot;
		OutResult.FailureReason = InFailureReason;
	}

	static void SetExternalAdditionalHitEffectsApplyResult(
		FHeroExternalAdditionalHitEffectsApplyResult& OutResult,
		const bool bInSuccess,
		const EHeroWeaponLoadoutSlot InResolvedLoadoutSlot,
		const EHeroExternalHitEffectSourceApplyFailureReason InFailureReason)
	{
		OutResult.bSuccess = bInSuccess;
		OutResult.ResolvedLoadoutSlot = InResolvedLoadoutSlot;
		OutResult.FailureReason = InFailureReason;
	}
}

namespace
{
	static UHeroLoadoutEffectComponent* GetHitEffectComponent(UHeroEquipmentComponent* InEquipmentComponent)
	{
		if (!InEquipmentComponent)
		{
			return nullptr;
		}

		return InEquipmentComponent->GetOwner()
			? InEquipmentComponent->GetOwner()->FindComponentByClass<UHeroLoadoutEffectComponent>()
			: nullptr;
	}

	static const UHeroLoadoutEffectComponent* GetHitEffectComponent(const UHeroEquipmentComponent* InEquipmentComponent)
	{
		if (!InEquipmentComponent)
		{
			return nullptr;
		}

		return InEquipmentComponent->GetOwner()
			? InEquipmentComponent->GetOwner()->FindComponentByClass<UHeroLoadoutEffectComponent>()
			: nullptr;
	}

	static UHeroLoadoutStateComponent* GetLoadoutStateComponent(UHeroEquipmentComponent* InEquipmentComponent)
	{
		if (!InEquipmentComponent)
		{
			return nullptr;
		}

		return InEquipmentComponent->GetOwner()
			? InEquipmentComponent->GetOwner()->FindComponentByClass<UHeroLoadoutStateComponent>()
			: nullptr;
	}

	static const UHeroLoadoutStateComponent* GetLoadoutStateComponent(const UHeroEquipmentComponent* InEquipmentComponent)
	{
		if (!InEquipmentComponent)
		{
			return nullptr;
		}

		return InEquipmentComponent->GetOwner()
			? InEquipmentComponent->GetOwner()->FindComponentByClass<UHeroLoadoutStateComponent>()
			: nullptr;
	}

}

UHeroEquipmentComponent::UHeroEquipmentComponent()
	: Super()
{
}

void UHeroEquipmentComponent::InitializeWeaponLoadout(const TArray<FHeroWeaponLoadoutDefinition>& InWeaponLoadoutDefinitions)
{
	// 装备组件初始化只允许成功跑一次。
	// 运行时如果重复初始化，会直接打断已有槽位、实例和当前装备态。
	if (bWeaponLoadoutInitialized)
	{
		return;
	}

	LoadoutRuntimeStatesBySlot.Reset();
	EHeroWeaponLoadoutSlot SpawnLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;
	bool bHasExplicitSpawnLoadoutSlot = false;

	for (const FHeroWeaponLoadoutDefinition& WeaponLoadoutDefinition : InWeaponLoadoutDefinitions)
	{
		if (!RegisterWeaponLoadoutDefinition(WeaponLoadoutDefinition))
		{
			continue;
		}


		if (!bHasExplicitSpawnLoadoutSlot && WeaponLoadoutDefinition.bEquipOnSpawn)
		{
			SpawnLoadoutSlot = WeaponLoadoutDefinition.LoadoutSlot;
			bHasExplicitSpawnLoadoutSlot = true;
		}
	}

	bWeaponLoadoutInitialized = true;

	// 当前版本的槽位 GA 改为常驻授予：
	// 初始化时一次性授予，后续切槽只切当前装备态，不再反复移除和重授。
	// 这样固定槽输入与槽位专属 GA 的存在性保持稳定，切武时只需要刷新当前生效快照。
	for (const TPair<EHeroWeaponLoadoutSlot, FHeroWeaponLoadoutRuntimeState>& ResidentPair : LoadoutRuntimeStatesBySlot)
	{
		GrantLoadoutAbilities(ResidentPair.Key);
	}

	if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetLoadoutStateComponent(this))
	{
		// startup prewarm 正式状态机已经迁到 state 组件；
		// equipment 在初始化末尾只负责把出生槽选择桥接过去，不自己持有 startup 状态。
		LoadoutStateComponent->InitializeWeaponLoadoutStartup(SpawnLoadoutSlot);
	}
}

bool UHeroEquipmentComponent::RegisterWeaponLoadoutDefinition(const FHeroWeaponLoadoutDefinition& InWeaponLoadoutDefinition)
{
	if (!InWeaponLoadoutDefinition.IsValidDefinition())
	{
		UE_LOG(LogHeroEquipmentComponent, Warning, TEXT("固定武器槽定义非法，注册失败。槽位=%d，允许类别=%d"),
			static_cast<int32>(InWeaponLoadoutDefinition.LoadoutSlot),
			static_cast<int32>(InWeaponLoadoutDefinition.AllowedWeaponCategory));
		return false;
	}

	FHeroWeaponLoadoutRuntimeState& RuntimeState = LoadoutRuntimeStatesBySlot.FindOrAdd(InWeaponLoadoutDefinition.LoadoutSlot);
	RuntimeState.LoadoutDefinition = InWeaponLoadoutDefinition;
	// 这里只保证装备域宿主和各子组件都为这个固定槽位准备好了最小运行时骨架。
	// 真正的定义加载、资源预热与实例创建仍要等后续设置武器定义或正式装备时再推进。
	if (UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent())
	{
		LoadoutRuntimeComponent->EnsureLoadoutSlotRuntimeState(InWeaponLoadoutDefinition.LoadoutSlot);
	}
	if (UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent())
	{
		LoadoutContextComponent->EnsureLoadoutSlotContextState(InWeaponLoadoutDefinition.LoadoutSlot);
	}
	return true;
}

bool UHeroEquipmentComponent::SetWeaponDefinitionForLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const TSoftObjectPtr<UDataAsset_WeaponDefinition>& InWeaponDefinition)
{
	FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		UE_LOG(LogHeroEquipmentComponent, Warning, TEXT("目标固定武器槽未注册，无法设置武器定义。槽位=%d"), static_cast<int32>(InLoadoutSlot));
		return false;
	}

	if (RuntimeState->LoadoutDefinition.DefaultWeaponDefinition == InWeaponDefinition)
	{
		// 同一份武器资产重复设置视为无变化，避免无意义推进版本号并重走异步加载链。
		return true;
	}

	if (InLoadoutSlot == EHeroWeaponLoadoutSlot::Unarmed && InWeaponDefinition.IsNull())
	{
		UE_LOG(LogHeroEquipmentComponent, Warning, TEXT("空手槽必须始终配置武器定义，拒绝卸下空手槽。"));
		return false;
	}

	if (const UDataAsset_WeaponDefinition* ResolvedWeaponDefinition = InWeaponDefinition.Get())
	{
		if (!DoesWeaponDefinitionMatchLoadoutSlot(ResolvedWeaponDefinition, InLoadoutSlot))
		{
			UE_LOG(LogHeroEquipmentComponent, Warning, TEXT("武器类别与固定武器槽不匹配，拒绝设置。槽位=%d，武器Tag=%s"),
				static_cast<int32>(InLoadoutSlot),
				*ResolvedWeaponDefinition->WeaponTag.ToString());
			return false;
		}
	}

	if (!InWeaponDefinition.IsNull()
		&& RuntimeState->HasAssignedWeaponDefinition()
		&& RuntimeState->LoadoutDefinition.DefaultWeaponDefinition != InWeaponDefinition)
	{
		// 当前模型里，“给同一槽位换另一把武器”必须先显式卸下旧定义，
		// 避免旧资源链、旧实例和旧附加能力还没收口时直接被新定义覆盖。
		UE_LOG(LogHeroEquipmentComponent, Warning, TEXT("固定武器槽已存在武器，必须先卸下当前武器后才能装备新武器。槽位=%d"),
			static_cast<int32>(InLoadoutSlot));
		return false;
	}

	UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	const int32 PreviousLoadRevision = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->GetLoadoutSlotLoadRevision(InLoadoutSlot)
		: INDEX_NONE;
	const bool bHadPendingEquipWhenReady = LoadoutRuntimeComponent
		&& LoadoutRuntimeComponent->MatchesPendingEquipWhenReadyRequest(InLoadoutSlot, PreviousLoadRevision);
	const int32 NewLoadRevision = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->AdvanceLoadoutSlotLoadRevision(InLoadoutSlot)
		: INDEX_NONE;
	RuntimeState->LoadoutDefinition.DefaultWeaponDefinition = InWeaponDefinition;
	if (UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent())
	{
		LoadoutContextComponent->ClearLoadoutSlotSelectedProjectileConfigTag(InLoadoutSlot);
	}

	if (InWeaponDefinition.IsNull())
	{
		if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetLoadoutStateComponent(this);
			LoadoutStateComponent && LoadoutStateComponent->IsLoadoutSlotStartupPrewarmPending(InLoadoutSlot))
		{
			LoadoutStateComponent->MarkLoadoutSlotStartupPrewarmCompleted(InLoadoutSlot);
		}

		// 当槽位被卸下武器时，立即清理它已经缓存的实例与已加载定义。
		// 这是“槽位配置变更”的收尾，不等价于当前装备态一定已经切走；
		// 如果卸下的刚好是手上这把武器，下面会再显式回退到空手槽。
		if (LoadoutRuntimeComponent)
		{
			LoadoutRuntimeComponent->ReleaseLoadoutSlotRuntimeResources(InLoadoutSlot, true, true);
		}

		if (EquippedWeaponState.EquippedLoadoutSlot == InLoadoutSlot)
		{
			return EquipWeaponByLoadoutSlot(EHeroWeaponLoadoutSlot::Unarmed);
		}

		if (bHadPendingEquipWhenReady)
		{
			BroadcastWeaponLoadoutEquipFailed(InLoadoutSlot);
		}

		BroadcastLoadoutUIStateChanged();
		return true;
	}

	if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetLoadoutStateComponent(this))
	{
		LoadoutStateComponent->HandleLoadoutSlotDefinitionChanged(InLoadoutSlot, true);
	}

	// 新武器一旦被装进槽位，就立刻启动加载、预热和预实例化流程。
	// 如果当前手上正好就是这个槽位，则等资源就绪后继续完成这次“同槽位武器刷新”的手动装备流程。
	// 也就是说“槽位里有什么武器”和“当前手上拿的是哪把武器”是两层状态，
	// 前者在这里改配置，后者要等正式 Equip 链命中后才写回 EquippedWeaponState。
	const bool bShouldAutoEquipWhenReady = bHadPendingEquipWhenReady
		|| EquippedWeaponState.EquippedLoadoutSlot == InLoadoutSlot;
	if (bShouldAutoEquipWhenReady && LoadoutRuntimeComponent)
	{
		LoadoutRuntimeComponent->SetPendingEquipWhenReadyRequest(InLoadoutSlot, NewLoadRevision);
	}
	if (LoadoutRuntimeComponent)
	{
		LoadoutRuntimeComponent->RequestLoadoutSlotDefinitionAsync(InLoadoutSlot, bShouldAutoEquipWhenReady, NewLoadRevision);
	}
	BroadcastLoadoutUIStateChanged();
	return true;
}

bool UHeroEquipmentComponent::EquipWeaponByLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return false;
	}

	UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	const int32 CurrentLoadRevision = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->GetLoadoutSlotLoadRevision(InLoadoutSlot)
		: INDEX_NONE;
	if (LoadoutRuntimeComponent)
	{
		LoadoutRuntimeComponent->SetPendingEquipWhenReadyRequest(InLoadoutSlot, CurrentLoadRevision);
	}

	const UDataAsset_WeaponDefinition* LoadedWeaponDefinition = LoadoutRuntimeComponent
		? LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot)
		: nullptr;
	const bool bLoadedWeaponRequiresRuntimeInstance =
		LoadedWeaponDefinition
		&& ((LoadedWeaponDefinition->GetWeaponCategory() != EHeroWeaponCategory::Unarmed)
			|| !LoadedWeaponDefinition->WeaponActorClass.IsNull());
	const bool bAlreadyEquippedRequestedWeapon =
		(EquippedWeaponState.EquippedLoadoutSlot == InLoadoutSlot)
		&& (LoadedWeaponDefinition
			// 这里不能只比较 WeaponTag。
			// 同一槽位在重载、重建实例或资源刷新期间，可能出现“Tag 相同但 Definition / Instance 不同”的中间状态。
			// 如果只按 Tag 判等，会把尚未真正完成写回的状态误判成“已经装备完成”。
			&& EquippedWeaponState.EquippedWeaponTag == LoadedWeaponDefinition->WeaponTag
			&& EquippedWeaponState.EquippedWeaponDefinition == LoadedWeaponDefinition
			&& (!bLoadedWeaponRequiresRuntimeInstance
				|| (LoadoutRuntimeComponent
					&& LoadoutRuntimeComponent->HasSpawnedWeaponInstanceByLoadoutSlot(InLoadoutSlot)
					&& EquippedWeaponState.EquippedWeaponInstance == LoadoutRuntimeComponent->GetSpawnedWeaponInstanceByLoadoutSlot(InLoadoutSlot))));

	if (bAlreadyEquippedRequestedWeapon)
	{
		// 目标槽位、定义和实例都已经与当前生效快照一致时，直接视为同步命中成功。
		return true;
	}

	if (!RuntimeState->HasAssignedWeaponDefinition())
	{
		// 当前槽位根本没有配置武器定义时，不存在后续异步链可继续完成这次装备请求。
		return false;
	}

	// 玩家已经明确手动请求切到这个槽位；如果资源还没准备好，就把请求暂存到异步链结束时再补完成。
	if (!LoadedWeaponDefinition)
	{
		if (LoadoutRuntimeComponent)
		{
			LoadoutRuntimeComponent->RequestLoadoutSlotDefinitionAsync(InLoadoutSlot, true, CurrentLoadRevision);
		}
		// 这里表示“切武请求已被接受，但需要等定义加载完成后再真正落装备态”。
		return true;
	}

	if (!UActionAssetManager::Get().AreWeaponRuntimeAssetsReady(LoadedWeaponDefinition))
	{
		if (LoadoutRuntimeComponent)
		{
			LoadoutRuntimeComponent->RequestLoadoutSlotRuntimeAssetsAsync(
				InLoadoutSlot,
				const_cast<UDataAsset_WeaponDefinition*>(LoadedWeaponDefinition),
				true,
				CurrentLoadRevision);
		}
		// 这里表示“切武请求已被接受，但需要等运行时资源预热完成后再真正落装备态”。
		return true;
	}

	if (!LoadoutRuntimeComponent || !LoadoutRuntimeComponent->EnsureLoadoutSlotWeaponInstance(InLoadoutSlot))
	{
		return false;
	}

	// 只有目标定义、运行时资源和目标实例都已经就绪后，
	// 才允许进入真正写回 EquippedWeaponState 的正式装备阶段。
	return EquipResolvedLoadoutSlot(InLoadoutSlot, const_cast<UDataAsset_WeaponDefinition*>(LoadedWeaponDefinition));
}

bool UHeroEquipmentComponent::HasWeaponAssignedToLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	// 这里只读槽位配置层是否已经挂了定义，
	// 不代表 definition 已加载、runtime 已 ready，或当前手上已经正式装备这个槽位。
	if (const FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot))
	{
		return RuntimeState->HasAssignedWeaponDefinition();
	}

	return false;
}

FHeroEquippedWeaponState UHeroEquipmentComponent::GetCurrentEquippedWeaponState() const
{
	// 对外正式当前装备态只认这一份已生效快照；
	// 外部不应再自己回拼槽位配置、runtime 资源链或实例链来推断当前装备结果。
	return EquippedWeaponState;
}

bool UHeroEquipmentComponent::GetWeaponLoadoutDefinition(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FHeroWeaponLoadoutDefinition& OutLoadoutDefinition) const
{
	// 这里只读固定槽配置模板，不把“槽位里配置了什么”混同成“当前正式装备了什么”。
	if (const FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot))
	{
		OutLoadoutDefinition = RuntimeState->LoadoutDefinition;
		return true;
	}

	return false;
}

bool UHeroEquipmentComponent::IsSpecialWeaponSwitchReady() const
{
	// 特殊切武能量只在这里作为高层可用性查询口暴露，
	// 不扩成另一套切武事务态或表现态状态源。
	if (const UActionAttributeSetBase* AttributeSet = GetOwningAttributeSet())
	{
		return AttributeSet->IsSpecialWeaponSwitchEnergyFull();
	}

	return false;
}

UAnimMontage* UHeroEquipmentComponent::GetSpecialWeaponSwitchMontageForLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (const UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent())
	{
		if (const UDataAsset_WeaponDefinition* WeaponDefinition =
			LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot))
		{
			return WeaponDefinition->GetSpecialWeaponSwitchMontage();
		}
	}

	return nullptr;
}

UAnimMontage* UHeroEquipmentComponent::GetNormalWeaponSwitchMontageForLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (const UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent())
	{
		if (const UDataAsset_WeaponDefinition* WeaponDefinition =
			LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot))
		{
			return WeaponDefinition->GetNormalWeaponSwitchMontage();
		}
	}

	return nullptr;
}

bool UHeroEquipmentComponent::ConsumeSpecialWeaponSwitchEnergy()
{
	if (UActionAttributeSetBase* AttributeSet = GetOwningAttributeSet())
	{
		if (!AttributeSet->IsSpecialWeaponSwitchEnergyFull())
		{
			return false;
		}

		// energy 写入口只负责扣当前高层资源，并通知 UI 重新拉快照；
		// 真正何时允许切武、是否进入事务态，仍由外层正式切武链裁决。
		AttributeSet->ConsumeSpecialWeaponSwitchEnergyValue(AttributeSet->GetMaxSpecialWeaponSwitchEnergy());
		BroadcastLoadoutUIStateChanged();
		return true;
	}

	return false;
}

void UHeroEquipmentComponent::AddSpecialWeaponSwitchEnergy(const float DeltaEnergy)
{
	if (DeltaEnergy <= 0.f)
	{
		return;
	}

	if (UActionAttributeSetBase* AttributeSet = GetOwningAttributeSet())
	{
		// 这里只增加高层切武能量镜像，并通过 UI 广播让外部刷新展示。
		AttributeSet->AddSpecialWeaponSwitchEnergyValue(DeltaEnergy);
		BroadcastLoadoutUIStateChanged();
	}
}

void UHeroEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<EHeroWeaponLoadoutSlot> RegisteredLoadoutSlots;
	LoadoutRuntimeStatesBySlot.GetKeys(RegisteredLoadoutSlots);

	// 槽位 GA 已经改为常驻授予，因此组件结束时需要显式回收能力。
	// 资源句柄、运行时实例和挂起自动装备请求，则统一交给独立 runtime 组件收口。
	// 这里要把“当前装备快照 + 常驻槽位能力 + 外部效果运行时”一起收掉，
	// 避免晚到的异步回调或外部系统继续消费一个即将销毁的旧装备上下文。
	for (const EHeroWeaponLoadoutSlot LoadoutSlot : RegisteredLoadoutSlots)
	{
		RemoveLoadoutAbilities(LoadoutSlot);
		if (UHeroLoadoutEffectComponent* HitEffectComponent = GetHitEffectComponent(this))
		{
			HitEffectComponent->ResetLoadoutSlotHitEffectRuntime(LoadoutSlot);
		}
	}

	if (UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent())
	{
		LoadoutRuntimeComponent->ResetRuntimeStateForEndPlay();
	}
	if (UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent())
	{
		LoadoutContextComponent->ResetRuntimeStateForHeroStartup();
	}

	LoadoutRuntimeStatesBySlot.Reset();
	EquippedWeaponState.ResetToUnarmed();
	bWeaponLoadoutInitialized = false;
	CachedOwnerHeroCharacter.Reset();

	if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetLoadoutStateComponent(this))
	{
		LoadoutStateComponent->ResetRuntimeStateForHeroStartup();
	}

	Super::EndPlay(EndPlayReason);
}

AActionHeroCharacter* UHeroEquipmentComponent::GetOwningHeroCharacter() const
{
	if (CachedOwnerHeroCharacter.IsValid())
	{
		return CachedOwnerHeroCharacter.Get();
	}

	CachedOwnerHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedOwnerHeroCharacter.Get();
}

UActionAbilitySystemComponent* UHeroEquipmentComponent::GetOwningActionAbilitySystemComponent() const
{
	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		return OwnerHeroCharacter->GetActionAbilitySystemComponent();
	}

	return nullptr;
}

UActionAttributeSetBase* UHeroEquipmentComponent::GetOwningAttributeSet() const
{
	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		return OwnerHeroCharacter->GetActionAttributeSet();
	}

	return nullptr;
}

UHeroLoadoutRuntimeComponent* UHeroEquipmentComponent::GetOwningHeroLoadoutRuntimeComponent() const
{
	// runtime 资源链 / 实例链已拆到独立宿主；
	// equipment 这里只按需桥接访问，不再并行缓存第二份正式资源状态。
	return GetOwner()
		? GetOwner()->FindComponentByClass<UHeroLoadoutRuntimeComponent>()
		: nullptr;
}

UHeroLoadoutContextComponent* UHeroEquipmentComponent::GetOwningHeroLoadoutContextComponent() const
{
	// context 属性缓存与发射物标签状态由独立宿主持有，
	// equipment 这里只在槽位配置变化时桥接通知。
	return GetOwner()
		? GetOwner()->FindComponentByClass<UHeroLoadoutContextComponent>()
		: nullptr;
}

FHeroWeaponLoadoutRuntimeState* UHeroEquipmentComponent::FindRuntimeState(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	return LoadoutRuntimeStatesBySlot.Find(InLoadoutSlot);
}

const FHeroWeaponLoadoutRuntimeState* UHeroEquipmentComponent::FindRuntimeState(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	return LoadoutRuntimeStatesBySlot.Find(InLoadoutSlot);
}

bool UHeroEquipmentComponent::DoesWeaponDefinitionMatchLoadoutSlot(
	const UDataAsset_WeaponDefinition* InWeaponDefinition,
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	// 这里只回答“这份定义是否合法落进这个固定槽位”，
	// 不推进 definition 加载、runtime 预热或正式装备写回。
	if (!InWeaponDefinition)
	{
		return InLoadoutSlot == EHeroWeaponLoadoutSlot::Unarmed;
	}

	const FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return false;
	}

	return InWeaponDefinition->MatchesWeaponCategory(RuntimeState->LoadoutDefinition.AllowedWeaponCategory);
}

void UHeroEquipmentComponent::BroadcastCurrentWeaponStateChanged() const
{
	// 对外统一广播这一份“当前生效快照”。
	// Combat、Anim、UI 等外部消费者都应从这里同步，而不是各自回头拼槽位运行时状态。
	EquippedWeaponStateChangedDelegate.Broadcast(GetCurrentEquippedWeaponState());
	BroadcastLoadoutUIStateChanged();
}

void UHeroEquipmentComponent::BroadcastWeaponLoadoutStartupReady() const
{
	// startup 正式状态机不在 equipment 内维护；
	// 这里仅作为旧外部调用方的桥接口，把 ready 广播继续转给 state 宿主。
	if (const UHeroLoadoutStateComponent* LoadoutStateComponent = GetLoadoutStateComponent(this))
	{
		LoadoutStateComponent->BroadcastWeaponLoadoutStartupReady();
	}
}

void UHeroEquipmentComponent::BroadcastWeaponLoadoutStartupFailed(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FString& InFailureReason) const
{
	// failed 广播同样只桥接给 state 宿主，避免 equipment 再并行持有 startup 失败状态。
	if (const UHeroLoadoutStateComponent* LoadoutStateComponent = GetLoadoutStateComponent(this))
	{
		LoadoutStateComponent->BroadcastWeaponLoadoutStartupFailed(InLoadoutSlot, InFailureReason);
	}
}

void UHeroEquipmentComponent::BroadcastWeaponLoadoutEquipFailed(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	// 这里只广播“这次正式装备请求失败了”，
	// 不在广播口追加资源回滚、槽位配置回滚或 startup 状态改写。
	WeaponLoadoutEquipFailedDelegate.Broadcast(InLoadoutSlot);
}

void UHeroEquipmentComponent::BroadcastLoadoutUIStateChanged() const
{
	// UI 快照桥已迁到 state 组件；
	// equipment 这里只转发脏标记，不自己维护独立 UI 状态。
	if (const UHeroLoadoutStateComponent* LoadoutStateComponent = GetLoadoutStateComponent(this))
	{
		LoadoutStateComponent->BroadcastLoadoutUIStateChanged();
	}
}

bool UHeroEquipmentComponent::EquipResolvedLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	UDataAsset_WeaponDefinition* InWeaponDefinition)
{
	if (!InWeaponDefinition)
	{
		UE_LOG(LogHeroEquipmentComponent, Warning, TEXT("EquipResolvedLoadoutSlot 收到空武器定义，已拒绝。槽位=%d"), static_cast<int32>(InLoadoutSlot));
		return false;
	}

	// 走到这里时，目标槽位、目标定义、目标实例都已经解析完成。
	// 这个函数只负责真正把“当前装备态”切换过去，并刷新表现层和外部订阅者。
	const FGameplayTag TargetWeaponTag = InWeaponDefinition->WeaponTag;

	if (!DoesWeaponDefinitionMatchLoadoutSlot(InWeaponDefinition, InLoadoutSlot))
	{
		return false;
	}

	AHeroWeaponBase* TargetWeaponInstance = nullptr;
	UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	if (!LoadoutRuntimeComponent
		|| !LoadoutRuntimeComponent->ResolveEquipTargetWeaponInstance(
			InLoadoutSlot,
			InWeaponDefinition,
			TargetWeaponInstance))
	{
		return false;
	}

	// 这里显式把 Definition 与 Instance 一并纳入判断，
	// 避免“Tag 相同但资源或实例并不是同一份”的状态被误判成已装备完成。
	const bool bAlreadyEquippedResolvedWeapon =
		EquippedWeaponState.EquippedLoadoutSlot == InLoadoutSlot
		&& EquippedWeaponState.EquippedWeaponTag == TargetWeaponTag
		&& EquippedWeaponState.EquippedWeaponDefinition == InWeaponDefinition
		&& EquippedWeaponState.EquippedWeaponInstance == TargetWeaponInstance;
	if (bAlreadyEquippedResolvedWeapon)
	{
		return true;
	}

	const FHeroEquippedWeaponState PreviousEquippedState = EquippedWeaponState;
	// 正式切换当前生效快照前，先收掉旧武器的附加能力、攻击检测和已装备表现。
	DeactivateCurrentEquippedWeapon();

	EquippedWeaponState.SetEquippedState(
		InLoadoutSlot,
		TargetWeaponTag,
		InWeaponDefinition->GetWeaponCategory(),
		InWeaponDefinition,
		TargetWeaponInstance);

	if (!ActivateCurrentEquippedWeapon())
	{
		UE_LOG(
			LogHeroEquipmentComponent,
			Warning,
			TEXT("正式装备武器失败：无法应用目标武器表现。槽位=%s，WeaponTag=%s，WeaponSubtypeTag=%s"),
			*HeroEquipmentComponentLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*TargetWeaponTag.ToString(),
			*InWeaponDefinition->GetWeaponSubtypeTag().ToString());

		SetWeaponEquippedState(TargetWeaponInstance, false);
		EquippedWeaponState = PreviousEquippedState;
		ActivateCurrentEquippedWeapon();
		return false;
	}

	// 只有当新武器表现和附加能力都成功激活后，才对外广播这次正式装备结果。
	BroadcastCurrentWeaponStateChanged();
	return true;
}

bool UHeroEquipmentComponent::SetWeaponEquippedState(AHeroWeaponBase* InWeapon, const bool bIsEquipped) const
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !InWeapon)
	{
		return false;
	}

	// 装备组件只关心“此刻该不该把武器当作已装备表现”。
	// 真正如何处理 Mesh、挂点与碰撞，由角色本体统一执行。
	// 因此它只是对 HeroAssembly 发一个表现请求，不自己维护 socket、可见性或碰撞状态机。
	if (UHeroAssemblyComponent* HeroAssemblyComponent = OwnerHeroCharacter->GetHeroAssemblyComponent())
	{
		return HeroAssemblyComponent->ApplyWeaponActorPresentation(InWeapon, bIsEquipped);
	}

	return false;
}

void UHeroEquipmentComponent::DeactivateCurrentEquippedWeapon()
{
	// 这里收的是“当前已装备武器”这一层附加能力和表现。
	// 它不决定下一个要切到哪个槽位，只为 EquipResolvedLoadoutSlot 腾出干净的当前装备上下文。
	RemoveWeaponDefinitionAbilities(EquippedWeaponState.EquippedLoadoutSlot);

	if (EquippedWeaponState.EquippedWeaponInstance)
	{
		// 当前切出的武器只切回“已挂点但隐藏”的待命状态，不立即销毁。
		EquippedWeaponState.EquippedWeaponInstance->EndAttackDetection();
		SetWeaponEquippedState(EquippedWeaponState.EquippedWeaponInstance, false);
	}
}

bool UHeroEquipmentComponent::ActivateCurrentEquippedWeapon()
{
	// 只有写回 EquippedWeaponState 后，目标武器定义带来的附加能力才允许正式生效。
	GrantWeaponDefinitionAbilities(
		EquippedWeaponState.EquippedLoadoutSlot,
		EquippedWeaponState.EquippedWeaponDefinition);

	if (EquippedWeaponState.EquippedWeaponInstance)
	{
		// 新武器成为当前装备后，统一从这里恢复显示与碰撞。
		if (!SetWeaponEquippedState(EquippedWeaponState.EquippedWeaponInstance, true))
		{
			RemoveWeaponDefinitionAbilities(EquippedWeaponState.EquippedLoadoutSlot);
			return false;
		}
	}

	return true;
}

void UHeroEquipmentComponent::GrantLoadoutAbilities(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot);
	UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!RuntimeState || !OwnerASC)
	{
		return;
	}

	if (RuntimeState->GrantedLoadoutAbilityHandles.Num() > 0)
	{
		// 固定槽能力按常驻模型处理；已经授予过的槽位不在切槽时重复重授。
		return;
	}

	TArray<FActionAbilitySet> GrantedAbilities;
	RuntimeState->LoadoutDefinition.BuildGrantedAbilities(GrantedAbilities);

	for (const FActionAbilitySet& AbilitySet : GrantedAbilities)
	{
		if (!AbilitySet.IsValid())
		{
			continue;
		}

		// 资产层仍然可以继续配置通用战斗输入标签。
		// 真正授予 ASC 前，会在这里按固定武器槽改写成槽位专属输入标签，避免常驻 GA 抢输入。
		const FGameplayTag ResolvedInputTag = ResolveGrantedAbilityInputTagForLoadoutSlot(InLoadoutSlot, AbilitySet.InputTag);
		RuntimeState->GrantedLoadoutAbilityHandles.Add(OwnerASC->GrantAbility(AbilitySet.AbilityToGrant, ResolvedInputTag));
	}
}

void UHeroEquipmentComponent::RemoveLoadoutAbilities(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot);
	UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!RuntimeState || !OwnerASC)
	{
		return;
	}

	for (FGameplayAbilitySpecHandle& GrantedHandle : RuntimeState->GrantedLoadoutAbilityHandles)
	{
		if (GrantedHandle.IsValid())
		{
			OwnerASC->RemovedAbility(GrantedHandle);
		}
	}

	// 这里只回收固定槽常驻能力层，不碰当前武器定义附加能力层。
	RuntimeState->GrantedLoadoutAbilityHandles.Reset();
}

void UHeroEquipmentComponent::GrantWeaponDefinitionAbilities(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const UDataAsset_WeaponDefinition* InWeaponDefinition)
{
	FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot);
	UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!RuntimeState || !OwnerASC || !InWeaponDefinition)
	{
		return;
	}

	if (!InWeaponDefinition->SupportsSpiritWeaponAbilities())
	{
		if (InWeaponDefinition->HasAnySpiritAbilityEntryConfigs())
		{
			UE_LOG(
				LogHeroEquipmentComponent,
				Warning,
				TEXT("[SpiritSkill] 跳过武器定义附加能力授予：当前武器不被识别为灵武器。槽位=%s WeaponTag=%s PropertyType=%s"),
				*HeroEquipmentComponentLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
				*InWeaponDefinition->WeaponTag.ToString(),
				*HeroEquipmentComponentLog::GetWeaponPropertyTypeName(InWeaponDefinition->GetWeaponPropertyType()));
		}

		return;
	}

	if (RuntimeState->GrantedWeaponDefinitionAbilityHandles.Num() > 0)
	{
		// 武器定义附加能力只跟随“当前已装备武器”生效，
		// 因此一旦这一轮已经授予过，就不在同一把当前武器上重复叠加。
		return;
	}

	for (const FActionSpiritAbilityEntryConfig& SpiritAbilityEntry : InWeaponDefinition->GetSpiritAbilityEntryConfigs())
	{
		if (!SpiritAbilityEntry.HasAnyConfiguredData()
			|| !SpiritAbilityEntry.InputTag.IsValid()
			|| !SpiritAbilityEntry.AbilityToGrant)
		{
			continue;
		}

		const FGameplayTag ResolvedInputTag =
			ResolveGrantedAbilityInputTagForLoadoutSlot(InLoadoutSlot, SpiritAbilityEntry.InputTag);
		const FGameplayAbilitySpecHandle GrantedHandle =
			OwnerASC->GrantAbility(SpiritAbilityEntry.AbilityToGrant, ResolvedInputTag);
		RuntimeState->GrantedWeaponDefinitionAbilityHandles.Add(GrantedHandle);

		UE_LOG(
			LogHeroEquipmentComponent,
			Log,
			TEXT("[SpiritSkill] 已授予武器定义附加能力。槽位=%s WeaponTag=%s InputTag=%s Ability=%s EntryKind=%d HandleValid=%s"),
			*HeroEquipmentComponentLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*InWeaponDefinition->WeaponTag.ToString(),
			*ResolvedInputTag.ToString(),
			*GetNameSafe(SpiritAbilityEntry.AbilityToGrant),
			static_cast<int32>(SpiritAbilityEntry.EntryKind),
			GrantedHandle.IsValid() ? TEXT("true") : TEXT("false"));
	}
}

void UHeroEquipmentComponent::RemoveWeaponDefinitionAbilities(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroWeaponLoadoutRuntimeState* RuntimeState = FindRuntimeState(InLoadoutSlot);
	UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!RuntimeState || !OwnerASC)
	{
		return;
	}

	for (FGameplayAbilitySpecHandle& GrantedHandle : RuntimeState->GrantedWeaponDefinitionAbilityHandles)
	{
		if (GrantedHandle.IsValid())
		{
			OwnerASC->RemovedAbility(GrantedHandle);
		}
	}

	// 这里只回收“当前武器定义附加能力”这一层，不影响槽位常驻能力。
	RuntimeState->GrantedWeaponDefinitionAbilityHandles.Reset();
}

FGameplayTag UHeroEquipmentComponent::ResolveGrantedAbilityInputTagForLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FGameplayTag& InInputTag) const
{
	// 这里只负责把资产层配置的通用输入标签，改写成当前固定武器槽对应的专属输入标签。
	// 这样资产侧仍能复用通用输入名，而运行时常驻槽位 GA 不会因为共用同一输入标签而互相抢占。
	if (!InInputTag.IsValid())
	{
		return InInputTag;
	}

	auto ResolveBySlot =
		[InLoadoutSlot](
			const FGameplayTag& UnarmedTag,
			const FGameplayTag& MeleeTag,
			const FGameplayTag& RangedTag,
			const FGameplayTag& HybridTag) -> FGameplayTag
		{
			switch (InLoadoutSlot)
			{
			case EHeroWeaponLoadoutSlot::Unarmed:
				return UnarmedTag;

			case EHeroWeaponLoadoutSlot::MeleeWeapon:
				return MeleeTag;

			case EHeroWeaponLoadoutSlot::RangedWeapon:
				return RangedTag;

			case EHeroWeaponLoadoutSlot::HybridWeapon:
				return HybridTag;

			default:
				break;
			}

			return FGameplayTag();
		};

	if (InInputTag == ActionGameplayTags::InputTag_GameplayAbility_Attack_Light)
	{
		return ResolveBySlot(
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Light_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Light_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Light_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Light_Hybrid);
	}

	if (InInputTag == ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy)
	{
		return ResolveBySlot(
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy_Hybrid);
	}

	if (InInputTag == ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter)
	{
		return ResolveBySlot(
			ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter_Hybrid);
	}

	if (InInputTag == ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint)
	{
		return ResolveBySlot(
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint_Hybrid);
	}

	if (InInputTag == ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne)
	{
		return ResolveBySlot(
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne_Hybrid);
	}

	if (InInputTag == ActionGameplayTags::InputTag_GameplayAbility_Dodge)
	{
		return ResolveBySlot(
			ActionGameplayTags::InputTag_GameplayAbility_Dodge_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Dodge_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Dodge_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Dodge_Hybrid);
	}

	if (InInputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense)
	{
		return ResolveBySlot(
			ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense_Hybrid);
	}

	return InInputTag;
}
