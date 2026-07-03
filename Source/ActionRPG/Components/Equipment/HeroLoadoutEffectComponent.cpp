// 文件说明：实现英雄装备域的外部命中效果来源生命周期组件。

#include "Components/Equipment/HeroLoadoutEffectComponent.h"

#include "Characters/ActionHeroCharacter.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroLoadoutEffectComponent, Log, All);

namespace HeroEquipmentHitEffectLog
{
	static FString GetWeaponLoadoutSlotName(const EHeroWeaponLoadoutSlot InLoadoutSlot)
	{
		if (const UEnum* LoadoutSlotEnum = StaticEnum<EHeroWeaponLoadoutSlot>())
		{
			return LoadoutSlotEnum->GetNameStringByValue(static_cast<int64>(InLoadoutSlot));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InLoadoutSlot));
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

UHeroLoadoutEffectComponent::UHeroLoadoutEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHeroLoadoutEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRuntimeStateForHeroStartup();
	Super::EndPlay(EndPlayReason);
}

bool UHeroLoadoutEffectComponent::GetLoadoutSlotExternalAdditionalHitEffects(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	TArray<FActionHitEffectEntry>& OutHitEffects) const
{
	OutHitEffects.Reset();

	// 查询口只读当前正式 effect runtime 已聚合好的结果。
	// 如果装备宿主、context 缓存或 runtime 本身还没就绪，这里宁可返回 false，也不拼装一份临时结果给外部误用。
	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
	const FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!EquipmentComponent
		|| !LoadoutContextComponent
		|| !RuntimeState
		|| !LoadoutContextComponent->HasLoadoutSlotWeaponAttributeCache(InLoadoutSlot))
	{
		return false;
	}

	OutHitEffects = RuntimeState->ExternalAdditionalHitEffects;
	return OutHitEffects.Num() > 0;
}

bool UHeroLoadoutEffectComponent::GetLoadoutSlotProjectileInheritedExternalAdditionalHitEffects(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	TArray<FActionHitEffectEntry>& OutHitEffects) const
{
	OutHitEffects.Reset();

	// projectile inherited 数组与 melee 聚合结果共用同一份正式 runtime，
	// 这里只读取已经按 apply scope 过滤好的结果，不在查询期再做额外推导。
	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
	const FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!EquipmentComponent
		|| !LoadoutContextComponent
		|| !RuntimeState
		|| !LoadoutContextComponent->HasLoadoutSlotWeaponAttributeCache(InLoadoutSlot))
	{
		return false;
	}

	OutHitEffects = RuntimeState->ProjectileInheritedExternalAdditionalHitEffects;
	return OutHitEffects.Num() > 0;
}

bool UHeroLoadoutEffectComponent::GetLoadoutSlotExternalHitEffectSourceDebugSnapshots(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	TArray<FActionExternalHitEffectSourceDebugSnapshot>& OutSnapshots) const
{
	OutSnapshots.Reset();

	const FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return false;
	}

	const float CurrentWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	// 调试快照只镜像当前正式来源状态，帮助外部定位 scoped / timed 来源是否仍存活、是否被 suppress。
	for (const TPair<FGameplayTag, FActionExternalHitEffectSourceRuntimeState>& SourceStatePair :
		RuntimeState->ExternalHitEffectSourceStates)
	{
		const FActionExternalHitEffectSourceRuntimeState& SourceState = SourceStatePair.Value;
		if (!SourceState.IsValidRuntimeState())
		{
			continue;
		}

		FActionExternalHitEffectSourceDebugSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
		Snapshot.SourceTag = SourceState.SourceEntry.SourceTag;
		Snapshot.ApplyScope = SourceState.SourceEntry.ApplyScope;
		Snapshot.bHasDurationLimit = SourceState.bHasDurationLimit;
		Snapshot.RemainingTime = SourceState.bHasDurationLimit
			? FMath::Max(SourceState.ExpireWorldTime - CurrentWorldTime, 0.f)
			: -1.f;
		Snapshot.bSuppressedByCurrentWeaponPolicy = RuntimeState->bSuppressedByCurrentWeaponPolicy;
		Snapshot.EffectCount = SourceState.SourceEntry.HitEffects.Num();
	}

	return OutSnapshots.Num() > 0;
}

bool UHeroLoadoutEffectComponent::ApplyExternalAdditionalHitEffectsRequestToLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FActionExternalAdditionalHitEffectsApplyRequest& InRequest,
	FHeroExternalAdditionalHitEffectsApplyResult& OutResult)
{
	// 高层 direct request 只是写入入口。
	// 真正的正式状态仍收口在当前槽位的 effect runtime，并在通过所有前置校验后再进入 direct 层写入。
	HeroEquipmentHitEffectLog::SetExternalAdditionalHitEffectsApplyResult(
		OutResult,
		false,
		InLoadoutSlot,
		EHeroExternalHitEffectSourceApplyFailureReason::InvalidRequest);

	if (!InRequest.IsValidRequest())
	{
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层 direct 外部额外命中效果写入请求无效，已拒绝。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (!IsLoadoutSlotRegistered(InLoadoutSlot))
	{
		HeroEquipmentHitEffectLog::SetExternalAdditionalHitEffectsApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::InvalidTargetLoadoutSlot);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层 direct 外部额外命中效果写入失败：目标固定武器槽未注册。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (const UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
		!LoadoutRuntimeComponent || !LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot))
	{
		HeroEquipmentHitEffectLog::SetExternalAdditionalHitEffectsApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::NoValidWeaponDefinition);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层 direct 外部额外命中效果写入失败：目标槽位当前没有有效武器定义。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
		!LoadoutContextComponent || !LoadoutContextComponent->HasLoadoutSlotWeaponAttributeCache(InLoadoutSlot))
	{
		HeroEquipmentHitEffectLog::SetExternalAdditionalHitEffectsApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::TargetWeaponRuntimeNotReady);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层 direct 外部额外命中效果写入失败：目标武器运行时缓存未就绪。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (!DoesLoadoutSlotWeaponAllowAdditionalHitEffects(InLoadoutSlot))
	{
		HeroEquipmentHitEffectLog::SetExternalAdditionalHitEffectsApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::WeaponPolicyDisallowsAdditionalHitEffects);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层 direct 外部额外命中效果写入失败：当前武器策略不允许额外命中效果。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (!SetLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot, InRequest.HitEffects))
	{
		HeroEquipmentHitEffectLog::SetExternalAdditionalHitEffectsApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::InternalApplyFailed);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层 direct 外部额外命中效果写入失败：底层 direct 数组写入未通过。槽位=%s 原因=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*HeroEquipmentHitEffectLog::GetExternalHitEffectApplyFailureReasonName(OutResult.FailureReason));
		return false;
	}

	HeroEquipmentHitEffectLog::SetExternalAdditionalHitEffectsApplyResult(
		OutResult,
		true,
		InLoadoutSlot,
		EHeroExternalHitEffectSourceApplyFailureReason::None);
	return true;
}

bool UHeroLoadoutEffectComponent::ApplyExternalHitEffectSourceRequestToLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FActionExternalHitEffectSourceApplyRequest& InRequest,
	FHeroExternalHitEffectSourceApplyResult& OutResult)
{
	// 高层具名来源 request 统一落到 scoped / timed 两条正式入口，
	// 避免 direct、具名来源和定时来源各自发展成互不一致的平行写链。
	HeroEquipmentHitEffectLog::SetExternalHitEffectSourceApplyResult(
		OutResult,
		false,
		InLoadoutSlot,
		EHeroExternalHitEffectSourceApplyFailureReason::InvalidRequest);

	if (!InRequest.IsValidRequest())
	{
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层外部命中效果来源请求无效，已拒绝写入。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (!IsLoadoutSlotRegistered(InLoadoutSlot))
	{
		HeroEquipmentHitEffectLog::SetExternalHitEffectSourceApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::InvalidTargetLoadoutSlot);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层外部命中效果来源写入失败：目标固定武器槽未注册。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (const UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent = GetOwningHeroLoadoutRuntimeComponent();
		!LoadoutRuntimeComponent || !LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot))
	{
		HeroEquipmentHitEffectLog::SetExternalHitEffectSourceApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::NoValidWeaponDefinition);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层外部命中效果来源写入失败：目标槽位当前没有有效武器定义。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
		!LoadoutContextComponent || !LoadoutContextComponent->HasLoadoutSlotWeaponAttributeCache(InLoadoutSlot))
	{
		HeroEquipmentHitEffectLog::SetExternalHitEffectSourceApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::TargetWeaponRuntimeNotReady);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层外部命中效果来源写入失败：目标武器运行时缓存未就绪。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (!DoesLoadoutSlotWeaponAllowAdditionalHitEffects(InLoadoutSlot))
	{
		HeroEquipmentHitEffectLog::SetExternalHitEffectSourceApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::WeaponPolicyDisallowsAdditionalHitEffects);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层外部命中效果来源写入失败：当前武器策略不允许额外命中效果。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	const bool bApplySucceeded = InRequest.bUseDurationLimit
		? ApplyTimedScopedExternalHitEffectSourceToLoadoutSlot(
			InLoadoutSlot,
			InRequest.SourceTag,
			InRequest.ApplyScope,
			InRequest.HitEffects,
			InRequest.Duration)
		: SetScopedLoadoutSlotExternalHitEffectSource(
			InLoadoutSlot,
			InRequest.SourceTag,
			InRequest.ApplyScope,
			InRequest.HitEffects);

	if (!bApplySucceeded)
	{
		HeroEquipmentHitEffectLog::SetExternalHitEffectSourceApplyResult(
			OutResult,
			false,
			InLoadoutSlot,
			EHeroExternalHitEffectSourceApplyFailureReason::InternalApplyFailed);
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("高层外部命中效果来源写入失败：底层来源写入未通过。槽位=%s 来源=%s 原因=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*InRequest.SourceTag.ToString(),
			*HeroEquipmentHitEffectLog::GetExternalHitEffectApplyFailureReasonName(OutResult.FailureReason));
		return false;
	}

	HeroEquipmentHitEffectLog::SetExternalHitEffectSourceApplyResult(
		OutResult,
		true,
		InLoadoutSlot,
		EHeroExternalHitEffectSourceApplyFailureReason::None);
	return true;
}

bool UHeroLoadoutEffectComponent::DoesLoadoutSlotWeaponAllowAdditionalHitEffects(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
	const FActionWeaponAttributeCacheData* WeaponAttributeCache = nullptr;
	// 这里判断的是“武器策略是否允许额外命中效果参与聚合”，
	// 不是当前 effect runtime 是否已经被 suppressed；后者还要再看运行态本身。
	return LoadoutContextComponent
		&& LoadoutContextComponent->TryGetLoadoutSlotWeaponAttributeCache(InLoadoutSlot, WeaponAttributeCache)
		&& WeaponAttributeCache
		&& WeaponAttributeCache->bAllowAdditionalHitEffects;
}

bool UHeroLoadoutEffectComponent::SetLoadoutSlotExternalAdditionalHitEffects(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const TArray<FActionHitEffectEntry>& InHitEffects)
{
	if (!IsLoadoutSlotRegistered(InLoadoutSlot))
	{
		return false;
	}

	FHeroLoadoutHitEffectRuntimeState& RuntimeState =
		FindOrAddHitEffectRuntimeState(InLoadoutSlot);

	if (InHitEffects.Num() <= 0)
	{
		// direct 层清空后，仍要统一走聚合重建，把 scoped/timed 来源与 cache mirror 一起收整齐。
		RuntimeState.DirectExternalAdditionalHitEffects.Reset();
		RebuildLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot);
		return true;
	}

	FString ValidationFailureReason;
	if (!ValidateExternalHitEffects(InHitEffects, InLoadoutSlot, ValidationFailureReason))
	{
		UE_LOG(LogHeroLoadoutEffectComponent, Warning, TEXT("%s"), *ValidationFailureReason);
		return false;
	}

	RuntimeState.DirectExternalAdditionalHitEffects = InHitEffects;
	// direct 数组本身只是正式运行态的一层输入。
	// 写入后必须统一重建聚合镜像，才能得到 external / projectile inherited 两份消费结果。
	RebuildLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot);
	if (IsLoadoutSlotExternalHitEffectSuppressed(InLoadoutSlot))
	{
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Log,
			TEXT("固定武器槽外部额外命中效果 direct 层已写入，但当前武器不兼容，先挂起不参与聚合。槽位=%s 数量=%d"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			InHitEffects.Num());
	}
	return true;
}

bool UHeroLoadoutEffectComponent::SetScopedLoadoutSlotExternalHitEffectSource(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FGameplayTag InSourceTag,
	const EActionExternalHitEffectApplyScope InApplyScope,
	const TArray<FActionHitEffectEntry>& InHitEffects)
{
	if (!IsLoadoutSlotRegistered(InLoadoutSlot))
	{
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("目标固定武器槽未注册，无法设置具名外部命中效果来源。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	if (!InSourceTag.IsValid())
	{
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("具名外部命中效果来源标签无效，已拒绝写入。槽位=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot));
		return false;
	}

	FHeroLoadoutHitEffectRuntimeState& RuntimeState =
		FindOrAddHitEffectRuntimeState(InLoadoutSlot);
	FActionExternalHitEffectSourceRuntimeState* MatchedSourceState =
		RuntimeState.ExternalHitEffectSourceStates.Find(InSourceTag);

	if (InHitEffects.Num() <= 0)
	{
		if (MatchedSourceState)
		{
			// 具名来源清空时，先收 timer，再移除 source state，最后统一重建聚合镜像。
			ClearLoadoutSlotExternalHitEffectSourceTimer(InLoadoutSlot, InSourceTag);
			RuntimeState.ExternalHitEffectSourceStates.Remove(InSourceTag);
			UE_LOG(
				LogHeroLoadoutEffectComponent,
				Log,
				TEXT("固定武器槽外部命中效果来源已移除。槽位=%s 来源=%s"),
				*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
				*InSourceTag.ToString());
		}

		RebuildLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot);
		return true;
	}

	FString ValidationFailureReason;
	if (!ValidateExternalHitEffects(InHitEffects, InLoadoutSlot, ValidationFailureReason))
	{
		UE_LOG(LogHeroLoadoutEffectComponent, Warning, TEXT("%s"), *ValidationFailureReason);
		return false;
	}

	const bool bWasExistingSource = MatchedSourceState != nullptr;
	FActionExternalHitEffectSourceRuntimeState& TargetSourceState =
		RuntimeState.ExternalHitEffectSourceStates.FindOrAdd(InSourceTag);
	// scoped source 以 SourceTag 为唯一 key。
	// 覆盖写入时直接改这份正式来源状态，不再保留旧版本或并行来源壳。
	TargetSourceState.SourceEntry.SourceTag = InSourceTag;
	TargetSourceState.SourceEntry.ApplyScope = InApplyScope;
	TargetSourceState.SourceEntry.HitEffects = InHitEffects;
	ClearLoadoutSlotExternalHitEffectSourceTimer(InLoadoutSlot, InSourceTag);
	RebuildLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot);

	UE_LOG(
		LogHeroLoadoutEffectComponent,
		Log,
		TEXT("固定武器槽外部命中效果来源已%s。槽位=%s 来源=%s 效果数=%d"),
		bWasExistingSource ? TEXT("覆盖") : TEXT("新增"),
		*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		*InSourceTag.ToString(),
		InHitEffects.Num());
	if (IsLoadoutSlotExternalHitEffectSuppressed(InLoadoutSlot))
	{
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Log,
			TEXT("固定武器槽外部命中效果来源当前因武器不兼容而挂起。槽位=%s 来源=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*InSourceTag.ToString());
	}
	return true;
}

bool UHeroLoadoutEffectComponent::AddExternalAdditionalHitEffectToLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FActionHitEffectEntry& InHitEffect)
{
	const FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	TArray<FActionHitEffectEntry> CurrentHitEffects =
		RuntimeState ? RuntimeState->DirectExternalAdditionalHitEffects : TArray<FActionHitEffectEntry>();
	CurrentHitEffects.Add(InHitEffect);
	return SetLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot, CurrentHitEffects);
}

void UHeroLoadoutEffectComponent::ClearLoadoutSlotExternalAdditionalHitEffects(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return;
	}

	// 这里收的是“当前槽位所有外部命中效果来源”这一整层正式运行态。
	// 先清 direct，再清全部具名来源，最后统一重建聚合镜像，避免 cache 侧残留旧结果。
	RuntimeState->DirectExternalAdditionalHitEffects.Reset();
	ClearLoadoutSlotExternalHitEffectSourceStates(InLoadoutSlot);
	RebuildLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot);
}

bool UHeroLoadoutEffectComponent::ApplyTimedScopedExternalHitEffectSourceToLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FGameplayTag InSourceTag,
	const EActionExternalHitEffectApplyScope InApplyScope,
	const TArray<FActionHitEffectEntry>& InHitEffects,
	const float InDuration)
{
	if (InDuration <= 0.f)
	{
		UE_LOG(
			LogHeroLoadoutEffectComponent,
			Warning,
			TEXT("外部命中效果来源持续时间必须大于 0，已拒绝写入定时来源。槽位=%s 来源=%s"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			*InSourceTag.ToString());
		return false;
	}

	if (!SetScopedLoadoutSlotExternalHitEffectSource(
		InLoadoutSlot,
		InSourceTag,
		InApplyScope,
		InHitEffects))
	{
		return false;
	}

	// timed source 不发明第二套业务状态；它只是先写 scoped source，再给这条来源挂上 duration 生命周期。
	FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!RuntimeState || !GetWorld())
	{
		return false;
	}

	FActionExternalHitEffectSourceRuntimeState* SourceState =
		RuntimeState->ExternalHitEffectSourceStates.Find(InSourceTag);
	if (!SourceState)
	{
		return false;
	}

	SourceState->bHasDurationLimit = true;
	SourceState->ExpireWorldTime = GetWorld()->GetTimeSeconds() + InDuration;
	FTimerDelegate ExpireDelegate;
	ExpireDelegate.BindUObject(
		this,
		&ThisClass::HandleTimedExternalHitEffectSourceExpired,
		InLoadoutSlot,
		InSourceTag);
	GetWorld()->GetTimerManager().SetTimer(SourceState->ExpireTimerHandle, ExpireDelegate, InDuration, false);
	UE_LOG(
		LogHeroLoadoutEffectComponent,
		Log,
		TEXT("固定武器槽外部命中效果来源定时已刷新。槽位=%s 来源=%s 持续时间=%.2f"),
		*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		*InSourceTag.ToString(),
		InDuration);
	return true;
}

bool UHeroLoadoutEffectComponent::RemoveLoadoutSlotExternalHitEffectSource(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FGameplayTag InSourceTag)
{
	if (!InSourceTag.IsValid())
	{
		return false;
	}

	TArray<FActionHitEffectEntry> EmptyHitEffects;
	// 正式移除入口继续复用 scoped source 写链，保证清 timer、删 source state 和重建聚合镜像都走同一套收尾。
	return SetScopedLoadoutSlotExternalHitEffectSource(
		InLoadoutSlot,
		InSourceTag,
		EActionExternalHitEffectApplyScope::MeleeAndProjectile,
		EmptyHitEffects);
}

void UHeroLoadoutEffectComponent::HandleLoadoutSlotWeaponAttributeCacheRefreshed(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FActionWeaponAttributeCacheData& InOutAttributeCache,
	const UDataAsset_WeaponDefinition* InWeaponDefinition)
{
	const FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	const bool bHadStoredExternalHitEffects = RuntimeState
		&& (RuntimeState->DirectExternalAdditionalHitEffects.Num() > 0
			|| RuntimeState->ExternalHitEffectSourceStates.Num() > 0);
	const bool bWasSuppressedBeforeRefresh = RuntimeState
		? RuntimeState->bSuppressedByCurrentWeaponPolicy
		: true;

	// context 缓存刷新后，要立刻按新武器策略重建 effect 聚合镜像。
	// 这样 suppression、projectile inherited 和 cache mirror 三层结果才能跟新 definition 一起切到位。
	RebuildLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot, InOutAttributeCache);

	const FHeroLoadoutHitEffectRuntimeState* RefreshedRuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	const bool bSuppressedAfterRefresh = RefreshedRuntimeState
		? RefreshedRuntimeState->bSuppressedByCurrentWeaponPolicy
		: true;

	if (bHadStoredExternalHitEffects && InWeaponDefinition)
	{
		if (!bWasSuppressedBeforeRefresh && bSuppressedAfterRefresh)
		{
			UE_LOG(
				LogHeroLoadoutEffectComponent,
				Log,
				TEXT("固定武器槽外部命中效果来源因当前武器不兼容而挂起。槽位=%s 武器Tag=%s"),
				*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
				*InWeaponDefinition->WeaponTag.ToString());
		}
		else if (bWasSuppressedBeforeRefresh && !bSuppressedAfterRefresh)
		{
			UE_LOG(
				LogHeroLoadoutEffectComponent,
				Log,
				TEXT("固定武器槽外部命中效果来源已恢复参与聚合。槽位=%s 武器Tag=%s"),
				*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
				*InWeaponDefinition->WeaponTag.ToString());
		}
	}
}

void UHeroLoadoutEffectComponent::HandleLoadoutSlotWeaponAttributeCacheCleared(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FActionWeaponAttributeCacheData& InOutAttributeCache)
{
	// context 清空后，effect 组件不能继续把旧聚合结果留在缓存镜像里。
	// 这里先把 runtime 镜像回零，再把当前聚合数组切成 suppressed + empty。
	RefreshCacheMirrorFromRuntime(FindHitEffectRuntimeState(InLoadoutSlot), InOutAttributeCache);
	if (FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot))
	{
		RuntimeState->bSuppressedByCurrentWeaponPolicy = true;
		RuntimeState->ExternalAdditionalHitEffects.Reset();
		RuntimeState->ProjectileInheritedExternalAdditionalHitEffects.Reset();
	}
}

void UHeroLoadoutEffectComponent::ResetLoadoutSlotHitEffectRuntime(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	// 单槽位重置要同时收 direct、具名来源和所有 timer，避免来源到期回调继续命中一个已失效槽位。
	ClearLoadoutSlotExternalHitEffectSourceStates(InLoadoutSlot);
	HitEffectRuntimeStatesBySlot.Remove(InLoadoutSlot);
}

void UHeroLoadoutEffectComponent::ResetRuntimeStateForHeroStartup()
{
	TArray<EHeroWeaponLoadoutSlot> LoadoutSlots;
	HitEffectRuntimeStatesBySlot.GetKeys(LoadoutSlots);
	for (const EHeroWeaponLoadoutSlot LoadoutSlot : LoadoutSlots)
	{
		// 组件退场时先清所有槽位的来源状态和 timer，再回收 runtime map。
		ClearLoadoutSlotExternalHitEffectSourceStates(LoadoutSlot);
	}

	HitEffectRuntimeStatesBySlot.Reset();
	CachedHeroCharacter.Reset();
	CachedHeroEquipmentComponent.Reset();
	CachedHeroLoadoutContextComponent.Reset();
	CachedHeroLoadoutRuntimeComponent.Reset();
}

AActionHeroCharacter* UHeroLoadoutEffectComponent::GetOwningHeroCharacter() const
{
	if (CachedHeroCharacter.IsValid())
	{
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedHeroCharacter.Get();
}

UHeroEquipmentComponent* UHeroLoadoutEffectComponent::GetOwningHeroEquipmentComponent() const
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

UHeroLoadoutContextComponent* UHeroLoadoutEffectComponent::GetOwningHeroLoadoutContextComponent() const
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

UHeroLoadoutRuntimeComponent* UHeroLoadoutEffectComponent::GetOwningHeroLoadoutRuntimeComponent() const
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

bool UHeroLoadoutEffectComponent::IsLoadoutSlotRegistered(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	return EquipmentComponent && EquipmentComponent->FindRuntimeState(InLoadoutSlot) != nullptr;
}

FHeroLoadoutHitEffectRuntimeState* UHeroLoadoutEffectComponent::FindHitEffectRuntimeState(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	return HitEffectRuntimeStatesBySlot.Find(InLoadoutSlot);
}

const FHeroLoadoutHitEffectRuntimeState* UHeroLoadoutEffectComponent::FindHitEffectRuntimeState(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	return HitEffectRuntimeStatesBySlot.Find(InLoadoutSlot);
}

FHeroLoadoutHitEffectRuntimeState& UHeroLoadoutEffectComponent::FindOrAddHitEffectRuntimeState(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	return HitEffectRuntimeStatesBySlot.FindOrAdd(InLoadoutSlot);
}

bool UHeroLoadoutEffectComponent::ValidateExternalHitEffects(
	const TArray<FActionHitEffectEntry>& InHitEffects,
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();

	// 这里只校验命中效果条目本身是否合法。
	// 槽位注册、武器 definition 已加载、context 缓存 ready 和武器策略允许与否，都在更外层写入口判断。
	for (int32 HitEffectIndex = 0; HitEffectIndex < InHitEffects.Num(); ++HitEffectIndex)
	{
		if (InHitEffects[HitEffectIndex].IsValidEntry())
		{
			continue;
		}

		OutFailureReason = FString::Printf(
			TEXT("固定武器槽外部额外命中效果配置无效，已拒绝写入。槽位=%s 索引=%d"),
			*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
			HitEffectIndex);
		return false;
	}

	return true;
}

bool UHeroLoadoutEffectComponent::IsLoadoutSlotExternalHitEffectSuppressed(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
	if (!LoadoutContextComponent || !LoadoutContextComponent->HasLoadoutSlotWeaponAttributeCache(InLoadoutSlot))
	{
		return true;
	}

	const FActionWeaponAttributeCacheData* WeaponAttributeCache = nullptr;
	// suppressed 判断只回答“这份 runtime 当前能不能参与聚合”。
	// 来源状态本身不会因此被删除，等武器策略重新允许后仍可恢复参与聚合。
	return !LoadoutContextComponent->TryGetLoadoutSlotWeaponAttributeCache(InLoadoutSlot, WeaponAttributeCache)
		|| !WeaponAttributeCache
		|| !WeaponAttributeCache->bAllowAdditionalHitEffects;
}

void UHeroLoadoutEffectComponent::RebuildLoadoutSlotExternalAdditionalHitEffects(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
	FActionWeaponAttributeCacheData* WeaponAttributeCache = nullptr;
	if (LoadoutContextComponent
		&& LoadoutContextComponent->TryGetMutableLoadoutSlotWeaponAttributeCache(InLoadoutSlot, WeaponAttributeCache)
		&& WeaponAttributeCache)
	{
		// 优先在拿到 context 持有的正式缓存镜像时原位重建，保证聚合结果和 WeaponAttributeCache 始终一致。
		RebuildLoadoutSlotExternalAdditionalHitEffects(InLoadoutSlot, *WeaponAttributeCache);
		return;
	}

	if (FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot))
	{
		RuntimeState->bSuppressedByCurrentWeaponPolicy = true;
		RuntimeState->ExternalAdditionalHitEffects.Reset();
		RuntimeState->ProjectileInheritedExternalAdditionalHitEffects.Reset();
	}
}

void UHeroLoadoutEffectComponent::RebuildLoadoutSlotExternalAdditionalHitEffects(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FActionWeaponAttributeCacheData& InOutAttributeCache)
{
	FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		InOutAttributeCache.DirectExternalAdditionalHitEffects.Reset();
		InOutAttributeCache.ExternalAdditionalHitEffects.Reset();
		InOutAttributeCache.ProjectileInheritedExternalAdditionalHitEffects.Reset();
		return;
	}

	RuntimeState->bSuppressedByCurrentWeaponPolicy = IsLoadoutSlotExternalHitEffectSuppressed(InLoadoutSlot);
	RuntimeState->ExternalAdditionalHitEffects.Reset();
	RuntimeState->ProjectileInheritedExternalAdditionalHitEffects.Reset();

	// 聚合顺序固定为：
	// 1. 先放 direct 层；
	// 2. 再按 apply scope 合并具名来源；
	// 3. 最后把 runtime 正式镜像回 WeaponAttributeCache。
	if (!RuntimeState->bSuppressedByCurrentWeaponPolicy)
	{
		RuntimeState->ExternalAdditionalHitEffects = RuntimeState->DirectExternalAdditionalHitEffects;
		RuntimeState->ProjectileInheritedExternalAdditionalHitEffects =
			RuntimeState->DirectExternalAdditionalHitEffects;

		for (const TPair<FGameplayTag, FActionExternalHitEffectSourceRuntimeState>& SourceStatePair :
			RuntimeState->ExternalHitEffectSourceStates)
		{
			const FActionExternalHitEffectSourceEntry& SourceEntry = SourceStatePair.Value.SourceEntry;
			if (!SourceEntry.IsValidSource() || SourceEntry.HitEffects.Num() <= 0)
			{
				continue;
			}

			if (SourceEntry.AppliesToMelee())
			{
				RuntimeState->ExternalAdditionalHitEffects.Append(SourceEntry.HitEffects);
			}

			if (SourceEntry.AppliesToProjectile())
			{
				RuntimeState->ProjectileInheritedExternalAdditionalHitEffects.Append(SourceEntry.HitEffects);
			}
		}
	}

	RefreshCacheMirrorFromRuntime(RuntimeState, InOutAttributeCache);
}

void UHeroLoadoutEffectComponent::RefreshCacheMirrorFromRuntime(
	const FHeroLoadoutHitEffectRuntimeState* InRuntimeState,
	FActionWeaponAttributeCacheData& InOutAttributeCache) const
{
	if (!InRuntimeState)
	{
		InOutAttributeCache.DirectExternalAdditionalHitEffects.Reset();
		InOutAttributeCache.ExternalAdditionalHitEffects.Reset();
		InOutAttributeCache.ProjectileInheritedExternalAdditionalHitEffects.Reset();
		return;
	}

	// 这里只做 runtime -> cache mirror 的镜像回写，不在 cache 侧再造第二份业务状态。
	InOutAttributeCache.DirectExternalAdditionalHitEffects =
		InRuntimeState->DirectExternalAdditionalHitEffects;
	InOutAttributeCache.ExternalAdditionalHitEffects =
		InRuntimeState->ExternalAdditionalHitEffects;
	InOutAttributeCache.ProjectileInheritedExternalAdditionalHitEffects =
		InRuntimeState->ProjectileInheritedExternalAdditionalHitEffects;
}

void UHeroLoadoutEffectComponent::ClearLoadoutSlotExternalHitEffectSourceTimer(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FGameplayTag InSourceTag)
{
	if (!InSourceTag.IsValid())
	{
		return;
	}

	FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return;
	}

	if (FActionExternalHitEffectSourceRuntimeState* SourceState =
		RuntimeState->ExternalHitEffectSourceStates.Find(InSourceTag))
	{
		// 单来源清 timer 只收这一个 SourceTag 的 duration 生命周期，不动其它来源。
		if (GetWorld() && SourceState->ExpireTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().ClearTimer(SourceState->ExpireTimerHandle);
		}
		SourceState->ClearDurationLimit();
	}
}

void UHeroLoadoutEffectComponent::ClearLoadoutSlotExternalHitEffectSourceTimers(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return;
	}

	for (TPair<FGameplayTag, FActionExternalHitEffectSourceRuntimeState>& SourceStatePair :
		RuntimeState->ExternalHitEffectSourceStates)
	{
		// 槽位级 timer 清理只收 duration 生命周期；来源状态本身继续由外层决定是否删除。
		if (GetWorld() && SourceStatePair.Value.ExpireTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().ClearTimer(SourceStatePair.Value.ExpireTimerHandle);
		}
		SourceStatePair.Value.ClearDurationLimit();
	}
}

void UHeroLoadoutEffectComponent::ClearLoadoutSlotExternalHitEffectSourceStates(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	FHeroLoadoutHitEffectRuntimeState* RuntimeState = FindHitEffectRuntimeState(InLoadoutSlot);
	if (!RuntimeState)
	{
		return;
	}

	// 全清来源状态时，先停所有 timer，再删 source states，最后由外层决定是否移除整个 runtime。
	ClearLoadoutSlotExternalHitEffectSourceTimers(InLoadoutSlot);
	RuntimeState->ExternalHitEffectSourceStates.Reset();
}

void UHeroLoadoutEffectComponent::HandleTimedExternalHitEffectSourceExpired(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FGameplayTag InSourceTag)
{
	UE_LOG(
		LogHeroLoadoutEffectComponent,
		Log,
		TEXT("固定武器槽外部命中效果来源已到期移除。槽位=%s 来源=%s"),
		*HeroEquipmentHitEffectLog::GetWeaponLoadoutSlotName(InLoadoutSlot),
		*InSourceTag.ToString());
	// 到期后统一回到正式 remove 入口，保证 scoped source、timer 和聚合镜像按同一条收尾链处理。
	RemoveLoadoutSlotExternalHitEffectSource(InLoadoutSlot, InSourceTag);
}
