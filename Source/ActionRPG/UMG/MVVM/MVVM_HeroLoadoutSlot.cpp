#include "UMG/MVVM/MVVM_HeroLoadoutSlot.h"

UMVVM_HeroLoadoutSlot::UMVVM_HeroLoadoutSlot()
	: Super()
{
}

void UMVVM_HeroLoadoutSlot::ApplySnapshot(const FHeroWeaponLoadoutSlotUISnapshot& InSnapshot)
{
	UE_MVVM_SET_PROPERTY_VALUE(LoadoutSlot, InSnapshot.LoadoutSlot);
	UE_MVVM_SET_PROPERTY_VALUE(AllowedWeaponCategory, InSnapshot.AllowedWeaponCategory);
	UE_MVVM_SET_PROPERTY_VALUE(WeaponTag, InSnapshot.WeaponTag);
	UE_MVVM_SET_PROPERTY_VALUE(WeaponLabel, InSnapshot.WeaponLabel);
	UE_MVVM_SET_PROPERTY_VALUE(WeaponPropertyType, InSnapshot.WeaponPropertyType);
	UE_MVVM_SET_PROPERTY_VALUE(bHasAssignedWeaponDefinition, InSnapshot.bHasAssignedWeaponDefinition);
	UE_MVVM_SET_PROPERTY_VALUE(bRuntimeReady, InSnapshot.bRuntimeReady);
	UE_MVVM_SET_PROPERTY_VALUE(bIsEquipped, InSnapshot.bIsEquipped);
	UE_MVVM_SET_PROPERTY_VALUE(bSupportsProjectileSwitching, InSnapshot.bSupportsProjectileSwitching);
	UE_MVVM_SET_PROPERTY_VALUE(SelectedProjectileConfigTag, InSnapshot.SelectedProjectileConfigTag);
}
