// Fill out your copyright notice in the Description page of Project Settings.


#include "UMG/MVVM/MVVM_HeroAttribute.h"

#include "Components/Attribute/HeroAttributeComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"

UMVVM_HeroAttribute::UMVVM_HeroAttribute()
	:Super()
{
}

float UMVVM_HeroAttribute::CalculationRegenRate(float InValue, float InMaxValue)
{
	if (InMaxValue <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return FMath::GridSnap(InValue / InMaxValue, 0.01f);
}

void UMVVM_HeroAttribute::InitMVVMWithHeroAttributeSet(UHeroAttributeComponent* HeroAttributeSet)
{
	if (HeroAttributeSet)
	{
		HeroAttributeSetPtr = HeroAttributeSet;
		HeroAttributeSetPtr->OnHeroAttributeChangeDelegate.AddUObject(this, &UMVVM_HeroAttribute::OnHeroAttributeChange);
	}
}

void UMVVM_HeroAttribute::UninitializeMVVM()
{
	if (HeroAttributeSetPtr.IsValid())
	{
		HeroAttributeSetPtr->OnHeroAttributeChangeDelegate.RemoveAll(this);
	}
	HeroAttributeSetPtr = nullptr;
}

void UMVVM_HeroAttribute::OnHeroAttributeChange(FGameplayAttribute ChangedAttribute, float OldValue, float NewValue)
{
	if (ChangedAttribute == UActionAttributeSetBase::GetHealthAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(Health, NewValue);
		UE_MVVM_SET_PROPERTY_VALUE(HealthRegenRate, CalculationRegenRate(Health, MaxHealth));
		return;
	}

	if (ChangedAttribute == UActionAttributeSetBase::GetMaxHealthAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewValue);
		UE_MVVM_SET_PROPERTY_VALUE(HealthRegenRate, CalculationRegenRate(Health, MaxHealth));
		UE_MVVM_SET_PROPERTY_VALUE(ShieldPercent, CalculationRegenRate(Shield, MaxHealth));
		return;
	}

	if (ChangedAttribute == UActionAttributeSetBase::GetShieldAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(Shield, NewValue);
		UE_MVVM_SET_PROPERTY_VALUE(ShieldPercent, CalculationRegenRate(Shield, MaxHealth));
		return;
	}

	if (ChangedAttribute == UActionAttributeSetBase::GetStaminaAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(Stamina, NewValue);
		UE_MVVM_SET_PROPERTY_VALUE(StaminaRegenRate, CalculationRegenRate(Stamina, MaxStamina));
		return;
	}

	if (ChangedAttribute == UActionAttributeSetBase::GetMaxStaminaAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, NewValue);
		UE_MVVM_SET_PROPERTY_VALUE(StaminaRegenRate, CalculationRegenRate(Stamina, MaxStamina));
		return;
	}

	if (ChangedAttribute == UActionAttributeSetBase::GetEnergyAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(Energy, NewValue);
		UE_MVVM_SET_PROPERTY_VALUE(EnergyRegenRate, CalculationRegenRate(Energy, MaxEnergy));
		return;
	}

	if (ChangedAttribute == UActionAttributeSetBase::GetMaxEnergyAttribute())
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxEnergy, NewValue);
		UE_MVVM_SET_PROPERTY_VALUE(EnergyRegenRate, CalculationRegenRate(Energy, MaxEnergy));
		return;
	}

}

void UMVVM_HeroAttribute::BeginDestroy()
{
	UninitializeMVVM();
	Super::BeginDestroy();
}
