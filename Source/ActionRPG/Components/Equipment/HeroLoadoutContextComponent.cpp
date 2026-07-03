// 文件说明：实现英雄装备域属性缓存 / 发射物标签上下文组件。
#include "Components/Equipment/HeroLoadoutContextComponent.h"

#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutEffectComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "Components/Equipment/HeroLoadoutStateComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroLoadoutContextComponent, Log, All);

namespace HeroLoadoutContextLog
{
	static FString GetWeaponLoadoutSlotName(const EHeroWeaponLoadoutSlot InLoadoutSlot)
	{
		if (const UEnum* LoadoutSlotEnum = StaticEnum<EHeroWeaponLoadoutSlot>())
		{
			return LoadoutSlotEnum->GetNameStringByValue(static_cast<int64>(InLoadoutSlot));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InLoadoutSlot));
	}
}

UHeroLoadoutContextComponent::UHeroLoadoutContextComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHeroLoadoutContextComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRuntimeStateForHeroStartup();
	Super::EndPlay(EndPlayReason);
}

void UHeroLoadoutContextComponent::EnsureLoadoutSlotContextState(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	// context 组件按槽位懒建骨架，避免未注册槽位提前持有多余的属性缓存和发射物标签运行态。
	ContextRuntimeStatesBySlot.FindOrAdd(InLoadoutSlot);
}

void UHeroLoadoutContextComponent::ResetLoadoutSlotContextRuntime(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutContextRuntimeState* RuntimeState = FindContextRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return;
	}

	const bool bHadSelectedProjectileConfigTag = RuntimeState->SelectedProjectileConfigTag.IsValid();
	// 这里只清单槽位 context runtime。
	// 清理后要同步通知 effect 消费链并重新 clamp 当前属性，避免旧缓存继续影响角色属性上限解析。
	RuntimeState->Reset();

	if (UHeroLoadoutEffectComponent* LoadoutEffectComponent = GetOwningHeroLoadoutEffectComponent())
	{
		LoadoutEffectComponent->HandleLoadoutSlotWeaponAttributeCacheCleared(
			InLoadoutSlot,
			RuntimeState->WeaponAttributeCache);
	}

	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		if (UActionAttributeSetBase* AttributeSet = OwnerHeroCharacter->GetActionAttributeSet())
		{
			AttributeSet->ClampCurrentValuesToResolvedMaximums();
		}
	}

	if (bHadSelectedProjectileConfigTag)
	{
		BroadcastLoadoutUIStateChanged();
	}
}

void UHeroLoadoutContextComponent::ResetRuntimeStateForHeroStartup()
{
	// 组件退场或启动链重置时，清掉本组件持有的槽位上下文和依赖缓存。
	// 真正的 definition、实例和当前生效装备态仍由 runtime/equipment 宿主维护。
	ContextRuntimeStatesBySlot.Reset();
	CachedHeroCharacter.Reset();
	CachedHeroLoadoutEffectComponent.Reset();
	CachedHeroLoadoutRuntimeComponent.Reset();
	CachedHeroLoadoutStateComponent.Reset();
}

bool UHeroLoadoutContextComponent::GetWeaponAttributeCacheByLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FActionWeaponAttributeCacheData& OutAttributeCache) const
{
	OutAttributeCache.Reset();

	const FHeroLoadoutContextRuntimeState* RuntimeState = FindContextRuntimeState(InLoadoutSlot);
	if (!RuntimeState || !RuntimeState->bHasWeaponAttributeCache)
	{
		return false;
	}

	OutAttributeCache = RuntimeState->WeaponAttributeCache;
	return true;
}

bool UHeroLoadoutContextComponent::HasLoadoutSlotWeaponAttributeCache(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	const FHeroLoadoutContextRuntimeState* RuntimeState = FindContextRuntimeState(InLoadoutSlot);
	return RuntimeState && RuntimeState->bHasWeaponAttributeCache;
}

bool UHeroLoadoutContextComponent::TryGetLoadoutSlotWeaponAttributeCache(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FActionWeaponAttributeCacheData*& OutAttributeCache) const
{
	OutAttributeCache = nullptr;

	const FHeroLoadoutContextRuntimeState* RuntimeState = FindContextRuntimeState(InLoadoutSlot);
	if (!RuntimeState || !RuntimeState->bHasWeaponAttributeCache)
	{
		return false;
	}

	OutAttributeCache = &RuntimeState->WeaponAttributeCache;
	return true;
}

bool UHeroLoadoutContextComponent::TryGetMutableLoadoutSlotWeaponAttributeCache(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FActionWeaponAttributeCacheData*& OutAttributeCache)
{
	OutAttributeCache = nullptr;

	FHeroLoadoutContextRuntimeState* RuntimeState = FindContextRuntimeState(InLoadoutSlot);
	if (!RuntimeState || !RuntimeState->bHasWeaponAttributeCache)
	{
		return false;
	}

	OutAttributeCache = &RuntimeState->WeaponAttributeCache;
	return true;
}

void UHeroLoadoutContextComponent::RefreshLoadoutSlotWeaponAttributeCache(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const UDataAsset_WeaponDefinition* InWeaponDefinition)
{
	FHeroLoadoutContextRuntimeState& RuntimeState = FindOrAddContextRuntimeState(InLoadoutSlot);
	if (!InWeaponDefinition)
	{
		ClearLoadoutSlotWeaponAttributeCache(InLoadoutSlot);
		return;
	}

	// context 组件持有的是当前槽位武器定义派生出来的正式属性缓存镜像。
	// 除 EquippedAttributeBonuses 外，还要同步写入运行时真正会被消费的武器属性类型、伤害类型、元素标签和额外 hit effect 语义。
	RuntimeState.WeaponAttributeCache = InWeaponDefinition->EquippedAttributeBonuses;
	RuntimeState.WeaponAttributeCache.WeaponPropertyType = InWeaponDefinition->GetWeaponPropertyType();
	RuntimeState.WeaponAttributeCache.DamageType = InWeaponDefinition->GetDamageType();
	RuntimeState.WeaponAttributeCache.DamageElementTypeTag = InWeaponDefinition->GetDamageElementTypeTag();
	RuntimeState.WeaponAttributeCache.bAllowAdditionalHitEffects = InWeaponDefinition->AllowsAdditionalHitEffects();
	RuntimeState.WeaponAttributeCacheSourceTag = InWeaponDefinition->WeaponTag;
	RuntimeState.bHasWeaponAttributeCache = true;

	if (UHeroLoadoutEffectComponent* LoadoutEffectComponent = GetOwningHeroLoadoutEffectComponent())
	{
		// effect 组件是缓存变化的消费方；context 自己仍是这份缓存的唯一正式状态源。
		LoadoutEffectComponent->HandleLoadoutSlotWeaponAttributeCacheRefreshed(
			InLoadoutSlot,
			RuntimeState.WeaponAttributeCache,
			InWeaponDefinition);
	}

	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		if (UActionAttributeSetBase* AttributeSet = OwnerHeroCharacter->GetActionAttributeSet())
		{
			AttributeSet->ClampCurrentValuesToResolvedMaximums();
		}
	}
}

void UHeroLoadoutContextComponent::ClearLoadoutSlotWeaponAttributeCache(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutContextRuntimeState& RuntimeState = FindOrAddContextRuntimeState(InLoadoutSlot);
	// 清缓存不仅要回零本槽位镜像，还要同步通知 effect 观察链，并重新 clamp 当前属性，
	// 避免旧武器最大值修正残留在角色实时属性上。
	RuntimeState.WeaponAttributeCache.Reset();
	RuntimeState.WeaponAttributeCacheSourceTag = FGameplayTag();
	RuntimeState.bHasWeaponAttributeCache = false;

	if (UHeroLoadoutEffectComponent* LoadoutEffectComponent = GetOwningHeroLoadoutEffectComponent())
	{
		LoadoutEffectComponent->HandleLoadoutSlotWeaponAttributeCacheCleared(
			InLoadoutSlot,
			RuntimeState.WeaponAttributeCache);
	}

	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		if (UActionAttributeSetBase* AttributeSet = OwnerHeroCharacter->GetActionAttributeSet())
		{
			AttributeSet->ClampCurrentValuesToResolvedMaximums();
		}
	}
}

bool UHeroLoadoutContextComponent::GetLoadoutSlotSelectedProjectileConfigTag(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FGameplayTag& OutProjectileConfigTag) const
{
	OutProjectileConfigTag = FGameplayTag();

	const UHeroLoadoutRuntimeComponent* RuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	const UDataAsset_WeaponDefinition* LoadedWeaponDefinition = RuntimeComponent
		? RuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot)
		: nullptr;
	const FHeroLoadoutContextRuntimeState* RuntimeState = FindContextRuntimeState(InLoadoutSlot);
	// 读取前必须同时满足三层前置：
	// 槽位 definition 已加载、当前武器支持发射物切换、并且 context runtime 已经存在。
	if (!LoadedWeaponDefinition || !RuntimeState || !LoadedWeaponDefinition->SupportsProjectileSwitching())
	{
		return false;
	}

	OutProjectileConfigTag = RuntimeState->SelectedProjectileConfigTag;
	return OutProjectileConfigTag.IsValid();
}

bool UHeroLoadoutContextComponent::SetLoadoutSlotSelectedProjectileConfigTag(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FGameplayTag InProjectileConfigTag)
{
	UHeroLoadoutRuntimeComponent* RuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
	UDataAsset_WeaponDefinition* LoadedWeaponDefinition = RuntimeComponent
		? RuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot)
		: nullptr;
	FHeroLoadoutContextRuntimeState* RuntimeState = FindContextRuntimeState(InLoadoutSlot);
	if (!LoadedWeaponDefinition || !RuntimeState)
	{
		return false;
	}

	if (!LoadedWeaponDefinition->SupportsProjectileSwitching())
	{
		UE_LOG(
			LogHeroLoadoutContextComponent,
			Warning,
			TEXT("当前武器不支持切换发射物，已拒绝设置。槽位=%s 武器Tag=%s"),
			*HeroLoadoutContextLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*LoadedWeaponDefinition->WeaponTag.ToString());
		return false;
	}

	if (!InProjectileConfigTag.IsValid())
	{
		// 空标签等价于清空当前槽位的选择结果。
		// 这里只改 context runtime 并刷新 UI，不回收武器定义或属性缓存。
		if (!RuntimeState->SelectedProjectileConfigTag.IsValid())
		{
			return true;
		}

		RuntimeState->SelectedProjectileConfigTag = FGameplayTag();
		BroadcastLoadoutUIStateChanged();
		return true;
	}

	FActionProjectileConfig ResolvedProjectileConfig;
	if (!LoadedWeaponDefinition->ResolveSwitchableProjectileConfigByTag(
		InProjectileConfigTag,
		ResolvedProjectileConfig))
	{
		UE_LOG(
			LogHeroLoadoutContextComponent,
			Warning,
			TEXT("目标发射物配置标签无效，已拒绝设置。槽位=%s ProjectileConfigTag=%s"),
			*HeroLoadoutContextLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*InProjectileConfigTag.ToString());
		return false;
	}

	if (RuntimeState->SelectedProjectileConfigTag == InProjectileConfigTag)
	{
		// 重复设置同一标签直接短路，避免无意义 UI 刷新。
		return true;
	}

	// SelectedProjectileConfigTag 是当前槽位的上下文运行态，不是 equipment 宿主或 UI 自己维护的平行状态。
	RuntimeState->SelectedProjectileConfigTag = InProjectileConfigTag;
	BroadcastLoadoutUIStateChanged();
	return true;
}

void UHeroLoadoutContextComponent::ClearLoadoutSlotSelectedProjectileConfigTag(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutContextRuntimeState* RuntimeState = FindContextRuntimeState(InLoadoutSlot);
	if (!RuntimeState || !RuntimeState->SelectedProjectileConfigTag.IsValid())
	{
		return;
	}

	// 这里只清当前槽位的发射物选择上下文，不动武器定义和 WeaponAttributeCache。
	RuntimeState->SelectedProjectileConfigTag = FGameplayTag();
	BroadcastLoadoutUIStateChanged();
}

AActionHeroCharacter* UHeroLoadoutContextComponent::GetOwningHeroCharacter() const
{
	if (CachedHeroCharacter.IsValid())
	{
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedHeroCharacter.Get();
}

UHeroLoadoutEffectComponent* UHeroLoadoutContextComponent::GetOwningHeroLoadoutEffectComponent() const
{
	if (CachedHeroLoadoutEffectComponent.IsValid())
	{
		return CachedHeroLoadoutEffectComponent.Get();
	}

	CachedHeroLoadoutEffectComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UHeroLoadoutEffectComponent>()
		: nullptr;

	return CachedHeroLoadoutEffectComponent.Get();
}

UHeroLoadoutRuntimeComponent* UHeroLoadoutContextComponent::GetOwningHeroLoadoutRuntimeComponent() const
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

UHeroLoadoutStateComponent* UHeroLoadoutContextComponent::GetOwningHeroLoadoutStateComponent() const
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

FHeroLoadoutContextRuntimeState* UHeroLoadoutContextComponent::FindContextRuntimeState(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	// 这些辅助函数只服务当前槽位上下文存取，不表达“当前槽位是否业务就绪”。
	return ContextRuntimeStatesBySlot.Find(InLoadoutSlot);
}

const FHeroLoadoutContextRuntimeState* UHeroLoadoutContextComponent::FindContextRuntimeState(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	return ContextRuntimeStatesBySlot.Find(InLoadoutSlot);
}

FHeroLoadoutContextRuntimeState& UHeroLoadoutContextComponent::FindOrAddContextRuntimeState(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	return ContextRuntimeStatesBySlot.FindOrAdd(InLoadoutSlot);
}

void UHeroLoadoutContextComponent::BroadcastLoadoutUIStateChanged() const
{
	// context 组件不自己维护 UI 脏状态，只桥接给 LoadoutState 统一广播 UI 快照刷新。
	if (const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent())
	{
		LoadoutStateComponent->BroadcastLoadoutUIStateChanged();
	}
}
