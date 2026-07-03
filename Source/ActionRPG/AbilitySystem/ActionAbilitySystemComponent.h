// 文件说明：声明 ActionAbilitySystemComponent 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionAbilitySystemComponent.generated.h"

class UActionGameplayAbilityBase;
class UActionCombatReactComponent;
class UDataAsset_StatusEffectDefinition;
class UGameplayAbility;
class UGameplayEffect;

/**
 * 战斗输入进入 GAS 的总入口，同时也是 Ability 关系系统的统一激活门。
 * 它负责把输入标签解析到候选 AbilitySpec，再在真正激活前统一完成关系裁决、
 * 失败口径整理和必要的主动取消收尾，避免每条 Ability 自己维护一套平行激活链。
 */
UCLASS()
class ACTIONRPG_API UActionAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UActionAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	// 输入转发接口。
	// 这里只负责把输入映射到候选 AbilitySpec，并把后续处理继续交给正式关系裁决链。
	bool OnAbilityInputPressed(const FGameplayTag& InInputTag);
	bool OnAbilityInputHeld(const FGameplayTag& InInputTag);
	void OnAbilityInputReleased(const FGameplayTag& InInputTag);

public:
	// 能力授予与移除接口。
	UFUNCTION(BlueprintCallable, Category = "Ability")
	FGameplayAbilitySpecHandle GrantAbility(const TSubclassOf<UGameplayAbility>& InGrantedAbilities, FGameplayTag AbilityTag = FGameplayTag(), int32 ApplyLevel = 1);

	UFUNCTION(BlueprintCallable, Category = "Ability")
	void RemovedAbility(FGameplayAbilitySpecHandle& InSpecHandlesToRemove);

public:
	// 能力查询与取消接口。
	FGameplayAbilitySpec* GetActivatableAbilitySpecByTag(FGameplayTag AbilityTag);
	const FGameplayAbilitySpec* GetActivatableAbilitySpecByTag(FGameplayTag AbilityTag) const;
	bool TryActivateAbilityByTag(FGameplayTag AbilityTag);
	void CancelAbilityByTag(FGameplayTag AbilityTag);
	void CancelAbilityByAbilityTag(FGameplayTag AbilityTag);
	void CancelAbilitiesByAbilityTags(const FGameplayTagContainer& AbilityTags);
	bool ApplyAbilityCooldownByInputTag(const FGameplayTag& AbilityInputTag);

	/** 打印当前玩家已授予的 Hero 战斗 GA 关系配置摘要，用于 OpenMap 实机场景联调核对蓝图默认值。 */
	void PrintHeroCombatAbilityRelationshipAudit() const;

	/** 按输入标签打印一条 Hero 战斗 GA 的运行时调试摘要，用于解释“为什么能放/为什么不能放”。 */
	bool PrintHeroCombatAbilityDebugByInputTag(FGameplayTag AbilityInputTag) const;

	/**
	 * 按 Ability 自身标签读取“当前已经激活的实例”。
	 * 适用场景：
	 * 1. AnimNotify 需要在运行时定位某个正在执行的 GA；
	 * 2. 不希望由外部系统直接依赖输入 Tag 或 SpecHandle；
	 * 3. 单人项目里先统一通过主实例完成联动，后续若扩展多实例 Ability，再单独细化。
	 */
	UFUNCTION(BlueprintPure, Category = "Ability")
	UGameplayAbility* GetActiveAbilityInstanceByAbilityTag(FGameplayTag AbilityTag) const;

public:
	// 状态效果定义与运行时查询接口。
	UFUNCTION(BlueprintPure, Category = "Action|StatusEffect")
	UDataAsset_StatusEffectDefinition* FindStatusEffectDefinitionByTag(FGameplayTag StatusEffectTag) const;

	/**
	 * 统一应用一份持续战斗修正效果。
	 * 当前用于把霸体、易伤、抗性、处决伤害强化这类“持续一段时间的战斗状态”全部收口到同一份 GE。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|StatusEffect")
	FActiveGameplayEffectHandle ApplyCombatModifierEffect(const FActionCombatModifierEffectSpec& EffectSpec);

	/**
	 * 统一应用一份处决执行保护效果。
	 * 这条入口只服务执行者无敌和 victim lock 这类执行链保护状态，
	 * 与普通 CombatModifier 的区别是：它不会因为自身授予的执行保护标签而触发 GE 自我抑制。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|StatusEffect")
	FActiveGameplayEffectHandle ApplyExecutionProtectionEffect(const FActionCombatModifierEffectSpec& EffectSpec);

	/**
	 * 读取当前 ASC 身上所有仍在生效的状态效果快照。
	 * 这份接口主要提供给 UI 与调试层使用，避免它们直接读取底层 ActiveGameplayEffect 结构。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|StatusEffect")
	void GetActiveStatusEffectInfos(TArray<FActionActiveStatusEffectInfo>& OutStatusEffects) const;

	/** 判断当前 ASC 是否正处于某个指定状态效果之下。 */
	UFUNCTION(BlueprintPure, Category = "Action|StatusEffect")
	bool HasActiveStatusEffectTag(FGameplayTag StatusEffectTag) const;

protected:
	// UAbilitySystemComponent 输入回调覆写。
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	/** 从已授予 AbilitySpec 中按输入标签选出当前最该响应的一条。 */
	FGameplayAbilitySpec* FindBestAbilitySpecByInputTag(const FGameplayTag& InInputTag);

	/**
	 * 走完整的 Ability 关系裁决后再尝试正式激活候选 Spec。
	 * 这里会先收集待取消 Ability，再统一取消，再激活候选者。
	 */
	bool TryActivateAbilitySpecWithRelationship(FGameplayAbilitySpec& Spec);

	/**
	 * 解析候选 Ability 在当前上下文里是否允许激活，并产出需要先取消的旧 Ability 列表。
	 * 这里只做关系层预裁决，不真正提交激活。
	 */
	bool ResolveAbilityRelationshipBeforeActivation(
		const FGameplayAbilitySpec& CandidateSpec,
		TArray<FGameplayAbilitySpecHandle>& OutAbilityHandlesToCancel,
		FString& OutFailureReason) const;
	UActionGameplayAbilityBase* ResolveActionAbilityFromSpec(const FGameplayAbilitySpec& Spec) const;

	/** 统一整理关系裁决失败文本，供屏幕调试和日志共用同一份口径。 */
	FString BuildRelationshipBlockedDebugText(
		const UActionGameplayAbilityBase* CandidateAbility,
		const UActionCombatReactComponent* CombatReactComponent,
		const FString& FailureReason) const;
	void GetHeroCombatAbilityRelationshipAuditLines(TArray<FString>& OutAuditLines) const;
	FString BuildHeroCombatAbilityRelationshipAuditLine(
		const FGameplayAbilitySpec& AbilitySpec,
		const UActionGameplayAbilityBase* Ability,
		const UActionCombatReactComponent* CombatReactComponent) const;
	void AddStatusEffectTagsToSpec(FGameplayEffectSpec& GameplayEffectSpec, const FGameplayTag& StatusEffectTag) const;
	FActiveGameplayEffectHandle ApplyRuntimeCombatModifierEffect(
		const TSubclassOf<UGameplayEffect>& GameplayEffectClass,
		const FActionCombatModifierEffectSpec& EffectSpec);
	void ApplyCombatModifierSetByCallerValues(FGameplayEffectSpec& GameplayEffectSpec, const FActionCombatModifierEffectSpec& EffectSpec) const;

protected:
	/** 当前项目已知的状态效果定义列表。后续 UI 可直接从这里补全标签对应的展示数据。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|StatusEffect")
	TArray<TObjectPtr<UDataAsset_StatusEffectDefinition>> StatusEffectDefinitions;

	/** 当前 ASC 组装持续战斗修正效果时所使用的通用 GE 类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|StatusEffect")
	TSubclassOf<UGameplayEffect> CombatModifierGameplayEffectClass;

	/** 当前 ASC 组装处决执行保护效果时所使用的专用 GE 类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|StatusEffect")
	TSubclassOf<UGameplayEffect> ExecutionProtectionGameplayEffectClass;
};
