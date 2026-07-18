// 文件说明：声明英雄属性 HUD 只读 ViewModel。

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

	/** 按当前值和上限换算显示用结果。只服务 HUD 展示，不是属性计算公式权威入口。 */
	float CalculationRegenRate(float InValue, float InMaxValue);

	/** 以下 getter 返回的都是 HUD 当前显示快照，不是正式属性状态源。 */
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

	/** 绑定 HeroAttributeComponent，并开始消费属性广播。它只建立 HUD 消费链绑定关系，不创造新的属性状态。 */
	void InitMVVMWithHeroAttributeSet(UHeroAttributeComponent* HeroAttributeSet);
	/** 解绑当前属性广播桥。 */
	void UninitializeMVVM();

	/** 消费属性变化广播并刷新当前 ViewModel 快照。这里维护的是显示快照，不是正式属性本体。 */
	void OnHeroAttributeChange(FGameplayAttribute ChangedAttribute, float OldValue, float NewValue);

	virtual void BeginDestroy() override;

protected:
	/** 以下字段都是 UI 层显示快照，通过属性广播桥刷新。 */
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的生命值快照。它只消费属性广播结果，不是正式属性本体。"))
	float Health;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的最大生命值快照。"))
	float MaxHealth;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的生命恢复展示值。它只服务显示换算。"))
	float HealthRegenRate;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的护盾值快照。"))
	float Shield;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的护盾百分比快照。"))
	float ShieldPercent;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的体力值快照。"))
	float Stamina;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的最大体力值快照。"))
	float MaxStamina;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的体力恢复展示值。它只服务显示换算。"))
	float StaminaRegenRate;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的能量值快照。"))
	float Energy;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的最大能量值快照。"))
	float MaxEnergy;
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroAttribute", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的能量恢复展示值。它只服务显示换算。"))
	float EnergyRegenRate;

private:
	/** 当前绑定的属性桥组件引用。它只是消费链绑定关系，不是属性宿主或属性缓存本体。 */
	TWeakObjectPtr<UHeroAttributeComponent> HeroAttributeSetPtr = nullptr;
};
