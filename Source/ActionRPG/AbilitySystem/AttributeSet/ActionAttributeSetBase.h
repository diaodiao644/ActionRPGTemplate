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

/** 属性变化统一广播。它只转发正式属性写回后的变化结果，不持有第二套属性缓存，也不替代属性本体。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttributeChange, const FOnAttributeChangeData&);

/**
 * 角色正式属性权威源。
 * 它负责持有生命、护盾、体力、能量、特殊切武能量、削韧等正式数值本体，
 * 同时承接生命伤害、格挡耗体、削韧伤害三条元属性收尾链。
 * 所有属性桥、UI 和调试查询都应把这里视为权威源，而不是再造第二套运行态。
 */
UCLASS()
class ACTIONRPG_API UActionAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UActionAttributeSetBase();

	/** 初始化属性变化广播入口。它只建立广播出口，不创建第二套属性缓存，也不执行属性初始化。 */
	void InitBindAttributeChangeDelegate();
	/** 把一次属性变化继续转发到统一广播。这里转发的是正式属性写回结果，不是新的属性状态快照。 */
	void OnRep_OnAttributeChange(const FOnAttributeChangeData& Data);

	/** `UAttributeSet`：注册需要复制的正式属性字段。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** `UAttributeSet`：属性在被修改前的统一钳制入口。 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** `UAttributeSet`：`GameplayEffect` 执行后的统一收尾入口。 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

public:
	/** 常用只读查询辅助接口。它们只读取当前正式属性结果，不推进属性写回，也不缓存第二套属性镜像。 */
	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "判断当前正式生命值是否仍大于 0。它只读取属性本体结果，不推进属性写回。"))
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "读取当前生命值相对于最大生命值的正式百分比结果。它只读属性本体，不创建新的 UI 快照。"))
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "读取当前护盾值相对于最大生命参考的正式百分比结果。它只读属性本体，不推进任何写回。"))
	float GetShieldPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "读取当前体力值相对于最大体力值的正式百分比结果。"))
	float GetStaminaPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "读取当前能量值相对于最大能量值的正式百分比结果。"))
	float GetEnergyPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "读取当前特殊切武能量相对于最大值的正式百分比结果。"))
	float GetSpecialWeaponSwitchEnergyPercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "读取当前削韧值相对于最大削韧值的正式百分比结果。"))
	float GetPoisePercent() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "判断当前正式能量是否已经达到最大值。"))
	bool IsEnergyFull() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "判断当前正式特殊切武能量是否已经达到最大值。"))
	bool IsSpecialWeaponSwitchEnergyFull() const;

public:
	/** 直接伤害结算接口：供单人版受击解析器在本地直接调用。它们属于正式写回入口，不是预览 helper。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Attributes", meta = (ToolTip = "按当前正式属性公式直接写回生命伤害结果。它属于正式写回入口，不是预览或调试 helper。"))
	float ApplyHealthDamage(float RawDamage, float FinalDamageMultiplier = 1.f, float IgnoredFinalDamageReductionPercent = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes", meta = (ToolTip = "按当前正式属性公式直接写回格挡耗体结果。它属于正式写回入口。"))
	float ApplyGuardStaminaCost(float RawGuardStaminaCost);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes", meta = (ToolTip = "按当前正式属性公式直接写回削韧伤害结果。它属于正式写回入口。"))
	float ApplyPoiseDamage(float RawPoiseDamage);

	/** 伤害预计算接口：便于 UI 预览、日志或调试复用同一套公式。它们不直接改写正式属性。 */
	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "按当前正式属性公式预估生命伤害结果。它只做计算，不直接改写正式属性。"))
	float CalculateHealthDamage(float RawDamage, float FinalDamageMultiplier = 1.f, float IgnoredFinalDamageReductionPercent = 0.f) const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "按当前正式属性公式预估格挡耗体结果。它只做计算，不直接改写正式属性。"))
	float CalculateGuardStaminaCost(float RawGuardStaminaCost) const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes", meta = (ToolTip = "按当前正式属性公式预估削韧伤害结果。它只做计算，不直接改写正式属性。"))
	float CalculatePoiseDamage(float RawPoiseDamage) const;

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes", meta = (ToolTip = "给当前正式能量值增加一段数值，并按最大值规则钳制。"))
	float AddEnergyValue(float DeltaEnergy);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes", meta = (ToolTip = "消耗当前正式能量值，并按最小值规则钳制。"))
	float ConsumeEnergyValue(float DeltaEnergy);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes", meta = (ToolTip = "给当前正式特殊切武能量增加一段数值，并按最大值规则钳制。"))
	float AddSpecialWeaponSwitchEnergyValue(float DeltaEnergy);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes", meta = (ToolTip = "消耗当前正式特殊切武能量，并按最小值规则钳制。"))
	float ConsumeSpecialWeaponSwitchEnergyValue(float DeltaEnergy);

	/** 按当前运行时解析出的最大值重新钳制生命、护盾、体力、能量与削韧。它只服务合法区间收口，不创造第二套属性状态。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Attributes", meta = (ToolTip = "按当前运行时解析出的最大值重新钳制生命、护盾、体力、能量与削韧。它只服务合法区间收口，不创造第二套属性状态。"))
	void ClampCurrentValuesToResolvedMaximums();

protected:
	/** 最大值变更时，按当前百分比调整对应的当前值，避免资源条突兀跳变。 */
	void AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty) const;

	/** 针对不同属性做数值钳制。它只处理本属性合法区间，不解释外层战斗语义。 */
	float ClampAttributeValue(const FGameplayAttribute& Attribute, float NewValue) const;

	/** 保证生命、护盾、体力、能量、削韧等当前值不超出最大值。 */
	void ClampCurrentAttributes();

	/** 处理元属性形式传入的生命伤害。它是 `IncomingDamage` 的正式收尾点。 */
	void HandleIncomingDamage(const FGameplayEffectModCallbackData& Data);
	/** 处理元属性形式传入的格挡耗体。它是 `IncomingGuardStaminaCost` 的正式收尾点。 */
	void HandleIncomingGuardStaminaCost();
	/** 处理元属性形式传入的削韧伤害。它是 `IncomingPoiseDamage` 的正式收尾点。 */
	void HandleIncomingPoiseDamage();

public:
	/** 属性变化广播出口。属性桥只消费这里的结果，不应把它误当成另一套属性存储或 UI 快照缓存。 */
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

	/** 特殊切武能量值。四个固定槽位共用这一条正式资源，不是单槽局部镜像。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialWeaponSwitchEnergy, Category = "Vital", meta = (ToolTip = "当前角色共享的正式特殊切武能量值。四个固定槽位共用这一条资源，不是单槽局部镜像。"))
	FGameplayAttributeData SpecialWeaponSwitchEnergy = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, SpecialWeaponSwitchEnergy)

	/** 特殊切武最大能量值。它是正式资源上限，不是 UI 独立配置快照。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpecialWeaponSwitchEnergy, Category = "Vital", meta = (ToolTip = "正式特殊切武能量的最大值。它是权威资源上限，不是 UI 独立配置快照。"))
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

	/** 额外生命伤害抗性。用于 GE 临时挂易伤/抗性时，避免继续挤占原有 DamageReduction 语义。它仍是正式属性本体。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthDamageResistance, Category = "Defense", meta = (ToolTip = "额外生命伤害抗性正式属性。它主要服务临时易伤/抗性效果，不替代最终减伤百分比语义。"))
	FGameplayAttributeData HealthDamageResistance = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, HealthDamageResistance)

	/** 防御耗体抗性。只服务普通格挡成立后的体力消耗公式。它是正式属性本体，不是格挡结果快照。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GuardStaminaCostResistance, Category = "Defense", meta = (ToolTip = "普通格挡成立后参与体力消耗公式的正式抗性属性，不是格挡结果快照。"))
	FGameplayAttributeData GuardStaminaCostResistance = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, GuardStaminaCostResistance)

	/** 额外削韧抗性。用于控制目标被打出失衡的难度。它是正式属性本体，不是受击阶段镜像。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PoiseDamageResistance, Category = "Defense", meta = (ToolTip = "参与削韧伤害公式的正式抗性属性，用于控制目标被打出失衡的难度；它不是受击阶段镜像。"))
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

	/** 受到生命伤害时的易伤百分比。与抗性分开，专门给“破甲/弱点暴露/处决前 debuff”使用。它是正式属性本体。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageVulnerability, Category = "Defense", meta = (ToolTip = "受到生命伤害时参与最终结果的正式易伤百分比属性。它和抗性分开，主要服务破甲、弱点暴露或处决前 debuff。"))
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

	/** 处决伤害倍率。通常放在攻击方身上，由处决命中在结算时读取。它是正式属性本体，不是一次处决结果快照。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExecutionDamageMultiplier, Category = "Offense", meta = (ToolTip = "攻击方身上的正式处决伤害倍率属性，由处决命中在结算时读取。它不是一次处决结果快照。"))
	FGameplayAttributeData ExecutionDamageMultiplier = 2.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, ExecutionDamageMultiplier)

	/** 传入生命伤害元属性。它只作为本次 GE 收尾前的中间入口，不是最终正式生命结果。 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta", meta = (ToolTip = "传入生命伤害元属性。它只作为本次 GameplayEffect 收尾前的中间入口，不是最终正式生命结果。"))
	FGameplayAttributeData IncomingDamage = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, IncomingDamage)

	/** 传入格挡耗体元属性，仅在普通格挡成功后消耗防守方体力。它只作为收尾入口，不是最终正式体力结果。 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta", meta = (ToolTip = "传入格挡耗体元属性，仅在普通格挡成功后消耗防守方体力。它只作为收尾入口，不是最终正式体力结果。"))
	FGameplayAttributeData IncomingGuardStaminaCost = 0.f;
	ATTRIBUTE_ACCESSORS(UActionAttributeSetBase, IncomingGuardStaminaCost)

	/** 传入削韧伤害元属性。它只作为收尾入口，不是最终正式削韧结果。 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta", meta = (ToolTip = "传入削韧伤害元属性。它只作为本次收尾入口，不是最终正式削韧结果。"))
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
