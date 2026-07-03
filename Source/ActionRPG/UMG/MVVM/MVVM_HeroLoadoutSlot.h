#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "MVVM_HeroLoadoutSlot.generated.h"

UCLASS()
class ACTIONRPG_API UMVVM_HeroLoadoutSlot : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UMVVM_HeroLoadoutSlot();

	void ApplySnapshot(const FHeroWeaponLoadoutSlotUISnapshot& InSnapshot);

	EHeroWeaponLoadoutSlot GetLoadoutSlot() const { return LoadoutSlot; }
	EHeroWeaponCategory GetAllowedWeaponCategory() const { return AllowedWeaponCategory; }
	FGameplayTag GetWeaponTag() const { return WeaponTag; }
	const FString& GetWeaponLabel() const { return WeaponLabel; }
	EActionWeaponPropertyType GetWeaponPropertyType() const { return WeaponPropertyType; }
	bool GetbHasAssignedWeaponDefinition() const { return bHasAssignedWeaponDefinition; }
	bool GetbRuntimeReady() const { return bRuntimeReady; }
	bool GetbIsEquipped() const { return bIsEquipped; }
	bool GetbSupportsProjectileSwitching() const { return bSupportsProjectileSwitching; }
	FGameplayTag GetSelectedProjectileConfigTag() const { return SelectedProjectileConfigTag; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	EHeroWeaponLoadoutSlot LoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	EHeroWeaponCategory AllowedWeaponCategory = EHeroWeaponCategory::Unarmed;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	FGameplayTag WeaponTag = ActionGameplayTags::Player_Weapon_Unarmed_Default;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	FString WeaponLabel;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	EActionWeaponPropertyType WeaponPropertyType = EActionWeaponPropertyType::Mundane;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	bool bHasAssignedWeaponDefinition = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	bool bRuntimeReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	bool bIsEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	bool bSupportsProjectileSwitching = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter)
	FGameplayTag SelectedProjectileConfigTag;
};
