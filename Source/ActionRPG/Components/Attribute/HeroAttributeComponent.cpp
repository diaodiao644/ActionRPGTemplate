
#include "Components/Attribute/HeroAttributeComponent.h"

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"

UHeroAttributeComponent::UHeroAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

}

void UHeroAttributeComponent::InitializeWithAbilitySystem(UActionAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	if (!Owner || !InASC) return;

	OwnerASC = InASC;

	OwnerAttributeSet = OwnerASC->GetSet<UActionAttributeSetBase>();
	if (!OwnerAttributeSet.IsValid()) return;

	// 绑定属性变化的回调函数，监听属性变化事件
	OwnerAttributeSet->OnAttributeChangeDelegate.AddUObject(this, &UHeroAttributeComponent::OnRep_HeroAttributeChange);

}

void UHeroAttributeComponent::UnInitializeWithAbilitySystem()
{
	if (OwnerAttributeSet.IsValid())
	{
		//解绑属性变化的委托
		OwnerAttributeSet->OnAttributeChangeDelegate.RemoveAll(this);
	}

	OnHeroAttributeChangeDelegate.Clear();

	OwnerAttributeSet = nullptr;
	OwnerASC = nullptr;
}

void UHeroAttributeComponent::ForceRefreshAttributeChange()
{
	if(OwnerAttributeSet.IsValid())
	{
		OnHeroAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetMaxHealthAttribute(), OwnerAttributeSet->GetMaxHealth(), OwnerAttributeSet->GetMaxHealth());
		OnHeroAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetHealthAttribute(), OwnerAttributeSet->GetHealth(), OwnerAttributeSet->GetHealth());
		OnHeroAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetShieldAttribute(), OwnerAttributeSet->GetShield(), OwnerAttributeSet->GetShield());

		OnHeroAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetMaxStaminaAttribute(), OwnerAttributeSet->GetMaxStamina(), OwnerAttributeSet->GetMaxStamina());
		OnHeroAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetStaminaAttribute(), OwnerAttributeSet->GetStamina(), OwnerAttributeSet->GetStamina());

		OnHeroAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetMaxEnergyAttribute(), OwnerAttributeSet->GetMaxEnergy(), OwnerAttributeSet->GetMaxEnergy());
		OnHeroAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetEnergyAttribute(), OwnerAttributeSet->GetEnergy(), OwnerAttributeSet->GetEnergy());
		OnHeroAttributeChangeDelegate.Broadcast(
			OwnerAttributeSet->GetMaxSpecialWeaponSwitchEnergyAttribute(),
			OwnerAttributeSet->GetMaxSpecialWeaponSwitchEnergy(),
			OwnerAttributeSet->GetMaxSpecialWeaponSwitchEnergy());
		OnHeroAttributeChangeDelegate.Broadcast(
			OwnerAttributeSet->GetSpecialWeaponSwitchEnergyAttribute(),
			OwnerAttributeSet->GetSpecialWeaponSwitchEnergy(),
			OwnerAttributeSet->GetSpecialWeaponSwitchEnergy());
	}
}

void UHeroAttributeComponent::OnRep_HeroAttributeChange(const FOnAttributeChangeData& Data)
{
	OnHeroAttributeChangeDelegate.Broadcast(Data.Attribute, Data.OldValue, Data.NewValue);
}

void UHeroAttributeComponent::DestroyComponent(bool bPromoteChildren)
{
	UnInitializeWithAbilitySystem();
	Super::DestroyComponent(bPromoteChildren);
}



void UHeroAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
}


