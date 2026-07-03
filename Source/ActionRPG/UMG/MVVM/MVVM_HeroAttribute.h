// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "MVVM_HeroAttribute.generated.h"

class UHeroAttributeComponent;
struct FGameplayAttribute;

UCLASS()
class ACTIONRPG_API UMVVM_HeroAttribute : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	UMVVM_HeroAttribute();

	float CalculationRegenRate(float InValue, float InMaxValue);

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetHealthRegenRate() const { return HealthRegenRate; }
	float GetShield() const { return Shield; }
	float GetShieldPercent() const { return ShieldPercent; }

	float GetStamina() const { return Stamina; }
	float GetMaxStamina() const { return MaxStamina; }
	float GetStaminaRegenRate() const { return StaminaRegenRate; }

	float GetEnergy() const { return Energy; }
	float GetMaxEnergy() const { return MaxEnergy; }
	float GetEnergyRegenRate() const { return EnergyRegenRate; }

	// 初始化函数，在HUD中进行创建并初始化，传入属性组件指针，绑定属性变化的回调函数等
	void InitMVVMWithHeroAttributeSet(UHeroAttributeComponent* HeroAttributeSet);
	void UninitializeMVVM();

	// 属性变化的回调函数，参数为属性类、旧值、新值
	void OnHeroAttributeChange(FGameplayAttribute ChangedAttribute, float OldValue, float NewValue);

	virtual void BeginDestroy() override;

protected:

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float Health;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float MaxHealth;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float HealthRegenRate;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float Shield;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float ShieldPercent;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float Stamina;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float MaxStamina;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float StaminaRegenRate;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float Energy;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float MaxEnergy;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter)
	float EnergyRegenRate;

private:
	TWeakObjectPtr<UHeroAttributeComponent> HeroAttributeSetPtr = nullptr;

};
