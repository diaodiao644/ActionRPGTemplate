// 文件说明：实现 ActionAttributeSetBase 相关逻辑。

#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"

#include "ActionGameplayTags.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace ActionAttributeSetRuntime
{
	static const AActionHeroCharacter* ResolveOwningHeroCharacter(const UActionAttributeSetBase* InAttributeSet)
	{
		if (!InAttributeSet)
		{
			return nullptr;
		}

		const UAbilitySystemComponent* OwnerASC = InAttributeSet->GetOwningAbilitySystemComponent();
		return OwnerASC
			? Cast<AActionHeroCharacter>(OwnerASC->GetAvatarActor())
			: nullptr;
	}

	static bool TryResolveCurrentLoadoutContext(
		const UActionAttributeSetBase* InAttributeSet,
		const UHeroLoadoutContextComponent*& OutLoadoutContextComponent,
		EHeroWeaponLoadoutSlot& OutLoadoutSlot)
	{
		OutLoadoutContextComponent = nullptr;
		OutLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

		const AActionHeroCharacter* HeroCharacter = ResolveOwningHeroCharacter(InAttributeSet);
		const UHeroEquipmentComponent* EquipmentComponent = HeroCharacter
			? HeroCharacter->FindComponentByClass<UHeroEquipmentComponent>()
			: nullptr;
		OutLoadoutContextComponent = HeroCharacter
			? HeroCharacter->FindComponentByClass<UHeroLoadoutContextComponent>()
			: nullptr;
		if (!EquipmentComponent || !OutLoadoutContextComponent)
		{
			return false;
		}

		OutLoadoutSlot = EquipmentComponent->GetCurrentEquippedLoadoutSlot();
		return true;
	}

	static bool TryGetCurrentWeaponAttributeCache(
		const UActionAttributeSetBase* AttributeSet,
		FActionWeaponAttributeCacheData& OutWeaponAttributeCache)
	{
		OutWeaponAttributeCache.Reset();

		if (!AttributeSet)
		{
			return false;
		}

		const UHeroLoadoutContextComponent* LoadoutContextComponent = nullptr;
		EHeroWeaponLoadoutSlot CurrentLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
		if (!TryResolveCurrentLoadoutContext(
			AttributeSet,
			LoadoutContextComponent,
			CurrentLoadoutSlot))
		{
			return false;
		}

		return LoadoutContextComponent->GetWeaponAttributeCacheByLoadoutSlot(
			CurrentLoadoutSlot,
			OutWeaponAttributeCache);
	}

	static float ResolveMaxHealth(
		const UActionAttributeSetBase* AttributeSet,
		const TOptional<float> OverrideBaseValue = TOptional<float>())
	{
		const float BaseValue = OverrideBaseValue.IsSet()
			? OverrideBaseValue.GetValue()
			: (AttributeSet ? AttributeSet->GetMaxHealth() : 0.f);
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.MaxHealthBonus
			: 0.f;
		return FMath::Max(ActionRoundBaseValueToInteger(BaseValue + BonusValue), 1.f);
	}

	static float ResolveMaxStamina(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetMaxStamina() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.MaxStaminaBonus
			: 0.f;
		return FMath::Max(ActionRoundBaseValueToInteger(BaseValue + BonusValue), 1.f);
	}

	static float ResolveMaxPoise(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetMaxPoise() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.MaxPoiseBonus
			: 0.f;
		return FMath::Max(ActionRoundBaseValueToInteger(BaseValue + BonusValue), 0.f);
	}

	static float ResolveDefensePower(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetDefensePower() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.DefensePowerBonus
			: 0.f;
		return FMath::Max(ActionRoundBaseValueToInteger(BaseValue + BonusValue), 0.f);
	}

	static float ResolveGuardDefense(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetGuardDefense() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.GuardDefenseBonus
			: 0.f;
		return FMath::Max(ActionRoundBaseValueToInteger(BaseValue + BonusValue), 0.f);
	}

	static float ResolveDamageReduction(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetDamageReduction() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.DamageReductionBonus
			: 0.f;
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(BaseValue + BonusValue, 0.f, 90.f));
	}

	static float ResolveHealthDamageResistance(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetHealthDamageResistance() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.HealthDamageResistanceBonus
			: 0.f;
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(BaseValue + BonusValue, 0.f, 95.f));
	}

	static float ResolveGuardStaminaCostResistance(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetGuardStaminaCostResistance() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.GuardStaminaCostResistanceBonus
			: 0.f;
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(BaseValue + BonusValue, 0.f, 95.f));
	}

	static float ResolvePoiseDefense(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetPoiseDefense() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.PoiseDefenseBonus
			: 0.f;
		return FMath::Max(ActionRoundBaseValueToInteger(BaseValue + BonusValue), 0.f);
	}

	static float ResolvePoiseDamageResistance(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetPoiseDamageResistance() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.PoiseDamageResistanceBonus
			: 0.f;
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(BaseValue + BonusValue, 0.f, 95.f));
	}

	static float ResolveDamageVulnerability(const UActionAttributeSetBase* AttributeSet)
	{
		const float BaseValue = AttributeSet ? AttributeSet->GetDamageVulnerability() : 0.f;
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const float BonusValue = TryGetCurrentWeaponAttributeCache(AttributeSet, WeaponAttributeCache)
			? WeaponAttributeCache.DamageVulnerabilityBonus
			: 0.f;
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(BaseValue + BonusValue, 0.f, 500.f));
	}
}

UActionAttributeSetBase::UActionAttributeSetBase()
{
}

void UActionAttributeSetBase::InitBindAttributeChangeDelegate()
{
	if (UAbilitySystemComponent* OwnerASC = GetOwningAbilitySystemComponent())
	{
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetHealthAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetMaxHealthAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetShieldAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetStaminaAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetMaxStaminaAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetEnergyAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetMaxEnergyAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetSpecialWeaponSwitchEnergyAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetMaxSpecialWeaponSwitchEnergyAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetPoiseAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
		OwnerASC->GetGameplayAttributeValueChangeDelegate(GetMaxPoiseAttribute()).AddUObject(this, &UActionAttributeSetBase::OnRep_OnAttributeChange);
	}

}

void UActionAttributeSetBase::OnRep_OnAttributeChange(const FOnAttributeChangeData& Data)
{
	OnAttributeChangeDelegate.Broadcast(Data);
}

void UActionAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, SpecialWeaponSwitchEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, MaxSpecialWeaponSwitchEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, CriticalChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, CriticalDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, OutgoingDamageMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, ExtraDamageMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, DefensePower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, GuardDefense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, DamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, HealthDamageResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, GuardStaminaCostResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, PoiseDamageResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, MaxPoise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, PoiseDefense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, DamageVulnerability, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, StaminaRecovery, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, EnergyRecovery, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActionAttributeSetBase, ExecutionDamageMultiplier, COND_None, REPNOTIFY_Always);
}

void UActionAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());

		if (UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent())
		{
			const float ResolvedNewMaxHealth = ActionAttributeSetRuntime::ResolveMaxHealth(this, TOptional<float>(NewValue));
			const float CurrentShield = GetShield();
			if (CurrentShield > ResolvedNewMaxHealth)
			{
				const float NewDelta = ActionRoundBaseValueToInteger(ResolvedNewMaxHealth) - CurrentShield;
				AbilitySystemComponent->ApplyModToAttributeUnsafe(
					GetShieldAttribute(),
					EGameplayModOp::Additive,
					NewDelta);
			}
		}
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		AdjustAttributeForMaxChange(Stamina, MaxStamina, NewValue, GetStaminaAttribute());
	}
	else if (Attribute == GetMaxEnergyAttribute())
	{
		AdjustAttributeForMaxChange(Energy, MaxEnergy, NewValue, GetEnergyAttribute());
	}
	else if (Attribute == GetMaxSpecialWeaponSwitchEnergyAttribute())
	{
		AdjustAttributeForMaxChange(
			SpecialWeaponSwitchEnergy,
			MaxSpecialWeaponSwitchEnergy,
			NewValue,
			GetSpecialWeaponSwitchEnergyAttribute());
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		AdjustAttributeForMaxChange(Poise, MaxPoise, NewValue, GetPoiseAttribute());
	}

	NewValue = ClampAttributeValue(Attribute, NewValue);
}

void UActionAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 这里专门处理“受击相关元属性”，真正的生命/体力/韧性落地公式仍然收口在 AttributeSet 内部。
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		HandleIncomingDamage(Data);
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingGuardStaminaCostAttribute())
	{
		HandleIncomingGuardStaminaCost();
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingPoiseDamageAttribute())
	{
		HandleIncomingPoiseDamage();
	}

	ClampCurrentAttributes();
}

bool UActionAttributeSetBase::IsAlive() const
{
	return GetHealth() > 0.f;
}

float UActionAttributeSetBase::GetHealthPercent() const
{
	const float ResolvedMaxHealth = ActionAttributeSetRuntime::ResolveMaxHealth(this);
	return ResolvedMaxHealth > 0.f ? GetHealth() / ResolvedMaxHealth : 0.f;
}

float UActionAttributeSetBase::GetShieldPercent() const
{
	const float ResolvedMaxHealth = ActionAttributeSetRuntime::ResolveMaxHealth(this);
	return ResolvedMaxHealth > 0.f ? GetShield() / ResolvedMaxHealth : 0.f;
}

float UActionAttributeSetBase::GetStaminaPercent() const
{
	const float ResolvedMaxStamina = ActionAttributeSetRuntime::ResolveMaxStamina(this);
	return ResolvedMaxStamina > 0.f ? GetStamina() / ResolvedMaxStamina : 0.f;
}

float UActionAttributeSetBase::GetEnergyPercent() const
{
	return GetMaxEnergy() > 0.f ? GetEnergy() / GetMaxEnergy() : 0.f;
}

float UActionAttributeSetBase::GetSpecialWeaponSwitchEnergyPercent() const
{
	return GetMaxSpecialWeaponSwitchEnergy() > 0.f
		? GetSpecialWeaponSwitchEnergy() / GetMaxSpecialWeaponSwitchEnergy()
		: 0.f;
}

float UActionAttributeSetBase::GetPoisePercent() const
{
	const float ResolvedMaxPoise = ActionAttributeSetRuntime::ResolveMaxPoise(this);
	return ResolvedMaxPoise > 0.f ? GetPoise() / ResolvedMaxPoise : 0.f;
}

bool UActionAttributeSetBase::IsEnergyFull() const
{
	return GetMaxEnergy() > 0.f && GetEnergy() >= GetMaxEnergy();
}

bool UActionAttributeSetBase::IsSpecialWeaponSwitchEnergyFull() const
{
	return GetMaxSpecialWeaponSwitchEnergy() > 0.f
		&& GetSpecialWeaponSwitchEnergy() >= GetMaxSpecialWeaponSwitchEnergy();
}

float UActionAttributeSetBase::ApplyHealthDamage(
	const float RawDamage,
	const float FinalDamageMultiplier,
	const float IgnoredFinalDamageReductionPercent)
{
	const float FinalDamage = CalculateHealthDamage(RawDamage, FinalDamageMultiplier, IgnoredFinalDamageReductionPercent);
	if (FinalDamage <= 0.f)
	{
		return 0.f;
	}

	SetHealth(ActionRoundBaseValueToInteger(GetHealth() - FinalDamage));
	ClampCurrentAttributes();
	return FinalDamage;
}

float UActionAttributeSetBase::ApplyGuardStaminaCost(float RawGuardStaminaCost)
{
	const float FinalCost = CalculateGuardStaminaCost(RawGuardStaminaCost);
	if (FinalCost <= 0.f)
	{
		return 0.f;
	}

	SetStamina(ActionRoundBaseValueToInteger(GetStamina() - FinalCost));
	ClampCurrentAttributes();
	return FinalCost;
}

float UActionAttributeSetBase::ApplyPoiseDamage(float RawPoiseDamage)
{
	const float FinalDamage = CalculatePoiseDamage(RawPoiseDamage);
	if (FinalDamage <= 0.f)
	{
		return 0.f;
	}

	SetPoise(ActionRoundBaseValueToInteger(GetPoise() - FinalDamage));
	ClampCurrentAttributes();
	return FinalDamage;
}

float UActionAttributeSetBase::CalculateHealthDamage(
	const float RawDamage,
	const float FinalDamageMultiplier,
	const float IgnoredFinalDamageReductionPercent) const
{
	if (RawDamage <= 0.f)
	{
		return 0.f;
	}

	const float ResolvedDefensePower = ActionAttributeSetRuntime::ResolveDefensePower(this);
	const float BaseDamageReductionPercent = ActionRoundRatioValueToFourPlaces(
		ResolvedDefensePower > 0.f ? ResolvedDefensePower / (ResolvedDefensePower + 1000.f) * 100.f : 0.f);
	const float EffectiveDamageReductionPercent = ActionRoundRatioValueToFourPlaces(FMath::Clamp(
		BaseDamageReductionPercent
			+ ActionAttributeSetRuntime::ResolveDamageReduction(this)
			- FMath::Max(ActionRoundRatioValueToFourPlaces(IgnoredFinalDamageReductionPercent), 0.f),
		0.f,
		95.f));
	const float DamageReductionMultiplier = 1.f - EffectiveDamageReductionPercent / 100.f;
	const float HealthResistanceMultiplier = 1.f - ActionAttributeSetRuntime::ResolveHealthDamageResistance(this) / 100.f;
	const float VulnerabilityMultiplier = 1.f + ActionAttributeSetRuntime::ResolveDamageVulnerability(this) / 100.f;
	const float ExecutionMultiplier = FMath::Max(ActionRoundRatioValueToFourPlaces(FinalDamageMultiplier), 0.f);
	return ActionRoundPositiveBaseValueToInteger(
		RawDamage * DamageReductionMultiplier * HealthResistanceMultiplier * VulnerabilityMultiplier * ExecutionMultiplier);
}

float UActionAttributeSetBase::CalculateGuardStaminaCost(float RawGuardStaminaCost) const
{
	if (RawGuardStaminaCost <= 0.f)
	{
		return 0.f;
	}

	// 防御强度越高，被格挡后的体力损失越低。
	return ActionRoundPositiveBaseValueToInteger(
		FMath::Max(RawGuardStaminaCost - ActionAttributeSetRuntime::ResolveGuardDefense(this), 0.f)
		* (1.f - ActionAttributeSetRuntime::ResolveGuardStaminaCostResistance(this) / 100.f));
}

float UActionAttributeSetBase::CalculatePoiseDamage(float RawPoiseDamage) const
{
	if (RawPoiseDamage <= 0.f)
	{
		return 0.f;
	}

	// 削韧防御越高，越不容易被打出硬直。
	return ActionRoundPositiveBaseValueToInteger(
		FMath::Max(RawPoiseDamage - ActionAttributeSetRuntime::ResolvePoiseDefense(this), 0.f)
		* (1.f - ActionAttributeSetRuntime::ResolvePoiseDamageResistance(this) / 100.f));
}

float UActionAttributeSetBase::AddEnergyValue(float DeltaEnergy)
{
	const float PreviousEnergy = GetEnergy();
	SetEnergy(ActionRoundBaseValueToInteger(FMath::Clamp(
		PreviousEnergy + ActionRoundPositiveBaseValueToInteger(DeltaEnergy),
		0.f,
		GetMaxEnergy())));
	return GetEnergy() - PreviousEnergy;
}

float UActionAttributeSetBase::ConsumeEnergyValue(float DeltaEnergy)
{
	const float PreviousEnergy = GetEnergy();
	SetEnergy(ActionRoundBaseValueToInteger(FMath::Clamp(
		PreviousEnergy - ActionRoundPositiveBaseValueToInteger(DeltaEnergy),
		0.f,
		GetMaxEnergy())));
	return PreviousEnergy - GetEnergy();
}

float UActionAttributeSetBase::AddSpecialWeaponSwitchEnergyValue(float DeltaEnergy)
{
	const float PreviousEnergy = GetSpecialWeaponSwitchEnergy();
	SetSpecialWeaponSwitchEnergy(ActionRoundBaseValueToInteger(FMath::Clamp(
		PreviousEnergy + ActionRoundPositiveBaseValueToInteger(DeltaEnergy),
		0.f,
		GetMaxSpecialWeaponSwitchEnergy())));
	return GetSpecialWeaponSwitchEnergy() - PreviousEnergy;
}

float UActionAttributeSetBase::ConsumeSpecialWeaponSwitchEnergyValue(float DeltaEnergy)
{
	const float PreviousEnergy = GetSpecialWeaponSwitchEnergy();
	SetSpecialWeaponSwitchEnergy(ActionRoundBaseValueToInteger(FMath::Clamp(
		PreviousEnergy - ActionRoundPositiveBaseValueToInteger(DeltaEnergy),
		0.f,
		GetMaxSpecialWeaponSwitchEnergy())));
	return PreviousEnergy - GetSpecialWeaponSwitchEnergy();
}

void UActionAttributeSetBase::ClampCurrentValuesToResolvedMaximums()
{
	ClampCurrentAttributes();
}

void UActionAttributeSetBase::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty) const
{
	UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();

	if (!AbilitySystemComponent || FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue))
	{
		return;
	}

	const float CurrentValue = AffectedAttribute.GetCurrentValue();
	const float NewDelta = CurrentMaxValue > 0.f ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;

	AbilitySystemComponent->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
}

float UActionAttributeSetBase::ClampAttributeValue(const FGameplayAttribute& Attribute, float NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		return ActionRoundBaseValueToInteger(FMath::Clamp(NewValue, 0.f, ActionAttributeSetRuntime::ResolveMaxHealth(this)));
	}

	if (Attribute == GetMaxHealthAttribute())
	{
		return FMath::Max(ActionRoundBaseValueToInteger(NewValue), 1.f);
	}

	if (Attribute == GetShieldAttribute())
	{
		return ActionRoundBaseValueToInteger(FMath::Clamp(NewValue, 0.f, ActionAttributeSetRuntime::ResolveMaxHealth(this)));
	}

	if (Attribute == GetStaminaAttribute())
	{
		return ActionRoundBaseValueToInteger(FMath::Clamp(NewValue, 0.f, ActionAttributeSetRuntime::ResolveMaxStamina(this)));
	}

	if (Attribute == GetMaxStaminaAttribute())
	{
		return FMath::Max(ActionRoundBaseValueToInteger(NewValue), 1.f);
	}

	if (Attribute == GetEnergyAttribute())
	{
		return ActionRoundBaseValueToInteger(FMath::Clamp(NewValue, 0.f, GetMaxEnergy()));
	}

	if (Attribute == GetMaxEnergyAttribute())
	{
		return FMath::Max(ActionRoundBaseValueToInteger(NewValue), 0.f);
	}

	if (Attribute == GetSpecialWeaponSwitchEnergyAttribute())
	{
		return ActionRoundBaseValueToInteger(FMath::Clamp(NewValue, 0.f, GetMaxSpecialWeaponSwitchEnergy()));
	}

	if (Attribute == GetMaxSpecialWeaponSwitchEnergyAttribute())
	{
		return FMath::Max(ActionRoundBaseValueToInteger(NewValue), 0.f);
	}

	if (Attribute == GetPoiseAttribute())
	{
		return ActionRoundBaseValueToInteger(FMath::Clamp(NewValue, 0.f, ActionAttributeSetRuntime::ResolveMaxPoise(this)));
	}

	if (Attribute == GetMaxPoiseAttribute())
	{
		return FMath::Max(ActionRoundBaseValueToInteger(NewValue), 0.f);
	}

	if (Attribute == GetCriticalChanceAttribute())
	{
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(NewValue, 0.f, 100.f));
	}

	if (Attribute == GetCriticalDamageAttribute())
	{
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(NewValue, 100.f, 1000.f));
	}

	if (Attribute == GetOutgoingDamageMultiplierAttribute()
		|| Attribute == GetExtraDamageMultiplierAttribute())
	{
		return FMath::Max(ActionRoundRatioValueToFourPlaces(NewValue), 0.f);
	}

	if (Attribute == GetDamageReductionAttribute())
	{
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(NewValue, 0.f, 90.f));
	}

	if (Attribute == GetHealthDamageResistanceAttribute()
		|| Attribute == GetGuardStaminaCostResistanceAttribute()
		|| Attribute == GetPoiseDamageResistanceAttribute())
	{
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(NewValue, 0.f, 95.f));
	}

	if (Attribute == GetDamageVulnerabilityAttribute())
	{
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(NewValue, 0.f, 500.f));
	}

	if (Attribute == GetExecutionDamageMultiplierAttribute())
	{
		return ActionRoundRatioValueToFourPlaces(FMath::Clamp(NewValue, 0.f, 100.f));
	}

	if (Attribute == GetMoveSpeedAttribute())
	{
		return ActionRoundBaseValueToInteger(FMath::Clamp(NewValue, 0.f, 2000.f));
	}

	if (Attribute == GetAttackPowerAttribute()
		|| Attribute == GetDefensePowerAttribute()
		|| Attribute == GetGuardDefenseAttribute()
		|| Attribute == GetPoiseDefenseAttribute()
		|| Attribute == GetStaminaRecoveryAttribute()
		|| Attribute == GetEnergyRecoveryAttribute())
	{
		return FMath::Max(ActionRoundBaseValueToInteger(NewValue), 0.f);
	}

	if (Attribute == GetIncomingDamageAttribute()
		|| Attribute == GetIncomingGuardStaminaCostAttribute()
		|| Attribute == GetIncomingPoiseDamageAttribute())
	{
		return ActionRoundPositiveBaseValueToInteger(NewValue);
	}

	return NewValue;
}

void UActionAttributeSetBase::ClampCurrentAttributes()
{
	SetHealth(ActionRoundBaseValueToInteger(FMath::Clamp(GetHealth(), 0.f, ActionAttributeSetRuntime::ResolveMaxHealth(this))));
	SetShield(ActionRoundBaseValueToInteger(FMath::Clamp(GetShield(), 0.f, ActionAttributeSetRuntime::ResolveMaxHealth(this))));
	SetStamina(ActionRoundBaseValueToInteger(FMath::Clamp(GetStamina(), 0.f, ActionAttributeSetRuntime::ResolveMaxStamina(this))));
	SetEnergy(ActionRoundBaseValueToInteger(FMath::Clamp(GetEnergy(), 0.f, GetMaxEnergy())));
	SetSpecialWeaponSwitchEnergy(ActionRoundBaseValueToInteger(FMath::Clamp(
		GetSpecialWeaponSwitchEnergy(),
		0.f,
		GetMaxSpecialWeaponSwitchEnergy())));
	SetPoise(ActionRoundBaseValueToInteger(FMath::Clamp(GetPoise(), 0.f, ActionAttributeSetRuntime::ResolveMaxPoise(this))));
}

void UActionAttributeSetBase::HandleIncomingDamage(const FGameplayEffectModCallbackData& Data)
{
	// 普通生命伤害与处决伤害统一收口到这里。
	// 先按既有公式算出最终生命伤害，再由护盾优先吸收；只有剩余值才真正落到生命。
	const float RawDamage = ActionRoundPositiveBaseValueToInteger(GetIncomingDamage());
	SetIncomingDamage(0.f);

	const float IncomingExecutionMultiplier = FMath::Max(ActionRoundRatioValueToFourPlaces(
		Data.EffectSpec.GetSetByCallerMagnitude(
			ActionGameplayTags::SetByCaller_Damage_ExecutionMultiplier,
			false,
			1.f)),
		0.f);
	const float IgnoredFinalDamageReductionPercent = ActionRoundRatioValueToFourPlaces(FMath::Clamp(
		Data.EffectSpec.GetSetByCallerMagnitude(
			ActionGameplayTags::SetByCaller_Damage_IgnoreFinalDamageReductionPercent,
			false,
			0.f),
		0.f,
		100.f));
	const float FinalDamage = CalculateHealthDamage(
		RawDamage,
		IncomingExecutionMultiplier,
		IgnoredFinalDamageReductionPercent);
	if (FinalDamage <= 0.f)
	{
		return;
	}

	const float CurrentShield = GetShield();
	const float ShieldAbsorbedDamage = FMath::Min(CurrentShield, FinalDamage);
	if (ShieldAbsorbedDamage > 0.f)
	{
		SetShield(ActionRoundBaseValueToInteger(CurrentShield - ShieldAbsorbedDamage));
	}

	const float RemainingHealthDamage = FinalDamage - ShieldAbsorbedDamage;
	if (RemainingHealthDamage > 0.f)
	{
		SetHealth(ActionRoundBaseValueToInteger(GetHealth() - RemainingHealthDamage));
	}
}

void UActionAttributeSetBase::HandleIncomingGuardStaminaCost()
{
	// 只有成功格挡时才会写入该元属性，因此这里明确走格挡体力损耗公式。
	const float RawGuardStaminaCost = ActionRoundPositiveBaseValueToInteger(GetIncomingGuardStaminaCost());
	SetIncomingGuardStaminaCost(0.f);
	ApplyGuardStaminaCost(RawGuardStaminaCost);
}

void UActionAttributeSetBase::HandleIncomingPoiseDamage()
{
	// 削韧伤害单独落地，方便后续给失衡、处决窗口等系统复用同一条入口。
	const float RawPoiseDamage = ActionRoundPositiveBaseValueToInteger(GetIncomingPoiseDamage());
	SetIncomingPoiseDamage(0.f);
	ApplyPoiseDamage(RawPoiseDamage);
}

void UActionAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, Health, OldValue);
}

void UActionAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, MaxHealth, OldValue);
}

void UActionAttributeSetBase::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, Shield, OldValue);
}

void UActionAttributeSetBase::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, Stamina, OldValue);
}

void UActionAttributeSetBase::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, MaxStamina, OldValue);
}

void UActionAttributeSetBase::OnRep_Energy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, Energy, OldValue);
}

void UActionAttributeSetBase::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, MaxEnergy, OldValue);
}

void UActionAttributeSetBase::OnRep_SpecialWeaponSwitchEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, SpecialWeaponSwitchEnergy, OldValue);
}

void UActionAttributeSetBase::OnRep_MaxSpecialWeaponSwitchEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, MaxSpecialWeaponSwitchEnergy, OldValue);
}

void UActionAttributeSetBase::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, AttackPower, OldValue);
}

void UActionAttributeSetBase::OnRep_CriticalChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, CriticalChance, OldValue);
}

void UActionAttributeSetBase::OnRep_CriticalDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, CriticalDamage, OldValue);
}

void UActionAttributeSetBase::OnRep_OutgoingDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, OutgoingDamageMultiplier, OldValue);
}

void UActionAttributeSetBase::OnRep_ExtraDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, ExtraDamageMultiplier, OldValue);
}

void UActionAttributeSetBase::OnRep_DefensePower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, DefensePower, OldValue);
}

void UActionAttributeSetBase::OnRep_GuardDefense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, GuardDefense, OldValue);
}

void UActionAttributeSetBase::OnRep_DamageReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, DamageReduction, OldValue);
}

void UActionAttributeSetBase::OnRep_HealthDamageResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, HealthDamageResistance, OldValue);
}

void UActionAttributeSetBase::OnRep_GuardStaminaCostResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, GuardStaminaCostResistance, OldValue);
}

void UActionAttributeSetBase::OnRep_PoiseDamageResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, PoiseDamageResistance, OldValue);
}

void UActionAttributeSetBase::OnRep_Poise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, Poise, OldValue);
}

void UActionAttributeSetBase::OnRep_MaxPoise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, MaxPoise, OldValue);
}

void UActionAttributeSetBase::OnRep_PoiseDefense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, PoiseDefense, OldValue);
}

void UActionAttributeSetBase::OnRep_DamageVulnerability(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, DamageVulnerability, OldValue);
}

void UActionAttributeSetBase::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, MoveSpeed, OldValue);
}

void UActionAttributeSetBase::OnRep_StaminaRecovery(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, StaminaRecovery, OldValue);
}

void UActionAttributeSetBase::OnRep_EnergyRecovery(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, EnergyRecovery, OldValue);
}

void UActionAttributeSetBase::OnRep_ExecutionDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActionAttributeSetBase, ExecutionDamageMultiplier, OldValue);
}
