#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "MVVM_HeroLoadout.generated.h"

class AActionHeroCharacter;
class UHeroAttributeComponent;
class UHeroCombatComponent;
class UHeroLoadoutStateComponent;
class UMVVM_HeroLoadoutSlot;
struct FGameplayAttribute;

UCLASS()
class ACTIONRPG_API UMVVM_HeroLoadout : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UMVVM_HeroLoadout();

	void InitMVVMWithHeroCharacter(AActionHeroCharacter* InHeroCharacter);
	void UninitializeMVVM();
	virtual void BeginDestroy() override;

	EHeroWeaponLoadoutSlot GetCurrentEquippedLoadoutSlot() const { return CurrentEquippedLoadoutSlot; }
	EHeroWeaponLoadoutStartupState GetStartupState() const { return StartupState; }
	float GetStartupProgressRatio() const { return StartupProgressRatio; }
	int32 GetStartupPendingSlotCount() const { return StartupPendingSlotCount; }
	int32 GetStartupTotalSlotCount() const { return StartupTotalSlotCount; }
	const FString& GetStartupFailureReason() const { return StartupFailureReason; }
	bool GetbWeaponSwitchBlockedByCooldown() const { return bWeaponSwitchBlockedByCooldown; }
	bool GetbSpecialWeaponSwitchReady() const { return bSpecialWeaponSwitchReady; }

	UFUNCTION(BlueprintCallable, Category = "MVVM|HeroLoadout")
	UMVVM_HeroLoadoutSlot* GetUnarmedSlotViewModel() const { return UnarmedSlotViewModel; }

	UFUNCTION(BlueprintCallable, Category = "MVVM|HeroLoadout")
	UMVVM_HeroLoadoutSlot* GetMeleeSlotViewModel() const { return MeleeSlotViewModel; }

	UFUNCTION(BlueprintCallable, Category = "MVVM|HeroLoadout")
	UMVVM_HeroLoadoutSlot* GetRangedSlotViewModel() const { return RangedSlotViewModel; }

	UFUNCTION(BlueprintCallable, Category = "MVVM|HeroLoadout")
	UMVVM_HeroLoadoutSlot* GetHybridSlotViewModel() const { return HybridSlotViewModel; }

protected:
	void EnsureSlotViewModels();
	void RefreshFromHeroCharacter();
	UMVVM_HeroLoadoutSlot* FindSlotViewModel(EHeroWeaponLoadoutSlot InLoadoutSlot) const;
	void HandleLoadoutUIStateChanged();
	void HandleCombatUIStateChanged();
	void HandleHeroAttributeChange(FGameplayAttribute ChangedAttribute, float OldValue, float NewValue);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout")
	TObjectPtr<UMVVM_HeroLoadoutSlot> UnarmedSlotViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout")
	TObjectPtr<UMVVM_HeroLoadoutSlot> MeleeSlotViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout")
	TObjectPtr<UMVVM_HeroLoadoutSlot> RangedSlotViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout")
	TObjectPtr<UMVVM_HeroLoadoutSlot> HybridSlotViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter)
	EHeroWeaponLoadoutSlot CurrentEquippedLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter)
	EHeroWeaponLoadoutStartupState StartupState = EHeroWeaponLoadoutStartupState::None;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter)
	float StartupProgressRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter)
	int32 StartupPendingSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter)
	int32 StartupTotalSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter)
	FString StartupFailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter)
	bool bWeaponSwitchBlockedByCooldown = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter)
	bool bSpecialWeaponSwitchReady = false;

private:
	TWeakObjectPtr<AActionHeroCharacter> HeroCharacterPtr = nullptr;
	TWeakObjectPtr<UHeroLoadoutStateComponent> HeroLoadoutStateComponentPtr = nullptr;
	TWeakObjectPtr<UHeroCombatComponent> HeroCombatComponentPtr = nullptr;
	TWeakObjectPtr<UHeroAttributeComponent> HeroAttributeComponentPtr = nullptr;
	FDelegateHandle LoadoutUIStateChangedHandle;
	FDelegateHandle CombatUIStateChangedHandle;
};
