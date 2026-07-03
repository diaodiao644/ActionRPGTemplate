// 文件说明：声明 ActionAttributeSetBase 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ActionAttributeSetBase.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

struct FGameplayEffectModCallbackData;

//属性改变的广播委托
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttributeChange, const FOnAttributeChangeData&);

/**
 * 角色基础属性集。
 * 当前版本先补齐动作游戏常用的生存、攻防、削韧和元属性，
 * 方便后续继续接伤害结算、格挡耗体、完反与闪反系统。
 */
UCLASS()
class ACTIONRPG_API UActionAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UActionAttributeSetBase();

	// 初始化属性变更广播委托以及委托绑定函数。
	void InitBindAttributeChangeDelegate();
	void OnRep_OnAttributeChange(const FOnAttributeChangeData& Data);

	// UAttributeSet：注册需要复制的属性。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// UAttributeSet：属性在被修改前的统一钳制入口。
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// UAttributeSet：GameplayEffect 执行后的收尾入口。
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

public:
	// 常用查询辅助接口。
	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetShieldPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetStaminaPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetEnergyPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetSpecialWeaponSwitchEnergyPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetPoisePercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	bool IsEnergyFull() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	bool IsSpecialWeaponSwitchEnergyFull() const;

public:
	// 直接伤害结算接口：供单人版受击解析器在本地直接调用。
	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	float ApplyHealthDamage(float RawDamage, float FinalDamageMultiplier = 1.f, float IgnoredFinalDamageReductionPercent = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	float ApplyGuardStaminaCost(float RawGuardStaminaCost);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	float ApplyPoiseDamage(float RawPoiseDamage);

	// 伤害预计算接口：便于后续 UI 预览或战斗日志使用同一套公式。
	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float CalculateHealthDamage(float RawDamage, float FinalDamageMultiplier = 1.f, float IgnoredFinalDamageReductionPercent = 0.f) const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float CalculateGuardStaminaCost(float RawGuardStaminaCost) const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float CalculatePoiseDamage(float RawPoiseDamage) const;

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	float AddEnergyValue(float DeltaEnergy);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	float ConsumeEnergyValue(float DeltaEnergy);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	float AddSpecialWeaponSwitchEnergyValue(float DeltaEnergy);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	float ConsumeSpecialWeaponSwitchEnergyValue(float DeltaEnergy);

	/** 按当前运行时解析出的最大值重新钳制生命、护盾、体力、能量与削韧。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	void ClampCurrentValuesToResolvedMaximums();

protected:
	// 最大值变更时，按当前百分比调整对应的当前值。
	void AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty) const;

	// 针对不同属性做数值钳制。
	float ClampAttributeValue(const FGameplayAttribute& Attribute, float NewValue) const;

	// 保证生命、护盾、体力、能量、削韧等当前值不超出最大值。
	void ClampCurrentAttributes();

	// 处理元属性形式传入的伤害。
	void HandleIncomingDamage(const FGameplayEffectModCallbackData& Data);
	void HandleIncomingGuardStaminaCost();
	void HandleIncomingPoiseDamage();

public:
	// 属性变更广播委托实例。
	mutable FOnAttributeChange OnAttributeChangeDelegate;

public:
	/** 生命值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vital")
	FGameplayAttributeData Health = 100.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, Health)

	/** 最大生命值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Vital")
	FGameplayAttributeData MaxHealth = 100.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, MaxHealth)

	/** 护盾值。只拦截走 IncomingDamage 链的正式生命伤害。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "Vital")
	FGameplayAttributeData Shield = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, Shield)

	/** 体力值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Vital")
	FGameplayAttributeData Stamina = 100.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, Stamina)

	/** 最大体力值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Vital")
	FGameplayAttributeData MaxStamina = 100.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, MaxStamina)

	/** 能量值，可作为后续必杀槽或技能资源。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Vital")
	FGameplayAttributeData Energy = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, Energy)

	/** 最大能量值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Vital")
	FGameplayAttributeData MaxEnergy = 100.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, MaxEnergy)

	/** 特殊切武能量值。四个固定槽位共用这一条正式资源。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialWeaponSwitchEnergy, Category = "Vital")
	FGameplayAttributeData SpecialWeaponSwitchEnergy = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, SpecialWeaponSwitchEnergy)

	/** 特殊切武最大能量值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpecialWeaponSwitchEnergy, Category = "Vital")
	FGameplayAttributeData MaxSpecialWeaponSwitchEnergy = 100.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, MaxSpecialWeaponSwitchEnergy)

	/** 基础攻击力。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Offense")
	FGameplayAttributeData AttackPower = 20.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, AttackPower)

	/** 暴击概率，百分比。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalChance, Category = "Offense")
	FGameplayAttributeData CriticalChance = 5.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, CriticalChance)

	/** 暴击伤害倍率，百分比。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalDamage, Category = "Offense")
	FGameplayAttributeData CriticalDamage = 150.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, CriticalDamage)

	/** 稳定增伤乘区。默认值固定为 1。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OutgoingDamageMultiplier, Category = "Offense")
	FGameplayAttributeData OutgoingDamageMultiplier = 1.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, OutgoingDamageMultiplier)

	/** 额外增伤乘区。默认值固定为 1。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExtraDamageMultiplier, Category = "Offense")
	FGameplayAttributeData ExtraDamageMultiplier = 1.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, ExtraDamageMultiplier)

	/** 基础防御力。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DefensePower, Category = "Defense")
	FGameplayAttributeData DefensePower = 10.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, DefensePower)

	/** 防御姿态下的额外减伤值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GuardDefense, Category = "Defense")
	FGameplayAttributeData GuardDefense = 15.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, GuardDefense)

	/** 最终减伤百分比。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageReduction, Category = "Defense")
	FGameplayAttributeData DamageReduction = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, DamageReduction)

	/** 额外生命伤害抗性。用于 GE 临时挂易伤/抗性时，避免继续挤占原有 DamageReduction 语义。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthDamageResistance, Category = "Defense")
	FGameplayAttributeData HealthDamageResistance = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, HealthDamageResistance)

	/** 防御耗体抗性。只服务普通格挡成立后的体力消耗公式。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GuardStaminaCostResistance, Category = "Defense")
	FGameplayAttributeData GuardStaminaCostResistance = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, GuardStaminaCostResistance)

	/** 额外削韧抗性。用于控制目标被打出失衡的难度。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PoiseDamageResistance, Category = "Defense")
	FGameplayAttributeData PoiseDamageResistance = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, PoiseDamageResistance)

	/** 当前削韧值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Poise, Category = "Defense")
	FGameplayAttributeData Poise = 100.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, Poise)

	/** 最大削韧值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxPoise, Category = "Defense")
	FGameplayAttributeData MaxPoise = 100.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, MaxPoise)

	/** 削韧减免值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PoiseDefense, Category = "Defense")
	FGameplayAttributeData PoiseDefense = 10.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, PoiseDefense)

	/** 受到生命伤害时的易伤百分比。与抗性分开，专门给“破甲/弱点暴露/处决前 debuff”使用。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageVulnerability, Category = "Defense")
	FGameplayAttributeData DamageVulnerability = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, DamageVulnerability)

	/** 移动速度，供角色移动组件同步使用。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "Utility")
	FGameplayAttributeData MoveSpeed = 500.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, MoveSpeed)

	/** 体力恢复速度。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRecovery, Category = "Utility")
	FGameplayAttributeData StaminaRecovery = 15.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, StaminaRecovery)

	/** 能量恢复速度。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EnergyRecovery, Category = "Utility")
	FGameplayAttributeData EnergyRecovery = 10.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, EnergyRecovery)

	/** 处决伤害倍率。通常放在攻击方身上，由处决命中在结算时读取。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExecutionDamageMultiplier, Category = "Offense")
	FGameplayAttributeData ExecutionDamageMultiplier = 2.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, ExecutionDamageMultiplier)

	/** 传入生命伤害元属性。 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingDamage = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, IncomingDamage)

	/** 传入格挡耗体元属性，仅在普通格挡成功后消耗防守方体力。 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingGuardStaminaCost = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, IncomingGuardStaminaCost)

	/** 传入削韧伤害元属性。 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingPoiseDamage = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, IncomingPoiseDamage)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Energy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_SpecialWeaponSwitchEnergy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxSpecialWeaponSwitchEnergy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CriticalChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CriticalDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_OutgoingDamageMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExtraDamageMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_DefensePower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_GuardDefense(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_DamageReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_HealthDamageResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_GuardStaminaCostResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_PoiseDamageResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Poise(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxPoise(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_PoiseDefense(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_DamageVulnerability(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_StaminaRecovery(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_EnergyRecovery(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExecutionDamageMultiplier(const FGameplayAttributeData& OldValue);
};
