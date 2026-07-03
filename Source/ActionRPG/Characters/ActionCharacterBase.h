// 文件说明：声明 ActionCharacterBase 相关接口。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"
#include "GameplayTagAssetInterface.h"
#include "Interfaces/Combat/ActionCombatReactInterface.h"
#include "ActionCharacterBase.generated.h"

class UActionAbilitySystemComponent;
class UActionAttributeSetBase;
class UActionCombatReactComponent;
class UActionCollisionRuntimeComponent;
class UExecutionWindowComponent;

/**
 * 角色默认属性初始化配置。
 * 当前这组数据用于在没有独立成长配置时，给 Hero / Enemy 写入一份可运行的最小属性基线。
 */
USTRUCT(BlueprintType)
struct FActionDefaultAttributeInitData
{
	GENERATED_BODY()

public:
	/** 默认最大生命值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float MaxHealth = 100.f;

	/** 默认当前生命值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float Health = 100.f;

	/** 默认护盾值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float Shield = 0.f;

	/** 默认最大体力值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float MaxStamina = 100.f;

	/** 默认当前体力值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float Stamina = 100.f;

	/** 默认最大能量值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float MaxEnergy = 100.f;

	/** 默认当前能量值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float Energy = 0.f;

	/** 默认最大韧性值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float MaxPoise = 100.f;

	/** 默认当前韧性值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital")
	float Poise = 100.f;

	/** 默认攻击力。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Offense")
	float AttackPower = 20.f;

	/** 默认防御力。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense")
	float DefensePower = 10.f;

	/** 默认格挡防御值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense")
	float GuardDefense = 15.f;

	/** 默认削韧防御值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense")
	float PoiseDefense = 10.f;

	/** 默认移动速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Utility")
	float MoveSpeed = 500.f;

	/** 默认体力恢复速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Utility")
	float StaminaRecovery = 15.f;

	/** 默认能量恢复速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Utility")
	float EnergyRecovery = 10.f;

	/** 默认处决伤害倍率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Offense")
	float ExecutionDamageMultiplier = 2.f;
};

UCLASS()
class ACTIONRPG_API AActionCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface, public IGameplayTagAssetInterface, public IActionCombatReactInterface
{
	GENERATED_BODY()

public:
	AActionCharacterBase();

	// IAbilitySystemInterface：向外暴露角色持有的能力系统组件。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// IGameplayTagAssetInterface：把 ASC 中的标签查询能力透传给角色。
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	// IActionCombatReactInterface：统一处理外部战斗反应事件。
	virtual bool HandleCombatReactEvent_Implementation(FGameplayTag EventTag, const FActionDamagePayload& DamagePayload) override;

	// 获取角色身上的自定义能力系统组件。
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UActionAbilitySystemComponent* GetActionAbilitySystemComponent() const { return ActionAbilitySystemComponent; }

	// 获取角色身上的属性集。
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UActionAttributeSetBase* GetActionAttributeSet() const { return ActionAttributeSet; }

	// 获取角色身上的处决窗口组件。
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UExecutionWindowComponent* GetExecutionWindowComponent() const { return ExecutionWindowComponent; }

	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	UActionCombatReactComponent* GetActionCombatReactComponent() const { return ActionCombatReactComponent; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	UActionCollisionRuntimeComponent* GetActionCollisionRuntimeComponent() const { return ActionCollisionRuntimeComponent; }

	// 通过武器 subtype 标签获取角色身上对应的插槽名。
	// 当前正式语义固定为 Player.Weapon.<Category>.<Subtype> 这一层，不再回退到粗类别根 Tag。
	FName GetWeaponSocketBySubtypeTag(FGameplayTag InWeaponSubtypeTag) const;

	// 通过武器 subtype 标签获取角色身上对应的待命 / 收纳插槽名。
	// 若当前角色未单独配置待命挂点，则回退到装备挂点，保证武器表现链始终有稳定落点。
	// 当前已装备武器在 Idle 表现下也会挂到这里，只是保持可见；非当前槽位武器仍由装配桥负责隐藏。
	FName GetHolsteredWeaponSocketBySubtypeTag(FGameplayTag InWeaponSubtypeTag) const;

protected:
	virtual void BeginPlay() override;

	/**
	 * 写入一份最小可用的默认属性。
	 * 这里的目标是让角色在没有独立成长配置前，也能稳定进入战斗主链。
	 */
	void InitializeDefaultAttributes();

protected:
	UPROPERTY()
	UActionAbilitySystemComponent* ActionAbilitySystemComponent = nullptr;

	UPROPERTY()
	UActionAttributeSetBase* ActionAttributeSet = nullptr;

	// 角色上的通用处决窗口组件，英雄和敌人都共用这一套失衡窗口机制。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UExecutionWindowComponent* ExecutionWindowComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UActionCombatReactComponent* ActionCombatReactComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UActionCollisionRuntimeComponent* ActionCollisionRuntimeComponent = nullptr;

	// 角色身上不同武器小类别标签对应的挂点插槽。
	UPROPERTY(EditDefaultsOnly, Category = "ConfigData|WeaponSocket", meta = (ToolTip = "角色身上不同武器小类别标签对应的挂点插槽。当前正式使用 Player.Weapon.<Category>.<Subtype> 这一层作为键。"))
	TMap<FGameplayTag, FName> WeaponSockets;

	// 角色身上不同武器小类别标签对应的待命 / 收纳挂点插槽。
	UPROPERTY(EditDefaultsOnly, Category = "ConfigData|WeaponSocket", meta = (ToolTip = "角色身上不同武器小类别标签对应的待命 / 收纳挂点插槽。若某个小类别未配置，则自动回退到装备挂点。"))
	TMap<FGameplayTag, FName> HolsteredWeaponSockets;

	// 默认属性初始化值。
	// 当前 Hero 和 Enemy 都先复用这一份最小初始化配置，
	// 后续如果切到正式 GE / 配表初始化，可以再把这层替换掉。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ConfigData|DefaultAttributes")
	FActionDefaultAttributeInitData DefaultAttributeInitData;

	// 防止 PossessedBy / OnRep / BeginPlay 等多条生命周期路径重复把默认属性写回去。
	bool bDefaultAttributesInitialized = false;
};
