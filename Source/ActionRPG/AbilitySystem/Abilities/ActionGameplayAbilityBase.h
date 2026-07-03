// 文件说明：声明通用 Gameplay Ability 基类的共享接口与配置。

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActionType/ActionAbilityTypes.h"
#include "ActionType/ActionReactionTypes.h"
#include "ActionGameplayAbilityBase.generated.h"

class UActionAbilitySystemComponent;
class UActionCombatReactComponent;
class UGameplayEffect;

/**
 * 通用 GameplayAbility 基类，负责共享的关系系统配置、受击规则解释、
 * 冷却标签合并和基类级生命周期收尾。
 * 它只描述“这条 Ability 在正式规则里是什么”，不持有英雄专属运行时对象，
 * 也不承担具体战斗链的状态源职责。
 */
UCLASS()
class ACTIONRPG_API UActionGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UActionGameplayAbilityBase();

	/** 在 Ability 未激活时，按当前配置把冷却效果直接写回拥有者 ASC。 */
	bool ApplyConfiguredCooldownExternally(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 从当前 ActorInfo 中解析并返回本项目使用的 ActionAbilitySystemComponent。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	UActionAbilitySystemComponent* GetActionAbilitySystemComponentFromActorInfo() const;

	/** 当前 Ability 是否接入统一的关系系统裁决。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	bool UsesAbilityRelationshipSystem() const;

	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	int32 GetAbilityPriority() const;

	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	bool CanBeInterruptedByHigherPriority() const;

	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	bool CanBeInterruptedBySamePriority() const;

	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	bool CanInterruptLowerPriorityAbilities() const;

	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	bool CanInterruptSamePriorityAbilities() const;

	/**
	 * 判断当前 Ability 是否允许在现有受击状态约束下进入激活。
	 * 这是“动态上下文能否放”的判断，不等于静态关系配置本身。
	 */
	bool IsActivationAllowedByCombatReact(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 检查 Ability 配置的受击规则是否与当前受击状态匹配。 */
	bool DoesCombatReactRuleMatchCurrentState(const UActionCombatReactComponent* CombatReactComponent) const;
	/** 当前受击状态是否命中了“覆盖默认打断权限”的那层规则。 */
	bool DoesCombatReactInterruptOverrideMatchCurrentState(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 判断当前 Ability 是否处于受击保护状态，从而不允许被外部逻辑直接打断。 */
	bool IsProtectedByCombatReact(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 结合当前受击状态，判断本 Ability 是否允许打断更低优先级 Ability。 */
	bool CanInterruptLowerPriorityAbilitiesInCurrentReact(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 结合当前受击状态，判断本 Ability 是否允许打断同优先级 Ability。 */
	bool CanInterruptSamePriorityAbilitiesInCurrentReact(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 结合当前受击状态，判断本 Ability 是否允许被更高优先级 Ability 打断。 */
	bool CanBeInterruptedByHigherPriorityInCurrentReact(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 结合当前受击状态，判断本 Ability 是否允许被同优先级 Ability 打断。 */
	bool CanBeInterruptedBySamePriorityInCurrentReact(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 当前 Ability 是否显式允许在恢复阶段通过取消窗口切出。 */
	bool AllowsActivationDuringRecoveryCancelWindow() const;

	/** 解析这条 Ability 在受击 / 取消窗口规则里对应的正式输入标签。 */
	virtual FGameplayTag GetPrimaryInputTagForCombatReact() const;

	const FGameplayTagContainer& GetAbilityIdentityTags() const;
	const FGameplayTagContainer& GetCancelAbilitiesWithTagsForRelationship() const;
	const FGameplayTagContainer& GetActivationBlockedTagsForRelationship() const;
	bool CanCancelAbilitiesWithTags(const FGameplayTagContainer& InAbilityTags) const;
	bool IsBlockedByActivationOwnedTags(const FGameplayTagContainer& InActivationOwnedTags) const;
	const FGameplayTagContainer& GetActivationOwnedTagsForRelationship() const;
	/**
	 * 给子类补一层“关系系统裁决前的运行时前置检查”。
	 * 这里只做补充对象/上下文校验，不替代 GAS 自带激活条件和 ASC 关系裁决。
	 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason);
	FString GetAbilityDebugName() const;
	/** 输出当前 Ability 配置的受击规则描述，便于日志和调试面板查看。 */
	FString DescribeCombatReactRule() const;
	/** 输出当前 Ability 在现有受击状态下是否允许激活的判定说明。 */
	FString DescribeCombatReactActivationDecision(const UActionCombatReactComponent* CombatReactComponent) const;
	/** 输出当前 Ability 在现有受击状态下的打断判定说明。 */
	FString DescribeCombatReactInterruptDecision(const UActionCombatReactComponent* CombatReactComponent) const;

protected:
	/** Ability 被授予到 ASC 时的入口，用来做基类级别的初始化。 */
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** Ability 结束入口：统一收口基类对激活态的清理逻辑。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** 返回本 Ability 应当附加的冷却标签集合，供 GAS 写入激活后的冷却状态。 */
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	/** 按指定 AbilitySpec 上下文执行冷却阻断检查，允许子类按真实输入动态解析冷却标签。 */
	virtual bool CheckCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 应用冷却效果：把配置好的冷却 GE 与时长参数真正写到 ASC。 */
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/** 让子类按当前 Spec / ActorInfo 解析正式冷却标签与时长。 */
	virtual void ResolveAbilityCooldownConfig(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer& OutCooldownTags,
		float& OutCooldownDuration) const;

	/** 判断当前 Ability 是否真的配置了可用的冷却类、标签或时长。 */
	bool HasAbilityCooldownConfig(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

protected:
	/** Ability 的激活策略：输入触发、事件触发，或两者都可触发。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityActivationPolicy")
	EActionAbilityActivationPolicy ActivationPolicy = EActionAbilityActivationPolicy::BothInputAndTriggered;

	/** 是否启用 Ability 关系系统，用于处理优先级、打断与输入冲突。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bUseAbilityRelationshipSystem = false;

	/** Ability 的优先级数值，数值越高越容易在关系系统中占据主导。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	int32 AbilityPriority = 0;

	/** 是否允许当前 Ability 被更高优先级 Ability 打断。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bCanBeInterruptedByHigherPriority = true;

	/** 是否允许当前 Ability 被同优先级 Ability 打断。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bCanBeInterruptedBySamePriority = false;

	/** 是否允许当前 Ability 主动打断更低优先级 Ability。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bCanInterruptLowerPriorityAbilities = false;

	/** 是否允许当前 Ability 主动打断同优先级 Ability。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bCanInterruptSamePriorityAbilities = false;

	/** Ability 在受击系统中的状态匹配规则与保护规则配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	FActionCombatReactAbilityRule CombatReactAbilityRule;

	/** 冷却效果类：Ability 激活后会实例化这个 GE 来承载冷却。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityCooldown")
	TSubclassOf<UGameplayEffect> AbilityCooldownGameplayEffectClass;

	/** 冷却标签集合：用于标记这条 Ability 当前处于冷却中的状态。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityCooldown")
	FGameplayTagContainer AbilityCooldownTags;

	/** 冷却持续时间，供通用冷却 GE 或规格参数读取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityCooldown", meta = (ClampMin = "0.0"))
	float AbilityCooldownDuration = 0.f;

private:
	/** 缓存合并后的冷却标签，避免每次查询都构造新的临时容器。 */
	mutable FGameplayTagContainer CachedCooldownTags;
};
