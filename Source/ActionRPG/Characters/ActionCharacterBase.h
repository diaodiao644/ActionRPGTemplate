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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认最大生命值。"))
	float MaxHealth = 100.f;

	/** 默认当前生命值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认当前生命值。通常初始化时会与 MaxHealth 配套填写。"))
	float Health = 100.f;

	/** 默认护盾值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认护盾值。"))
	float Shield = 0.f;

	/** 默认最大体力值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认最大体力值。"))
	float MaxStamina = 100.f;

	/** 默认当前体力值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认当前体力值。"))
	float Stamina = 100.f;

	/** 默认最大能量值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认最大能量值。"))
	float MaxEnergy = 100.f;

	/** 默认当前能量值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认当前能量值。"))
	float Energy = 0.f;

	/** 默认最大韧性值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认最大韧性值。"))
	float MaxPoise = 100.f;

	/** 默认当前韧性值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vital", meta = (ToolTip = "默认当前韧性值。"))
	float Poise = 100.f;

	/** 默认攻击力。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Offense", meta = (ToolTip = "默认攻击力。"))
	float AttackPower = 20.f;

	/** 默认防御力。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ToolTip = "默认防御力。"))
	float DefensePower = 10.f;

	/** 默认格挡防御值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ToolTip = "默认格挡防御值。"))
	float GuardDefense = 15.f;

	/** 默认削韧防御值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ToolTip = "默认削韧防御值。"))
	float PoiseDefense = 10.f;

	/** 默认移动速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Utility", meta = (ToolTip = "默认移动速度。"))
	float MoveSpeed = 500.f;

	/** 默认体力恢复速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Utility", meta = (ToolTip = "默认体力恢复速度。"))
	float StaminaRecovery = 15.f;

	/** 默认能量恢复速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Utility", meta = (ToolTip = "默认能量恢复速度。"))
	float EnergyRecovery = 10.f;

	/** 默认处决伤害倍率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Offense", meta = (ToolTip = "默认处决伤害倍率。"))
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
	// 这是通用角色宿主对外暴露的正式 ASC 解析入口，不在这里创建新的能力系统状态。
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取角色当前持有的 ActionAbilitySystemComponent。它是通用角色宿主对外暴露的正式 ASC 解析入口，不会在这里创建新的能力系统状态。"))
	UActionAbilitySystemComponent* GetActionAbilitySystemComponent() const { return ActionAbilitySystemComponent; }

	// 获取角色身上的属性集。
	// 它只返回当前宿主持有的正式属性集对象，不替代成长、存档或运行时属性来源。
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取角色当前持有的属性集对象。它只返回当前宿主挂着的正式属性集，不替代成长配置、存档回档或运行时属性来源。"))
	UActionAttributeSetBase* GetActionAttributeSet() const { return ActionAttributeSet; }

	// 获取角色身上的处决窗口组件。
	// 这里读到的是通用失衡/处决窗口宿主入口，不代表当前一定已经处于可处决状态。
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取角色当前持有的处决窗口组件。这里读到的是通用失衡/处决窗口宿主入口，不代表当前一定已经处于可处决状态。"))
	UExecutionWindowComponent* GetExecutionWindowComponent() const { return ExecutionWindowComponent; }

	// 获取角色身上的受击宿主组件。
	// 它是外部战斗反应、受击恢复和受击窗口查询的正式宿主解析入口。
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem", meta = (ToolTip = "获取角色当前持有的 CombatReact 组件。它是外部战斗反应、受击恢复和受击窗口查询的正式宿主解析入口，不形成新的平行受击状态。"))
	UActionCombatReactComponent* GetActionCombatReactComponent() const { return ActionCombatReactComponent; }

	// 获取角色身上的碰撞运行时组件。
	// 它只暴露正式碰撞运行时宿主，不把高层覆写请求模板本身当成当前真实碰撞状态。
	UFUNCTION(BlueprintCallable, Category = "Combat", meta = (ToolTip = "获取角色当前持有的碰撞运行时组件。它只暴露正式碰撞运行时宿主，不把高层覆写请求模板本身当成当前真实碰撞状态。"))
	UActionCollisionRuntimeComponent* GetActionCollisionRuntimeComponent() const { return ActionCollisionRuntimeComponent; }

	// 通过武器 subtype 标签获取角色身上对应的插槽名。
	// 当前正式语义固定为 Player.Weapon.<Category>.<Subtype> 这一层，不再回退到粗类别根 Tag。
	// 这层入口只负责角色骨骼挂点映射，不承担“这把武器当前是否应显示/装备”的运行时裁决。
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
	 * 它只服务出生期最小属性基线初始化，不应被误解成任意时机都可重刷正式属性。
	 */
	void InitializeDefaultAttributes();

protected:
	// 正式宿主对象：
	// 这一组字段分别承接 ASC、属性、处决窗口、受击与碰撞运行时等正式宿主。
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
	// 这份数据只负责“出生时先写一份可运行基线”，不是成长系统或存档回档的正式来源。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ConfigData|DefaultAttributes", meta = (ToolTip = "角色默认属性初始化值。当前 Hero 和 Enemy 都可以先用它搭出最小可运行属性基线。"))
	FActionDefaultAttributeInitData DefaultAttributeInitData;

	// 防止 PossessedBy / OnRep / BeginPlay 等多条生命周期路径重复把默认属性写回去。
	// 它只是初始化保护标志，不是新的正式属性状态源。
	bool bDefaultAttributesInitialized = false;
};
