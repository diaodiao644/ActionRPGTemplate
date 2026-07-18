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

	/** 在 Ability 未激活时，按当前配置把冷却效果直接写回拥有者 ASC。它只复用这条 Ability 的正式冷却出口，不等于当前 Ability 已正式激活。 */
	bool ApplyConfiguredCooldownExternally(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 从当前 ActorInfo 中解析并返回本项目使用的 ActionAbilitySystemComponent。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从当前 ActorInfo 中解析本项目使用的 ActionAbilitySystemComponent。它只是共享宿主解析入口，不会创建新的 ASC，也不替代 Ability 自身的正式状态。"))
	UActionAbilitySystemComponent* GetActionAbilitySystemComponentFromActorInfo() const;

	/** 当前 Ability 是否接入统一的关系系统裁决。关闭后，它仍可被 GAS 正常激活，但不会参与本项目自定义优先级/打断门禁。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "当前 Ability 是否接入本项目的统一关系系统裁决。关闭后，它仍可按 GAS 正常激活，但不会参与自定义优先级、打断和输入冲突门禁。"))
	bool UsesAbilityRelationshipSystem() const;

	/** 读取当前 Ability 在主动 GA 统一关系矩阵中的正式能力类别。它默认从顶层 Ability 身份标签解析，不新建第二套标签体系。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "读取当前 Ability 在主动 GA 统一关系矩阵中的正式能力类别。它默认从 AbilityTags 里的顶层 Player.Ability.* 身份标签解析，不新建第二套标签体系。"))
	virtual EActionAbilityCategory GetAbilityCategory() const;

	/**
	 * 判断当前 Ability 是否允许在现有受击状态约束下进入激活。
	 * 这是“动态上下文能否放”的判断，不等于静态关系配置本身。
	 */
	bool IsActivationAllowedByCombatReact(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 检查 Ability 配置的受击规则是否与当前受击状态匹配。这里只解释规则命中，不推进正式激活。 */
	bool DoesCombatReactRuleMatchCurrentState(const UActionCombatReactComponent* CombatReactComponent) const;
	/** 当前受击状态是否命中了“覆盖默认打断权限”的那层规则。 */
	bool DoesCombatReactInterruptOverrideMatchCurrentState(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 判断当前 Ability 是否处于受击保护状态，从而不允许被外部逻辑直接打断。 */
	bool IsProtectedByCombatReact(const UActionCombatReactComponent* CombatReactComponent) const;

	/** 当前 Ability 是否显式允许在恢复阶段通过取消窗口切出。它读取的是静态规则命中结果，不会主动打开或关闭取消窗口。 */
	bool AllowsActivationDuringRecoveryCancelWindow() const;

	/** 解析这条 Ability 在受击 / 取消窗口规则里对应的正式输入标签。它主要服务关系裁决、恢复窗口和日志解释。 */
	virtual FGameplayTag GetPrimaryInputTagForCombatReact() const;

	/** 读取这条 Ability 自身的正式身份标签集合。它们是关系裁决和查询入口，不是新的运行时状态源。 */
	const FGameplayTagContainer& GetAbilityIdentityTags() const;
	/** 当前 Ability 是否使用授予即激活语义。它只服务授予链校验和调试解释，不直接触发激活。 */
	bool UsesOnGivenActivationPolicy() const;
	/** 当前 Ability 是否存在蓝图 CanActivate 覆写。它只服务授予期和 editor-only 治理链的硬约束，不改变正式激活语义。 */
	bool HasBlueprintCanActivateOverride() const;
	/** 当前 Ability 是否启用了 InstancedPerActor 重触发配置。它只服务白名单能力形态治理，不参与正式关系裁决。 */
	bool UsesRetriggerInstancedAbility() const;
	/** 当前 Ability 是否仍配置了 Source / Target 激活标签门。它只服务白名单能力形态治理，不把这些标签门纳入正式关系主链。 */
	bool HasSourceOrTargetActivationTagRequirements() const;
	/** 输出当前 Ability 配置的 Source / Target 激活标签门摘要。它只服务授予期与 editor 校验解释。 */
	FString DescribeSourceAndTargetActivationTagRequirements() const;
	/**
	 * 给子类补一层“关系系统裁决前的运行时前置检查”。
	 * 这里只做共享硬门禁之外、又必须在 relationship cancel 前先知道的运行时依赖 / 业务前置校验，
	 * 例如组件是否存在、当前武器上下文是否有效、当前目标 / 资源是否至少满足最小起手资格。
	 * 它不替代 GAS 自带激活条件，也不替代 ASC 默认矩阵或 AbilityInterruptWindow 的关系裁决权威。
	 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason);
	/** 构建当前 Ability 的调试名，供日志和调试面板复用。它只服务解释链，不形成新的正式状态。 */
	FString GetAbilityDebugName() const;
	/** 输出当前 Ability 配置的受击规则描述，便于日志和调试面板查看。 */
	FString DescribeCombatReactRule() const;
	/** 输出当前 Ability 在现有受击状态下是否允许激活的判定说明。它是调试解释结果，不是新的关系裁决状态源。 */
	FString DescribeCombatReactActivationDecision(const UActionCombatReactComponent* CombatReactComponent) const;
	/** 输出当前 Ability 在现有受击状态下的打断判定说明。它是调试解释结果，不会反向改写正式规则。 */
	FString DescribeCombatReactInterruptDecision(const UActionCombatReactComponent* CombatReactComponent) const;

protected:
	/** Ability 被授予到 ASC 时的入口，用来做基类级别的初始化。它只补共享配置整理，不在这里创建英雄侧运行时引用。 */
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** Ability 结束入口：统一收口基类对激活态的清理逻辑；它不替代各 Hero 主动 GA 自己的业务收尾。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** 返回本 Ability 应当附加的冷却标签集合，供 GAS 写入激活后的冷却状态。若子类按当前输入动态改冷却，这里也会走同一条正式出口。 */
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	/** 按指定 AbilitySpec 上下文执行冷却阻断检查，允许子类按真实输入动态解析冷却标签。这里只判断冷却门禁，不承担关系裁决。 */
	virtual bool CheckCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 只给 ASC 两阶段激活预检用的无副作用资源检查桥。它只检查冷却和成本，不替代完整激活门。 */
	bool CheckPreCancelResourcePreconditions(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const;

	/** 预测这条 Ability 如果此刻被关系链取消，会从 owner 身上带走哪些正式标签。它只服务取消前预判，不创建第二套运行态。 */
	virtual void GetPredictedOwnedTagsToReleaseOnRelationshipCancel(
		FGameplayTagContainer& OutOwnedTags) const;

	/** 按一份预测 owner tags 快照，检查当前 Ability 的 owner-side tag gate 是否仍会拒绝激活。 */
	bool CheckPredictiveFinalActivationTagGateAgainstOwnedTags(
		const FGameplayTagContainer& PredictedOwnedTags,
		FString& OutFailureReason,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const;

	/** 应用冷却效果：把配置好的冷却 GE 与时长参数真正写到 ASC。它是统一正式出口，不创造第二套冷却状态。 */
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/** 让子类按当前 Spec / ActorInfo 解析正式冷却标签与时长。返回的是这次请求要提交的解析结果，不是持久运行时状态。 */
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
	/** Ability 的静态激活语义模板：回答“通常通过什么入口激活”。它不是运行时激活状态，也不等于当前这次请求一定能通过资格裁决。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityActivationPolicy", meta = (ToolTip = "这条 Ability 的静态激活语义模板，例如输入触发、事件触发或两者都可触发。它不是运行时激活状态，也不等于当前一定能成功激活。"))
	EActionAbilityActivationPolicy ActivationPolicy = EActionAbilityActivationPolicy::BothInputAndTriggered;

	/** 是否启用 Ability 关系系统，用于处理优先级、打断与输入冲突。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship", meta = (ToolTip = "是否启用 Ability 关系系统。启用后，这条 Ability 会参与优先级、打断和输入冲突裁决。"))
	bool bUseAbilityRelationshipSystem = false;

	/** Ability 在受击系统中的状态匹配规则与保护规则配置。它是静态关系模板，不是当前受击阶段镜像，也不是取消窗口本体。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship", meta = (ToolTip = "Ability 在受击系统中的阶段匹配、保护和恢复开窗关系模板。它只描述静态规则，不是当前受击阶段镜像，也不替代 CombatReact 或 AbilityWindow 正式状态源。"))
	FActionCombatReactAbilityRule CombatReactAbilityRule;

	/** 冷却效果类：Ability 激活后会实例化这个 GE 来承载冷却。它只是正式冷却出口的静态配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityCooldown", meta = (ToolTip = "这条 Ability 提交流却时实例化的 GameplayEffect 类。它只是正式冷却出口的静态配置，不表示当前一定已经进入冷却。"))
	TSubclassOf<UGameplayEffect> AbilityCooldownGameplayEffectClass;

	/** 冷却标签集合：用于标记这条 Ability 当前处于冷却中的状态。它们是提交时要写入 ASC 的正式标签模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityCooldown", meta = (ToolTip = "这条 Ability 提交流却后写入 ASC 的冷却标签集合。它们是正式冷却标签模板，不是 Ability 自己维护的第二套激活状态。"))
	FGameplayTagContainer AbilityCooldownTags;

	/** 冷却持续时间静态模板，供通用冷却 GE 或规格参数读取。它不是当前剩余冷却时间快照。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityCooldown", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "冷却持续时间静态模板，单位秒。通用冷却 GE 和外部补冷却逻辑都会读取它；它不是当前剩余冷却时间快照。"))
	float AbilityCooldownDuration = 0.f;

private:
	/** 缓存合并后的冷却标签，避免每次查询都构造新的临时容器。它只是基类 helper 缓存，不构成第二套冷却状态源。 */
	mutable FGameplayTagContainer CachedCooldownTags;

	friend class UActionAbilitySystemComponent;
};
