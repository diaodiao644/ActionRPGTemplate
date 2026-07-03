// 文件说明：实现 HeroWeaponBase 相关逻辑。

#include "Items/Weapons/HeroWeaponBase.h"

#include "ActionGameplayTags.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Characters/ActionCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroHitSourceComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroWeaponBase, Log, All);

namespace HeroWeaponDamageRuntime
{
	static const AActionHeroCharacter* ResolveOwnerHeroCharacter(const AHeroWeaponBase* InWeapon)
	{
		return InWeapon
			? Cast<AActionHeroCharacter>(InWeapon->GetOwner())
			: nullptr;
	}

	static bool TryResolveCurrentLoadoutContext(
		const AHeroWeaponBase* InWeapon,
		const UHeroLoadoutContextComponent*& OutLoadoutContextComponent,
		EHeroWeaponLoadoutSlot& OutLoadoutSlot)
	{
		OutLoadoutContextComponent = nullptr;
		OutLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

		const AActionHeroCharacter* OwnerHeroCharacter = ResolveOwnerHeroCharacter(InWeapon);
		const UHeroEquipmentComponent* EquipmentComponent = OwnerHeroCharacter
			? OwnerHeroCharacter->FindComponentByClass<UHeroEquipmentComponent>()
			: nullptr;
		OutLoadoutContextComponent = OwnerHeroCharacter
			? OwnerHeroCharacter->FindComponentByClass<UHeroLoadoutContextComponent>()
			: nullptr;
		if (!EquipmentComponent || !OutLoadoutContextComponent)
		{
			return false;
		}

		OutLoadoutSlot = EquipmentComponent->GetCurrentEquippedLoadoutSlot();
		return true;
	}

	static bool TryGetCurrentWeaponAttributeCache(
		const AHeroWeaponBase* InWeapon,
		FActionWeaponAttributeCacheData& OutWeaponAttributeCache)
	{
		OutWeaponAttributeCache.Reset();

		const UHeroLoadoutContextComponent* LoadoutContextComponent = nullptr;
		EHeroWeaponLoadoutSlot CurrentLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
		if (!TryResolveCurrentLoadoutContext(
			InWeapon,
			LoadoutContextComponent,
			CurrentLoadoutSlot))
		{
			return false;
		}

		return LoadoutContextComponent->GetWeaponAttributeCacheByLoadoutSlot(
			CurrentLoadoutSlot,
			OutWeaponAttributeCache);
	}

	static void BuildCombatAttributeSnapshot(
		const UActionAttributeSetBase* InAttributeSet,
		const FActionWeaponAttributeCacheData* InWeaponAttributeCache,
		FActionCombatAttributeSnapshot& OutAttributeSnapshot)
	{
		OutAttributeSnapshot = FActionCombatAttributeSnapshot();

		const float AttackPowerBonus = InWeaponAttributeCache ? InWeaponAttributeCache->AttackPowerBonus : 0.f;
		const float DefensePowerBonus = InWeaponAttributeCache ? InWeaponAttributeCache->DefensePowerBonus : 0.f;
		const float MaxHealthBonus = InWeaponAttributeCache ? InWeaponAttributeCache->MaxHealthBonus : 0.f;
		const float CriticalChanceBonus = InWeaponAttributeCache ? InWeaponAttributeCache->CriticalChanceBonus : 0.f;
		const float CriticalDamageBonus = InWeaponAttributeCache ? InWeaponAttributeCache->CriticalDamageBonus : 0.f;
		const float OutgoingDamageMultiplierBonus =
			InWeaponAttributeCache ? InWeaponAttributeCache->OutgoingDamageMultiplierBonus : 0.f;
		const float ExtraDamageMultiplierBonus =
			InWeaponAttributeCache ? InWeaponAttributeCache->ExtraDamageMultiplierBonus : 0.f;

		OutAttributeSnapshot.AttackPower = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetAttackPower() : 0.f) + AttackPowerBonus),
			0.f);
		OutAttributeSnapshot.DefensePower = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetDefensePower() : 0.f) + DefensePowerBonus),
			0.f);
		OutAttributeSnapshot.MaxHealth = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetMaxHealth() : 0.f) + MaxHealthBonus),
			0.f);
		OutAttributeSnapshot.OutgoingDamageMultiplier = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetOutgoingDamageMultiplier() : 1.f) + OutgoingDamageMultiplierBonus),
			0.f);
		OutAttributeSnapshot.ExtraDamageMultiplier = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetExtraDamageMultiplier() : 1.f) + ExtraDamageMultiplierBonus),
			0.f);
		OutAttributeSnapshot.CriticalChance = ActionRoundRatioValueToFourPlaces(FMath::Clamp(
			(InAttributeSet ? InAttributeSet->GetCriticalChance() : 0.f) + CriticalChanceBonus,
			0.f,
			100.f));
		OutAttributeSnapshot.CriticalDamage = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetCriticalDamage() : 150.f) + CriticalDamageBonus),
			100.f);
	}

	static FActionDamageContextRuntimeState ResolveDamageContext(const AHeroWeaponBase* InWeapon)
	{
		FActionDamageContextRuntimeState DamageContext;
		DamageContext.AbilityLevel = 1;

		const AActionHeroCharacter* OwnerHeroCharacter = InWeapon
			? Cast<AActionHeroCharacter>(InWeapon->GetOwner())
			: nullptr;
		const UHeroCombatComponent* CombatComponent = OwnerHeroCharacter
			? OwnerHeroCharacter->GetHeroCombatComponent()
			: nullptr;
		if (CombatComponent)
		{
			CombatComponent->TryGetActiveDamageContext(DamageContext);
		}

		DamageContext.AbilityLevel = FMath::Max(DamageContext.AbilityLevel, 1);
		return DamageContext;
	}

	static float ResolveEffectiveKnockbackStrength(
		const FActionWeaponHitConfig& InHitConfig,
		const FActionHitWindowRuntimeConfig* InHitWindowRuntimeConfig)
	{
		float ResolvedKnockbackStrength = InHitConfig.KnockbackStrength;
		if (InHitWindowRuntimeConfig && InHitWindowRuntimeConfig->bOverrideKnockbackStrength)
		{
			ResolvedKnockbackStrength = InHitWindowRuntimeConfig->OverrideKnockbackStrength;
		}

		return FMath::Max(ResolvedKnockbackStrength, 0.f);
	}

}

AHeroWeaponBase::AHeroWeaponBase()
	: Super()
{
}

FGameplayTag AHeroWeaponBase::GetWeaponTag() const
{
	if (WeaponDefinition && WeaponDefinition->WeaponTag.IsValid())
	{
		// 武器标签只允许由武器定义资产提供，避免运行时出现“Actor 字段”和“数据资产字段”分叉。
		return WeaponDefinition->WeaponTag;
	}

	return FGameplayTag();
}

FGameplayTag AHeroWeaponBase::GetWeaponSubtypeTag() const
{
	if (WeaponDefinition)
	{
		return WeaponDefinition->GetWeaponSubtypeTag();
	}

	return FGameplayTag();
}

FActionDamagePayload AHeroWeaponBase::BuildDamagePayloadForTarget(AActor* OtherActor) const
{
	return BuildDamagePayload(OtherActor);
}

FActionDamagePayload AHeroWeaponBase::BuildExecutionDamagePayloadForTarget(AActor* OtherActor) const
{
	FActionDamagePayload DamagePayload;
	if (!WeaponDefinition
		|| !BuildExecutionDamagePayloadFromConfig(
			OtherActor,
			WeaponDefinition->GetExecutionHitConfig(),
			DamagePayload))
	{
		UE_LOG(
			LogHeroWeaponBase,
			Warning,
			TEXT("[GA][Execution][BuildPayload][Weapon] reason=weapon_execution_payload_invalid weapon=%s weapon_definition=%s owner=%s target=%s"),
			*GetNameSafe(this),
			*GetNameSafe(WeaponDefinition),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(OtherActor));
		return FActionDamagePayload();
	}

	return DamagePayload;
}

void AHeroWeaponBase::SetWeaponDefinition(UDataAsset_WeaponDefinition* InWeaponDefinition)
{
	WeaponDefinition = InWeaponDefinition;
}

FActionDamagePayload AHeroWeaponBase::BuildDamagePayload(AActor* OtherActor) const
{
	return BuildDamagePayloadForHitSource(
		OtherActor,
		ActionHitSourceDefaults::GetWeaponCollisionSourceId(),
		WeaponCollisionBox);
}

FActionDamagePayload AHeroWeaponBase::BuildDamagePayloadForHitSource(
	AActor* OtherActor,
	const FName InSourceId,
	const UPrimitiveComponent* InSourceComponent) const
{
	if (!WeaponDefinition)
	{
		return Super::BuildDamagePayloadForHitSource(OtherActor, InSourceId, InSourceComponent);
	}

	if (const AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(GetOwner()))
	{
		if (const UHeroCombatComponent* HeroCombatComponent = OwnerHeroCharacter->GetHeroCombatComponent())
		{
			FActionDamagePayload AttackDamagePayload;
			if (const UHeroAttackComponent* HeroAttackComponent = HeroCombatComponent->GetOwningHeroAttackComponent())
			{
				if (HeroAttackComponent->TryBuildCurrentAttackDamagePayloadForHitSource(
					OtherActor,
					InSourceId,
					InSourceComponent ? InSourceComponent->GetFName() : NAME_None,
					AttackDamagePayload))
				{
					return AttackDamagePayload;
				}
			}
		}
	}

	FActionDamagePayload DamagePayload;
	FActionWeaponHitConfig FallbackHitConfig;
	if (!WeaponDefinition->TryResolveAttackHitConfigByRequestTag(
		ActionGameplayTags::Attack_Request_Default,
		FallbackHitConfig))
	{
		return Super::BuildDamagePayloadForHitSource(OtherActor, InSourceId, InSourceComponent);
	}

	return BuildDamagePayloadFromHitConfig(
		OtherActor,
		FallbackHitConfig,
		DamagePayload,
		InSourceId,
		InSourceComponent ? InSourceComponent->GetFName() : NAME_None)
		? DamagePayload
		: Super::BuildDamagePayloadForHitSource(OtherActor, InSourceId, InSourceComponent);
}

bool AHeroWeaponBase::BuildProjectileDamagePayloadTemplate(
	const FActionWeaponHitConfig& InBaseHitConfig,
	const FActionProjectileConfig& InProjectileConfig,
	FActionDamagePayload& OutDamagePayload) const
{
	OutDamagePayload = FActionDamagePayload();
	const FActionDamageContextRuntimeState DamageContext =
		HeroWeaponDamageRuntime::ResolveDamageContext(this);
	FActionWeaponAttributeCacheData WeaponAttributeCache;
	const bool bHasWeaponAttributeCache =
		HeroWeaponDamageRuntime::TryGetCurrentWeaponAttributeCache(this, WeaponAttributeCache);

	// 发射物命中时，命中效果和伤害语义优先使用发射物自己的配置，
	// 若发射物自己已经配置了直伤骨架，就不再借用近战命中配置。
	if (InProjectileConfig.HasAnyDamageConfig())
	{
		const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetOwner());
		const UActionAttributeSetBase* AttributeSet =
			OwnerCharacter ? OwnerCharacter->GetActionAttributeSet() : nullptr;
		FActionCombatAttributeSnapshot AttributeSnapshot;
		HeroWeaponDamageRuntime::BuildCombatAttributeSnapshot(
			AttributeSet,
			bHasWeaponAttributeCache ? &WeaponAttributeCache : nullptr,
			AttributeSnapshot);

		OutDamagePayload = Super::BuildDamagePayloadForHitSource(
			nullptr,
			ActionHitSourceDefaults::GetProjectileSourceId(),
			nullptr);
		OutDamagePayload.InstigatorActor = GetOwner();
		OutDamagePayload.SourceActor = const_cast<AHeroWeaponBase*>(this);

		bool bDidCritical = false;
		OutDamagePayload.BaseDamage = ActionResolveDrivenDamageValue(
			InProjectileConfig.HealthDamageValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			true,
			bDidCritical);

		bool bIgnoredCritical = false;
		OutDamagePayload.GuardStaminaCost = ActionResolveDrivenDamageValue(
			InProjectileConfig.GuardStaminaCostValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			false,
			bIgnoredCritical);
		OutDamagePayload.PoiseDamage = ActionResolveDrivenDamageValue(
			InProjectileConfig.PoiseDamageValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			false,
			bIgnoredCritical);
		FillWeaponHitSourceInfo(OutDamagePayload.HitSource);
	}
	else if (!BuildDamagePayloadFromHitConfig(nullptr, InBaseHitConfig, OutDamagePayload))
	{
		return false;
	}

	OutDamagePayload.HitTag = InBaseHitConfig.HitTag;
	OutDamagePayload.bCanBeBlocked = InBaseHitConfig.bCanBeBlocked;
	OutDamagePayload.bCanBeParried = InBaseHitConfig.bCanBeParried;
	OutDamagePayload.bCanBePerfectDodged = InBaseHitConfig.bCanBePerfectDodged;
	OutDamagePayload.HitReactType = InBaseHitConfig.HitReactType;
	OutDamagePayload.HitStateDuration = FMath::Max(InBaseHitConfig.HitStateDuration, 0.f);
	OutDamagePayload.KnockbackStrength = HeroWeaponDamageRuntime::ResolveEffectiveKnockbackStrength(
		InBaseHitConfig,
		nullptr);
	OutDamagePayload.SpecialWeaponSwitchEnergyRewardOnHit =
		FMath::Max(InBaseHitConfig.SpecialWeaponSwitchEnergyRewardOnHit, 0.f);
	OutDamagePayload.HitPresentationConfig = InProjectileConfig.HitPresentationConfig;

	OutDamagePayload.DefaultEffects = InProjectileConfig.DefaultEffects;
	OutDamagePayload.AdditionalEffects = InProjectileConfig.AdditionalEffects;
	OutDamagePayload.DamageType = InProjectileConfig.DamageType;
	OutDamagePayload.DamageElementTypeTag = InProjectileConfig.DamageElementTypeTag;
	OutDamagePayload.HitSource.ParentSourceType = OutDamagePayload.HitSource.SourceType;
	OutDamagePayload.HitSource.ParentSourceTag = OutDamagePayload.HitSource.SourceTag;
	OutDamagePayload.HitSource.SourceId = ActionHitSourceDefaults::GetProjectileSourceId();
	OutDamagePayload.HitSource.SourceType = EActionHitSourceType::Projectile;
	OutDamagePayload.HitSource.SourceTag = InProjectileConfig.ProjectileTag;
	OutDamagePayload.HitSource.SourceComponentName = NAME_None;
	OutDamagePayload.HitSource.SourceSocketName = NAME_None;

	const bool bCanInheritExternalAdditionalEffects =
		WeaponDefinition
		&& WeaponDefinition->AllowsAdditionalHitEffects()
		&& InProjectileConfig.bAllowInheritedAdditionalEffects;
	if (!bCanInheritExternalAdditionalEffects)
	{
		OutDamagePayload.ExternalAdditionalEffects.Reset();
	}
	else
	{
		OutDamagePayload.ExternalAdditionalEffects = WeaponAttributeCache.ProjectileInheritedExternalAdditionalHitEffects;
	}

	return OutDamagePayload.IsValidPayload();
}

bool AHeroWeaponBase::BuildDamagePayloadFromHitConfig(
	AActor* OtherActor,
	const FActionWeaponHitConfig& InHitConfig,
	FActionDamagePayload& OutDamagePayload,
	const FName InPreferredSourceId,
	const FName InPreferredSourceComponentName) const
{
	OutDamagePayload = Super::BuildDamagePayloadForHitSource(
		OtherActor,
		InPreferredSourceId != NAME_None ? InPreferredSourceId : ActionHitSourceDefaults::GetWeaponCollisionSourceId(),
		nullptr);
	if (!WeaponDefinition)
	{
		return false;
	}

	const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetOwner());
	const UActionAttributeSetBase* AttributeSet = OwnerCharacter ? OwnerCharacter->GetActionAttributeSet() : nullptr;
	FActionWeaponAttributeCacheData WeaponAttributeCache;
	const bool bHasWeaponAttributeCache =
		HeroWeaponDamageRuntime::TryGetCurrentWeaponAttributeCache(this, WeaponAttributeCache);
	FActionCombatAttributeSnapshot AttributeSnapshot;
	HeroWeaponDamageRuntime::BuildCombatAttributeSnapshot(
		AttributeSet,
		bHasWeaponAttributeCache ? &WeaponAttributeCache : nullptr,
		AttributeSnapshot);
	const FActionDamageContextRuntimeState DamageContext =
		HeroWeaponDamageRuntime::ResolveDamageContext(this);

	// 新框架下由角色攻击力作为主要伤害来源，武器只负责倍率、追加值和特殊属性。
	if (InHitConfig.HasAnyLevelDrivenDamageConfig())
	{
		bool bDidCritical = false;
		OutDamagePayload.BaseDamage = ActionResolveDrivenDamageValue(
			InHitConfig.HealthDamageValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			true,
			bDidCritical);

		bool bIgnoredCritical = false;
		OutDamagePayload.GuardStaminaCost = ActionResolveDrivenDamageValue(
			InHitConfig.GuardStaminaCostValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			false,
			bIgnoredCritical);
		OutDamagePayload.PoiseDamage = ActionResolveDrivenDamageValue(
			InHitConfig.PoiseDamageValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			false,
			bIgnoredCritical);
	}
	OutDamagePayload.SpecialWeaponSwitchEnergyRewardOnHit =
		FMath::Max(InHitConfig.SpecialWeaponSwitchEnergyRewardOnHit, 0.f);
	OutDamagePayload.HitTag = InHitConfig.HitTag;
	OutDamagePayload.DamageType = WeaponDefinition->GetDamageType();
	OutDamagePayload.DamageElementTypeTag = WeaponDefinition->GetDamageElementTypeTag();
	OutDamagePayload.bAllowAdditionalHitEffects = WeaponDefinition->AllowsAdditionalHitEffects();
	FillWeaponHitSourceInfo(
		OutDamagePayload.HitSource,
		InPreferredSourceId,
		InPreferredSourceComponentName);
	OutDamagePayload.DefaultEffects = InHitConfig.DefaultEffects;
	OutDamagePayload.AdditionalEffects = InHitConfig.AdditionalEffects;
	OutDamagePayload.HitPresentationConfig = InHitConfig.HitPresentationConfig;
	OutDamagePayload.ExternalAdditionalEffects = WeaponAttributeCache.ExternalAdditionalHitEffects;
	OutDamagePayload.bCanBeBlocked = InHitConfig.bCanBeBlocked;
	OutDamagePayload.bCanBeParried = InHitConfig.bCanBeParried;
	OutDamagePayload.bCanBePerfectDodged = InHitConfig.bCanBePerfectDodged;
	OutDamagePayload.HitReactType = InHitConfig.HitReactType;
	OutDamagePayload.HitStateDuration = FMath::Max(InHitConfig.HitStateDuration, 0.f);
	OutDamagePayload.KnockbackStrength = HeroWeaponDamageRuntime::ResolveEffectiveKnockbackStrength(
		InHitConfig,
		&ActiveHitWindowRuntimeConfig);
	return OutDamagePayload.IsValidPayload();
}

bool AHeroWeaponBase::BuildExecutionDamagePayloadFromConfig(
	AActor* OtherActor,
	const FActionExecutionHitConfig& InHitConfig,
	FActionDamagePayload& OutDamagePayload) const
{
	OutDamagePayload = Super::BuildDamagePayloadForHitSource(
		OtherActor,
		ActionHitSourceDefaults::GetExecutionSourceId(),
		nullptr);
	if (!WeaponDefinition)
	{
		return false;
	}

	const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetOwner());
	const UActionAttributeSetBase* AttributeSet = OwnerCharacter ? OwnerCharacter->GetActionAttributeSet() : nullptr;
	if (!AttributeSet)
	{
		return false;
	}

	FActionWeaponAttributeCacheData WeaponAttributeCache;
	const bool bHasWeaponAttributeCache =
		HeroWeaponDamageRuntime::TryGetCurrentWeaponAttributeCache(this, WeaponAttributeCache);
	FActionCombatAttributeSnapshot AttributeSnapshot;
	HeroWeaponDamageRuntime::BuildCombatAttributeSnapshot(
		AttributeSet,
		bHasWeaponAttributeCache ? &WeaponAttributeCache : nullptr,
		AttributeSnapshot);
	const FActionDamageContextRuntimeState DamageContext =
		HeroWeaponDamageRuntime::ResolveDamageContext(this);

	OutDamagePayload.InstigatorActor = GetOwner();
	OutDamagePayload.SourceActor = const_cast<AHeroWeaponBase*>(this);
	OutDamagePayload.DamageType = WeaponDefinition->GetDamageType();
	OutDamagePayload.DamageElementTypeTag = WeaponDefinition->GetDamageElementTypeTag();

	if (InHitConfig.HasAnyLevelDrivenDamageConfig())
	{
		bool bDidCritical = false;
		OutDamagePayload.BaseDamage = ActionResolveDrivenDamageValue(
			InHitConfig.HealthDamageValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			true,
			bDidCritical);

		bool bIgnoredCritical = false;
		OutDamagePayload.PoiseDamage = ActionResolveDrivenDamageValue(
			InHitConfig.PoiseDamageValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			false,
			bIgnoredCritical);
	}

	InHitConfig.ApplySharedExecutionPayloadSettings(OutDamagePayload);
	OutDamagePayload.ImpactDirection = OtherActor
		? (OtherActor->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal()
		: FVector::ZeroVector;
	OutDamagePayload.HitSource.SourceId = ActionHitSourceDefaults::GetExecutionSourceId();
	OutDamagePayload.HitSource.SourceType = EActionHitSourceType::Execution;
	OutDamagePayload.HitSource.SourceComponentName = TEXT("Execution");
	OutDamagePayload.HitSource.SourceTag = WeaponDefinition->WeaponTag;
	OutDamagePayload.HitSource.WeaponTag = WeaponDefinition->WeaponTag;
	return OutDamagePayload.IsValidPayload();
}

void AHeroWeaponBase::FillWeaponHitSourceInfo(
	FActionHitSourceInfo& InOutHitSourceInfo,
	const FName InPreferredSourceId,
	const FName InPreferredSourceComponentName) const
{
	const FName SourceId = InPreferredSourceId != NAME_None
		? InPreferredSourceId
		: ActionHitSourceDefaults::GetWeaponCollisionSourceId();
	bool bFilledFromRegistration = false;
	if (const AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(GetOwner()))
	{
		if (const UHeroHitSourceComponent* HitSourceComponent = OwnerHeroCharacter->GetHeroHitSourceComponent())
		{
			if (InPreferredSourceId != NAME_None)
			{
				bFilledFromRegistration = HitSourceComponent->TryFillHitSourceInfoFromRegistration(
					InPreferredSourceId,
					InOutHitSourceInfo);
			}
			else
			{
				FActionHitSourceDefinition ActiveWeaponHitSourceDefinition;
				if (HitSourceComponent->ShouldUseWeaponCollisionDetection()
					&& HitSourceComponent->TryGetFirstActiveHitSourceByType(
						EActionHitSourceType::WeaponCollision,
						ActiveWeaponHitSourceDefinition))
				{
					bFilledFromRegistration = HitSourceComponent->TryFillHitSourceInfoFromRegistration(
						ActiveWeaponHitSourceDefinition.SourceId,
						InOutHitSourceInfo);
				}

				if (!bFilledFromRegistration)
				{
					bFilledFromRegistration = HitSourceComponent->TryFillHitSourceInfoFromRegistration(SourceId, InOutHitSourceInfo);
				}
			}
		}
	}

	if (!bFilledFromRegistration)
	{
		InOutHitSourceInfo.SourceId = SourceId;
		InOutHitSourceInfo.SourceType = EActionHitSourceType::WeaponCollision;
	}

	InOutHitSourceInfo.WeaponTag = GetWeaponTag();
	if (!InOutHitSourceInfo.SourceTag.IsValid())
	{
		InOutHitSourceInfo.SourceTag = GetWeaponTag();
	}
	if (InPreferredSourceComponentName != NAME_None)
	{
		InOutHitSourceInfo.SourceComponentName = InPreferredSourceComponentName;
	}
	else
	{
		InOutHitSourceInfo.SourceComponentName = WeaponCollisionBox
			? WeaponCollisionBox->GetFName()
			: NAME_None;
	}

	if (const AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(GetOwner()))
	{
		if (const UHeroEquipmentComponent* EquipmentComponent = OwnerHeroCharacter->FindComponentByClass<UHeroEquipmentComponent>())
		{
			InOutHitSourceInfo.LoadoutSlot = EquipmentComponent->GetCurrentEquippedLoadoutSlot();
		}
	}
}

void AHeroWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}
