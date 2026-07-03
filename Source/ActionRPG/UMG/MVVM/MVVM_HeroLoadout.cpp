#include "UMG/MVVM/MVVM_HeroLoadout.h"

#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Attribute/HeroAttributeComponent.h"
#include "Components/Character/HeroAssemblyComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Equipment/HeroLoadoutStateComponent.h"
#include "UMG/MVVM/MVVM_HeroLoadoutSlot.h"

UMVVM_HeroLoadout::UMVVM_HeroLoadout()
	: Super()
{
}

void UMVVM_HeroLoadout::InitMVVMWithHeroCharacter(AActionHeroCharacter* InHeroCharacter)
{
	UninitializeMVVM();
	EnsureSlotViewModels();

	if (!InHeroCharacter)
	{
		return;
	}

	HeroCharacterPtr = InHeroCharacter;
	HeroLoadoutStateComponentPtr = InHeroCharacter->FindComponentByClass<UHeroLoadoutStateComponent>();
	HeroCombatComponentPtr = InHeroCharacter->GetHeroCombatComponent();
	HeroAttributeComponentPtr = InHeroCharacter->GetHeroAttributeComponent();

	if (HeroLoadoutStateComponentPtr.IsValid())
	{
		LoadoutUIStateChangedHandle = HeroLoadoutStateComponentPtr->OnLoadoutUIStateChanged().AddUObject(
			this,
			&ThisClass::HandleLoadoutUIStateChanged);
	}

	if (HeroCombatComponentPtr.IsValid())
	{
		CombatUIStateChangedHandle = HeroCombatComponentPtr->OnCombatUIStateChanged().AddUObject(
			this,
			&ThisClass::HandleCombatUIStateChanged);
	}

	if (HeroAttributeComponentPtr.IsValid())
	{
		HeroAttributeComponentPtr->OnHeroAttributeChangeDelegate.AddUObject(
			this,
			&ThisClass::HandleHeroAttributeChange);
	}

	RefreshFromHeroCharacter();
}

void UMVVM_HeroLoadout::UninitializeMVVM()
{
	if (HeroLoadoutStateComponentPtr.IsValid() && LoadoutUIStateChangedHandle.IsValid())
	{
		HeroLoadoutStateComponentPtr->OnLoadoutUIStateChanged().Remove(LoadoutUIStateChangedHandle);
	}

	if (HeroCombatComponentPtr.IsValid() && CombatUIStateChangedHandle.IsValid())
	{
		HeroCombatComponentPtr->OnCombatUIStateChanged().Remove(CombatUIStateChangedHandle);
	}

	if (HeroAttributeComponentPtr.IsValid())
	{
		HeroAttributeComponentPtr->OnHeroAttributeChangeDelegate.RemoveAll(this);
	}

	LoadoutUIStateChangedHandle.Reset();
	CombatUIStateChangedHandle.Reset();
	HeroCharacterPtr = nullptr;
	HeroLoadoutStateComponentPtr = nullptr;
	HeroCombatComponentPtr = nullptr;
	HeroAttributeComponentPtr = nullptr;
}

void UMVVM_HeroLoadout::BeginDestroy()
{
	UninitializeMVVM();
	Super::BeginDestroy();
}

void UMVVM_HeroLoadout::EnsureSlotViewModels()
{
	if (!UnarmedSlotViewModel)
	{
		UnarmedSlotViewModel = NewObject<UMVVM_HeroLoadoutSlot>(this, UMVVM_HeroLoadoutSlot::StaticClass());
	}

	if (!MeleeSlotViewModel)
	{
		MeleeSlotViewModel = NewObject<UMVVM_HeroLoadoutSlot>(this, UMVVM_HeroLoadoutSlot::StaticClass());
	}

	if (!RangedSlotViewModel)
	{
		RangedSlotViewModel = NewObject<UMVVM_HeroLoadoutSlot>(this, UMVVM_HeroLoadoutSlot::StaticClass());
	}

	if (!HybridSlotViewModel)
	{
		HybridSlotViewModel = NewObject<UMVVM_HeroLoadoutSlot>(this, UMVVM_HeroLoadoutSlot::StaticClass());
	}
}

void UMVVM_HeroLoadout::RefreshFromHeroCharacter()
{
	EnsureSlotViewModels();

	FHeroWeaponLoadoutUISnapshot Snapshot;
	if (HeroCharacterPtr.IsValid())
	{
		if (const UHeroAssemblyComponent* HeroAssemblyComponent =
			HeroCharacterPtr->FindComponentByClass<UHeroAssemblyComponent>())
		{
			HeroAssemblyComponent->BuildHeroLoadoutUISnapshot(Snapshot);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(CurrentEquippedLoadoutSlot, Snapshot.CurrentEquippedLoadoutSlot);
	UE_MVVM_SET_PROPERTY_VALUE(StartupState, Snapshot.StartupState);
	UE_MVVM_SET_PROPERTY_VALUE(StartupProgressRatio, Snapshot.StartupProgressRatio);
	UE_MVVM_SET_PROPERTY_VALUE(StartupPendingSlotCount, Snapshot.StartupPendingSlotCount);
	UE_MVVM_SET_PROPERTY_VALUE(StartupTotalSlotCount, Snapshot.StartupTotalSlotCount);
	UE_MVVM_SET_PROPERTY_VALUE(StartupFailureReason, Snapshot.StartupFailureReason);
	UE_MVVM_SET_PROPERTY_VALUE(bWeaponSwitchBlockedByCooldown, Snapshot.bWeaponSwitchBlockedByCooldown);
	UE_MVVM_SET_PROPERTY_VALUE(bSpecialWeaponSwitchReady, Snapshot.bSpecialWeaponSwitchReady);

	for (const FHeroWeaponLoadoutSlotUISnapshot& SlotSnapshot : Snapshot.LoadoutSlots)
	{
		if (UMVVM_HeroLoadoutSlot* SlotViewModel = FindSlotViewModel(SlotSnapshot.LoadoutSlot))
		{
			SlotViewModel->ApplySnapshot(SlotSnapshot);
		}
	}
}

UMVVM_HeroLoadoutSlot* UMVVM_HeroLoadout::FindSlotViewModel(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	switch (InLoadoutSlot)
	{
	case EHeroWeaponLoadoutSlot::Unarmed:
		return UnarmedSlotViewModel;

	case EHeroWeaponLoadoutSlot::MeleeWeapon:
		return MeleeSlotViewModel;

	case EHeroWeaponLoadoutSlot::RangedWeapon:
		return RangedSlotViewModel;

	case EHeroWeaponLoadoutSlot::HybridWeapon:
		return HybridSlotViewModel;

	default:
		break;
	}

	return nullptr;
}

void UMVVM_HeroLoadout::HandleLoadoutUIStateChanged()
{
	RefreshFromHeroCharacter();
}

void UMVVM_HeroLoadout::HandleCombatUIStateChanged()
{
	RefreshFromHeroCharacter();
}

void UMVVM_HeroLoadout::HandleHeroAttributeChange(
	FGameplayAttribute ChangedAttribute,
	float OldValue,
	float NewValue)
{
	if (ChangedAttribute == UActionAttributeSetBase::GetEnergyAttribute()
		|| ChangedAttribute == UActionAttributeSetBase::GetMaxEnergyAttribute()
		|| ChangedAttribute == UActionAttributeSetBase::GetSpecialWeaponSwitchEnergyAttribute()
		|| ChangedAttribute == UActionAttributeSetBase::GetMaxSpecialWeaponSwitchEnergyAttribute())
	{
		RefreshFromHeroCharacter();
	}
}
