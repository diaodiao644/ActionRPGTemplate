// 文件说明：声明 ActionAbilitySystemComponent 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionAbilityTypes.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionAbilitySystemComponent.generated.h"

class UActionGameplayAbilityBase;
class UActionCombatReactComponent;
class UDataAsset_StatusEffectDefinition;
class UGameplayAbility;
class UGameplayEffect;

enum class EActionAbilityRelationshipFailureStage : uint8
{
	None,
	PostCancelFinalPrecheck,
	FinalTryActivate
};

enum class EActionAbilityRelationshipFailureKind : uint8
{
	None,
	CandidateSpecMissingDrift,
	// post-cancel 时 CandidateSpec 还在，但已经拿不到可检查的能力对象。
	// 这说明失败点已经从“普通关系拒绝”漂移成了运行态对象漂移。
	CandidateAbilityObjectMissingDrift,
	ActorInfoInvalid,
	AvatarOrNetRoleDrift,
	HeroAvatarTypeDrift,
	UnsupportedBlueprintCanActivateOverrideDrift,
	UnsupportedOnGivenActivationPolicyDrift,
	UnsupportedNetExecutionPolicyDrift,
	UnsupportedReplicationPolicyDrift,
	UnsupportedInstancingPolicyDrift,
	UnsupportedRetriggerInstancedAbilityDrift,
	UnsupportedSourceOrTargetTagRequirementsDrift,
	UnsupportedHeroCombatCategoryAuditDrift,
	// post-cancel 时 owner-side 最终标签门本身失败。
	// 它和“预测标签快照与实际标签快照不一致”不是一回事，后者单独归类到 OwnedTagSnapshotDrift。
	ResidualOwnerTagGateDrift,
	// cancel 前预测的 owner tags 与 cancel 后真实 owner tags 不一致。
	// 这里解释的是“快照漂移”，不是候选 Ability 自己的 owner-side 标签门定义错误。
	OwnedTagSnapshotDrift,
	ResourceDrift,
	CombatReactDrift,
	UserActivationInhibitedDrift,
	AbilityTagsBlockedDrift,
	InputBlockedDrift,
	// 只保留当前确实还无法稳定前移或重分类的残余最终 gate。
	// 它是保底桶，不是常规失败语义。
	UnknownFinalGate,
	CandidateSpecMissingOnFinalTry,
	FinalTryActorInfoInvalid,
	// final TryActivate 尾部门已走到最后，但仍然无法从 resolved spec 拿到能力对象做进一步检查。
	FinalTryAbilityObjectMissing,
	FinalTryExecutionContextRejected,
	FinalTryInstancedPerActorAlreadyActive,
	FinalTryMissingPrimaryInstance,
	// final TryActivate 尾部门在所有已知 probe 都跑完后仍无法解释时的保底桶。
	FinalTryActivateUnknown
};

struct FActionAbilityRelationshipFailureDiagnostic
{
	bool bValid = false;
	FGameplayAbilitySpecHandle CandidateSpecHandle;
	FString CandidateAbilityClassName;
	FString CandidateAbilityDebugName;
	EActionAbilityCategory CandidateCategory = EActionAbilityCategory::None;
	TArray<FString> AbilitiesToCancel;
	FGameplayTagContainer OwnedTagsBeforeCancel;
	FGameplayTagContainer PredictedOwnedTagsAfterCancel;
	FGameplayTagContainer PredictedReleasedTags;
	FGameplayTagContainer OwnedTagsAfterCancel;
	FGameplayTagContainer FinalActivationFailureTags;
	FString CombatReactActivationDecision;
	uint64 FailureSequence = 0;
	float WorldTimeSeconds = 0.f;
	EActionAbilityRelationshipFailureStage FailureStage = EActionAbilityRelationshipFailureStage::None;
	EActionAbilityRelationshipFailureKind FailureKind = EActionAbilityRelationshipFailureKind::None;
	FString FailureReason;

	void Reset()
	{
		bValid = false;
		CandidateSpecHandle = FGameplayAbilitySpecHandle();
		CandidateAbilityClassName.Reset();
		CandidateAbilityDebugName.Reset();
		CandidateCategory = EActionAbilityCategory::None;
		AbilitiesToCancel.Reset();
		OwnedTagsBeforeCancel.Reset();
		PredictedOwnedTagsAfterCancel.Reset();
		PredictedReleasedTags.Reset();
		OwnedTagsAfterCancel.Reset();
		FinalActivationFailureTags.Reset();
		CombatReactActivationDecision.Reset();
		FailureSequence = 0;
		WorldTimeSeconds = 0.f;
		FailureStage = EActionAbilityRelationshipFailureStage::None;
		FailureKind = EActionAbilityRelationshipFailureKind::None;
		FailureReason.Reset();
	}

	bool MatchesSpecHandle(const FGameplayAbilitySpecHandle InSpecHandle) const
	{
		return bValid && CandidateSpecHandle == InSpecHandle;
	}
};

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
	// 外部输入系统不应绕开这里直接逐条 TryActivateAbility，否则会丢掉统一关系门禁与失败口径。
	// `Pressed / Held / Released` 在这里仍只是激活门入口，不替代 `HeroCombatInputComponent` 的正式按钮阶段运行态。
	bool OnAbilityInputPressed(const FGameplayTag& InInputTag);
	bool OnAbilityInputHeld(const FGameplayTag& InInputTag);
	void OnAbilityInputReleased(const FGameplayTag& InInputTag);

public:
	// 能力授予与移除接口。
	UFUNCTION(BlueprintCallable, Category = "Ability", meta = (ToolTip = "把一条 GameplayAbility 授予到当前 ASC，并按需要挂上正式输入标签与等级。它属于统一授予入口，不等于能力已经成功激活。"))
	FGameplayAbilitySpecHandle GrantAbility(const TSubclassOf<UGameplayAbility>& InGrantedAbilities, FGameplayTag AbilityTag = FGameplayTag(), int32 ApplyLevel = 1);

	UFUNCTION(BlueprintCallable, Category = "Ability", meta = (ToolTip = "按 SpecHandle 从当前 ASC 移除一条已授予 Ability。它只处理授予层移除，不替代 Ability 自身的业务收尾。"))
	void RemovedAbility(FGameplayAbilitySpecHandle& InSpecHandlesToRemove);

public:
	// 能力查询与取消接口。
	/** 按动态 Ability 标签查找当前 ASC 上正式应命中的最佳授予 Spec。语义固定为最后授予优先；它返回的是授予层快照，不等于能力当前已经激活。 */
	FGameplayAbilitySpec* GetActivatableAbilitySpecByTag(FGameplayTag AbilityTag);
	/** 按动态 Ability 标签查找当前 ASC 上正式应命中的最佳授予 Spec。const 版本同样只返回授予层快照。 */
	const FGameplayAbilitySpec* GetActivatableAbilitySpecByTag(FGameplayTag AbilityTag) const;
	/** 按动态 Ability 标签先解析最后授予优先的最佳 Spec，再走完整关系裁决链后尝试正式激活。它不是简单的 Spec 查找 helper。 */
	bool TryActivateAbilityByTag(FGameplayTag AbilityTag);
	/** 按 Ability 标签取消当前已激活能力。它只负责激活层取消，不替代能力自身的业务收尾。 */
	void CancelAbilityByTag(FGameplayTag AbilityTag);
	/** 按 Ability 自身标签取消当前已激活能力。它更适合不依赖输入标签的取消场景。 */
	void CancelAbilityByAbilityTag(FGameplayTag AbilityTag);
	/** 按一组 Ability 标签统一取消当前已激活能力。它主要服务关系裁决和高层强切换收尾。 */
	void CancelAbilitiesByAbilityTags(const FGameplayTagContainer& AbilityTags);
	/** 按输入标签解析最后授予优先的最佳授予 Spec，并应用该能力的统一冷却。它只处理冷却提交，不等于能力已经成功激活。 */
	bool ApplyAbilityCooldownByInputTag(const FGameplayTag& AbilityInputTag);

	/** 打印当前玩家已授予的 Hero 战斗 GA 关系配置摘要，用于 OpenMap 实机场景联调核对蓝图默认值。它只服务调试，不推进任何能力状态。 */
	void PrintHeroCombatAbilityRelationshipAudit() const;

	/** 打印当前玩家已授予的 Hero 战斗 GA 类别审计摘要，用于快速发现顶层身份标签缺失、多标签冲突或矩阵规则缺口。它只服务调试，不改写能力类别。 */
	void PrintHeroCombatAbilityCategoryAudit() const;

	/** 按输入标签打印当前真实会命中的最佳 Hero 战斗 GA 的运行时调试摘要，用于解释“为什么能放/为什么不能放”。它返回的是调试解释结果，不是新的裁决状态。 */
	bool PrintHeroCombatAbilityDebugByInputTag(FGameplayTag AbilityInputTag) const;

	/** 按输入标签打印当前真实会命中的最佳 Hero 战斗 GA 的类别审计摘要，用于快速定位身份标签与类别解析问题。 */
	bool PrintHeroCombatAbilityCategoryAuditByInputTag(FGameplayTag AbilityInputTag) const;

	/** 打印最近几次关系主链激活失败历史。它只服务运行时排错，不持久化历史，也不替代整批 relationship audit。 */
	void PrintHeroCombatAbilityRelationshipFailureHistory(int32 MaxEntries = 8) const;

	/**
	 * 按 Ability 自身标签读取“当前已经激活的实例”。
	 * 适用场景：
	 * 1. AnimNotify 需要在运行时定位某个正在执行的 GA；
	 * 2. 不希望由外部系统直接依赖输入 Tag 或 SpecHandle；
	 * 3. 单人项目里先统一通过主实例完成联动，后续若扩展多实例 Ability，再单独细化。
	 *
	 * 注意：
	 * 1. 这里读到的是“当前激活实例快照”；
	 * 2. 它不等于这条 Ability 是否已授予；
	 * 3. 也不等于这条 Ability 当前一定允许再次激活。
	 */
	UFUNCTION(BlueprintPure, Category = "Ability", meta = (ToolTip = "按 Ability 自身标签读取当前已经激活的 Ability 实例。它返回的是运行时激活实例快照，不等于能力是否已授予，也不等于当前一定允许再次激活。"))
	UGameplayAbility* GetActiveAbilityInstanceByAbilityTag(FGameplayTag AbilityTag) const;

public:
	// 状态效果定义与运行时查询接口。
	UFUNCTION(BlueprintPure, Category = "Action|StatusEffect", meta = (ToolTip = "按状态效果标签查找静态定义资产。它主要服务 UI、图标和调试描述补全，不是状态效果是否生效的唯一判断入口。"))
	UDataAsset_StatusEffectDefinition* FindStatusEffectDefinitionByTag(FGameplayTag StatusEffectTag) const;

	/**
	 * 统一应用一份持续战斗修正效果。
	 * 当前用于把霸体、易伤、抗性、处决伤害强化这类“持续一段时间的战斗状态”全部收口到同一份 GE。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|StatusEffect", meta = (ToolTip = "统一应用一份持续战斗修正效果。它是正式运行时应用入口，不建议外围系统各自手拼独立 GE 来表达同类持续战斗状态。"))
	FActiveGameplayEffectHandle ApplyCombatModifierEffect(const FActionCombatModifierEffectSpec& EffectSpec);

	/**
	 * 统一应用一份处决执行保护效果。
	 * 这条入口只服务执行者无敌和 victim lock 这类执行链保护状态，
	 * 与普通 CombatModifier 的区别是：它不会因为自身授予的执行保护标签而触发 GE 自我抑制。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|StatusEffect", meta = (ToolTip = "统一应用一份处决执行保护效果。它只服务执行链保护状态，不建议把普通持续战斗修正也从这里走。"))
	FActiveGameplayEffectHandle ApplyExecutionProtectionEffect(const FActionCombatModifierEffectSpec& EffectSpec);

	/**
	 * 读取当前 ASC 身上所有仍在生效的状态效果快照。
	 * 这份接口主要提供给 UI 与调试层使用，避免它们直接读取底层 ActiveGameplayEffect 结构。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|StatusEffect", meta = (ToolTip = "读取当前 ASC 身上所有仍在生效的状态效果信息快照。它主要服务 UI 与调试展示，不返回新的正式状态源。"))
	void GetActiveStatusEffectInfos(TArray<FActionActiveStatusEffectInfo>& OutStatusEffects) const;

	/** 判断当前 ASC 是否正处于某个指定状态效果之下。 */
	UFUNCTION(BlueprintPure, Category = "Action|StatusEffect", meta = (ToolTip = "判断当前 ASC 是否正处于某个指定状态效果之下。它只回答当前运行时是否命中该标签，不返回静态定义信息。"))
	bool HasActiveStatusEffectTag(FGameplayTag StatusEffectTag) const;

protected:
	// UAbilitySystemComponent 输入回调覆写。
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	/** 按动态标签解析当前正式应命中的最佳授予 Spec。语义固定为“最后授予优先”。 */
	FGameplayAbilitySpec* FindBestActivatableAbilitySpecByDynamicTag(const FGameplayTag& InDynamicTag);
	/** const 版本同样按动态标签解析当前正式应命中的最佳授予 Spec。 */
	const FGameplayAbilitySpec* FindBestActivatableAbilitySpecByDynamicTag(const FGameplayTag& InDynamicTag) const;
	/** 从已授予 AbilitySpec 中按输入标签选出当前最该响应的一条。它只是输入链对统一最佳 spec helper 的转发壳，不在这里直接拍板关系裁决。 */
	FGameplayAbilitySpec* FindBestAbilitySpecByInputTag(const FGameplayTag& InInputTag);

	/**
	 * Hero 主动战斗 GA 的关系激活主链。
	 * 这里固定按“候选解析 -> 关系裁决 -> cancel 前预测 gate -> 统一 cancel ->
	 * post-cancel final precheck -> final TryActivateAbility -> 失败归类与记录”的顺序推进。
	 * 它只维护这一条单向主链，不在取消旧 GA 后做事务式回滚或恢复旧 GA。
	 */
	bool TryActivateAbilitySpecWithRelationship(FGameplayAbilitySpec& Spec);

	/**
	 * 解析候选 Ability 在当前上下文里是否允许激活，并产出需要先取消的旧 Ability 列表。
	 * 这里只处理“默认矩阵 + AbilityInterruptWindow 例外窗口 + 共享运行时硬门禁”这层关系预裁决，
	 * 不真正提交激活，也不替代后面的 GAS 最终激活门。
	 */
	bool ResolveAbilityRelationshipBeforeActivation(
		const FGameplayAbilitySpec& CandidateSpec,
		TArray<FGameplayAbilitySpecHandle>& OutAbilityHandlesToCancel,
		FString& OutFailureReason) const;
	/** 在统一取消旧 GA 之前，只做无副作用的资源预检。它只回答候选能力自己是否有冷却/成本问题。 */
	bool CheckAbilityResourcePreconditionsBeforeRelationshipCancel(
		const FGameplayAbilitySpec& CandidateSpec,
		const UActionGameplayAbilityBase* CandidateAbility,
		FString& OutFailureReason) const;
	/** 在真正 cancel 旧 GA 前，按预测取消后的 owner tags 再做一轮 owner-side 最终标签门预判。它只前移稳定且无副作用的最终 gate，不伪装完整 TryActivate。 */
	bool CheckPredictiveFinalAbilityActivationPreconditionsBeforeRelationshipCancel(
		const FGameplayAbilitySpec& CandidateSpec,
		const UActionGameplayAbilityBase* CandidateAbility,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandlesToCancel,
		FString& OutFailureReason) const;
	/** 在真正 cancel 旧 GA 前，对当前已知稳定且无副作用的共享引擎 / Hero 共性 gate 再做一轮前置预判。它不覆盖需要真实 TryActivateAbility(...) 才能暴露的尾部门。 */
	bool CheckPredictiveSharedFinalActivationPreconditionsBeforeRelationshipCancel(
		const FGameplayAbilitySpec& CandidateSpec,
		const UActionGameplayAbilityBase* CandidateAbility,
		const UActionCombatReactComponent* CombatReactComponent,
		FString& OutFailureReason) const;
	/** 对 Hero 主动战斗关系型 GA 复用授予期白名单校验，避免坏能力形态绕过授予口后再混进 fallback 桶。 */
	bool TryClassifyUnsupportedHeroCombatRelationshipAbilityShape(
		const UGameplayAbility* AbilityToInspect,
		EActionAbilityRelationshipFailureKind& OutFailureKind,
		FString& OutFailureReason) const;
	/** 在旧 GA 已取消后，按 Ability 自己完整的正式激活门再做一轮最终无副作用判定。若这里仍失败，后续只允许走 residual drift 归类，不回滚旧 GA。 */
	bool CheckFinalAbilityActivationPreconditionsAfterRelationshipCancel(
		const FGameplayAbilitySpec& CandidateSpec,
		const UActionGameplayAbilityBase* CandidateAbility,
		FString& OutFailureReason,
		FGameplayTagContainer* OutFailureTags = nullptr) const;
	/** 基于当前 owner tags 和待取消 Ability 列表，构建“预测取消后 owner tags”与“预计释放 tags”两份只读快照。 */
	void BuildPredictedOwnedTagsAfterRelationshipCancel(
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandlesToCancel,
		FGameplayTagContainer& OutPredictedOwnedTags,
		FGameplayTagContainer& OutPredictedReleasedTags) const;
	/** 根据 post-cancel final precheck 失败时的上下文，构建一份统一关系激活失败快照。 */
	FActionAbilityRelationshipFailureDiagnostic BuildRelationshipActivationFailureDiagnosticForPostCancelFinalPrecheck(
		const FGameplayAbilitySpec& CandidateSpec,
		const UActionGameplayAbilityBase* CandidateAbility,
		const UActionCombatReactComponent* CombatReactComponent,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandlesToCancel,
		const FGameplayTagContainer& OwnedTagsBeforeCancel,
		const FGameplayTagContainer& PredictedOwnedTagsAfterCancel,
		const FGameplayTagContainer& PredictedReleasedTags,
		const FGameplayTagContainer& FinalActivationFailureTags,
		const FString& FinalFailureReason) const;
	/** 根据最终 TryActivateAbility(...) 尾部失败时的上下文，构建一份统一关系激活失败快照。 */
	FActionAbilityRelationshipFailureDiagnostic BuildRelationshipActivationFailureDiagnosticForFinalTryActivate(
		const FGameplayAbilitySpec& CandidateSpec,
		const UActionGameplayAbilityBase* CandidateAbility,
		const UActionCombatReactComponent* CombatReactComponent,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandlesToCancel,
		const FGameplayTagContainer& OwnedTagsBeforeCancel,
		const FGameplayTagContainer& PredictedOwnedTagsAfterCancel,
		const FGameplayTagContainer& PredictedReleasedTags,
		const FGameplayTagContainer& FinalActivationFailureTags,
		const FString& FinalFailureReason) const;
	/** 只对 post-cancel residual drift 做分类。它解释的是“cancel 后才暴露”的残余最终 gate，不负责 final TryActivate 尾部门的专有失败。 */
	EActionAbilityRelationshipFailureKind ClassifyPostCancelDriftKind(
		const FGameplayAbilitySpec& CandidateSpec,
		const UActionGameplayAbilityBase* CandidateAbility,
		const UActionCombatReactComponent* CombatReactComponent,
		const FGameplayTagContainer& PredictedOwnedTagsAfterCancel,
		const FGameplayTagContainer& OwnedTagsAfterCancel,
		const FGameplayTagContainer& FinalActivationFailureTags,
		FString& OutFailureReason) const;
	/** 对 final TryActivateAbility(...) 尾部失败做分类。它先复用 InternalTryActivateAbilityFailureTags 和 post-cancel 共享重分类，再补 final-tail probe。 */
	EActionAbilityRelationshipFailureKind ClassifyFinalTryActivateFailureKind(
		const FGameplayAbilitySpec& CandidateSpec,
		const UActionGameplayAbilityBase* CandidateAbility,
		const UActionCombatReactComponent* CombatReactComponent,
		const FGameplayTagContainer& PredictedOwnedTagsAfterCancel,
		const FGameplayTagContainer& FinalActivationFailureTags,
		FString& OutFailureReason) const;
	/** 判断预测取消后的 owner tags 视图与实际取消后的 owner tags 视图是否一致。 */
	bool DoOwnedTagSnapshotsMatch(
		const FGameplayTagContainer& ExpectedOwnedTags,
		const FGameplayTagContainer& ActualOwnedTags) const;
	/** 把待取消 SpecHandle 列表解析成可读能力名，供 drift 日志与调试输出复用。 */
	void BuildAbilityDebugNamesForHandles(
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles,
		TArray<FString>& OutAbilityNames) const;
	/** 组装统一关系激活失败快照的日志 / 调试文本。 */
	FString BuildRelationshipActivationFailureDebugText(const FActionAbilityRelationshipFailureDiagnostic& Diagnostic) const;
	/** 给整批 relationship audit 追加最近一次 matching spec 的失败摘要。它只展示最后一次失败快照，不持久化历史。 */
	FString BuildRelationshipActivationFailureAuditSummaryLine(const FActionAbilityRelationshipFailureDiagnostic& Diagnostic) const;
	/** 组装最近失败历史的单条摘要文本，供专门的失败历史调试命令输出。 */
	FString BuildRelationshipActivationFailureHistoryLine(const FActionAbilityRelationshipFailureDiagnostic& Diagnostic) const;
	/** 把失败阶段枚举转成可读文本，供日志和调试输出复用。 */
	FString DescribeRelationshipActivationFailureStage(EActionAbilityRelationshipFailureStage FailureStage) const;
	/** 把失败类型枚举转成可读文本，供日志和调试输出复用。 */
	FString DescribeRelationshipActivationFailureKind(EActionAbilityRelationshipFailureKind FailureKind) const;
	/** 把一条关系主链失败快照写入最近失败历史，并同步覆盖最后一次失败快照。 */
	void RecordRelationshipActivationFailureDiagnostic(FActionAbilityRelationshipFailureDiagnostic Diagnostic);
	/** 当同一条 Spec 后续成功激活时，清掉仅服务单条 debug / audit 的 LastFailureOnly 快照。 */
	void ClearLastRelationshipActivationFailureDiagnosticIfSupersededBySuccessfulActivation(
		FGameplayAbilitySpecHandle CandidateSpecHandle);
	/** 收集最近几次关系主链激活失败历史的多行文本。 */
	void GetHeroCombatAbilityRelationshipFailureHistoryLines(TArray<FString>& OutHistoryLines, int32 MaxEntries) const;
	/** 从候选授予 Spec 中解析本项目通用 Ability 基类实例。它只服务关系裁决解释，不创建新的激活实例。 */
	UActionGameplayAbilityBase* ResolveActionAbilityFromSpec(const FGameplayAbilitySpec& Spec) const;

	/** 统一整理关系裁决失败文本，供屏幕调试和日志共用同一份口径。 */
	FString BuildRelationshipBlockedDebugText(
		const UActionGameplayAbilityBase* CandidateAbility,
		const UActionCombatReactComponent* CombatReactComponent,
		const FString& FailureReason) const;
	/** 在真正授予前校验这条 Hero 主动战斗 GA 的身份标签、类别解析和矩阵规则是否完整。 */
	bool ValidateGrantedHeroCombatAbilityCategoryBeforeGrant(
		const UGameplayAbility* GrantedAbility,
		const FGameplayTag& AbilityInputTag,
		FString& OutFailureReason,
		FString& OutAuditState) const;
	/** 统一组装授予期类别硬校验失败文本，供 ASC 和调用方日志复用。 */
	FString BuildGrantAbilityCategoryValidationFailureText(
		const UGameplayAbility* GrantedAbility,
		const FGameplayTag& AbilityInputTag,
		const FString& FailureReason,
		const FString& AuditState) const;
	/** 收集 Hero 战斗 Ability 关系审计的多行文本。它只服务调试输出，不参与正式裁决。 */
	void GetHeroCombatAbilityRelationshipAuditLines(TArray<FString>& OutAuditLines) const;
	/** 收集 Hero 战斗 Ability 类别审计的多行文本。它只解释身份标签、类别解析与矩阵规则，不推进任何能力行为。 */
	void GetHeroCombatAbilityCategoryAuditLines(TArray<FString>& OutAuditLines) const;
	/** 组装单条 Hero 战斗 Ability 的审计文本。它只解释当前配置与上下文，不推进任何能力行为。 */
	FString BuildHeroCombatAbilityRelationshipAuditLine(
		const FGameplayAbilitySpec& AbilitySpec,
		const UActionGameplayAbilityBase* Ability,
		const UActionCombatReactComponent* CombatReactComponent) const;
	/** 组装单条 Hero 战斗 Ability 的类别审计文本。它只服务排错解释，不会反向修正身份标签或类别。 */
	FString BuildHeroCombatAbilityCategoryAuditLine(
		const FGameplayAbilitySpec& AbilitySpec,
		const UActionGameplayAbilityBase* Ability) const;
	/** 往效果 Spec 里补状态效果标签。它只服务统一应用链，不单独创建新的状态效果定义。 */
	void AddStatusEffectTagsToSpec(FGameplayEffectSpec& GameplayEffectSpec, const FGameplayTag& StatusEffectTag) const;
	/** 统一组装并应用运行时战斗修正 GE。它是 ApplyCombatModifierEffect / ApplyExecutionProtectionEffect 的内部提交点。 */
	FActiveGameplayEffectHandle ApplyRuntimeCombatModifierEffect(
		const TSubclassOf<UGameplayEffect>& GameplayEffectClass,
		const FActionCombatModifierEffectSpec& EffectSpec);
	/** 把战斗修正规格里的数值写进 GameplayEffectSpec。它只负责 set-by-caller 组装，不承担裁决语义。 */
	void ApplyCombatModifierSetByCallerValues(FGameplayEffectSpec& GameplayEffectSpec, const FActionCombatModifierEffectSpec& EffectSpec) const;

protected:
	/** 当前项目已知的状态效果静态定义列表。UI 和调试层只从这里补展示数据，不把它误读成运行时生效状态集合。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|StatusEffect", meta = (ToolTip = "当前项目已知的状态效果静态定义列表。UI 和调试层可通过它把状态标签补全成名称、颜色和图标；它不是运行时生效状态集合。"))
	TArray<TObjectPtr<UDataAsset_StatusEffectDefinition>> StatusEffectDefinitions;

	/** 当前 ASC 组装持续战斗修正效果时所使用的通用 GE 类型。它是静态提交模板，不是当前已生效效果快照。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|StatusEffect", meta = (ToolTip = "当前 ASC 组装持续战斗修正效果时使用的通用 GameplayEffect 类型。它是静态提交模板，不是当前已生效效果快照。"))
	TSubclassOf<UGameplayEffect> CombatModifierGameplayEffectClass;

	/** 当前 ASC 组装处决执行保护效果时所使用的专用 GE 类型。它只描述正式提交流程的静态模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|StatusEffect", meta = (ToolTip = "当前 ASC 组装处决执行保护效果时使用的专用 GameplayEffect 类型。它只描述正式提交流程的静态模板，不是当前执行保护状态本体。"))
	TSubclassOf<UGameplayEffect> ExecutionProtectionGameplayEffectClass;

	/** 最近一次关系主链激活失败的运行时诊断快照。它只服务现有调试命令读取，不参与正式关系裁决。 */
	FActionAbilityRelationshipFailureDiagnostic LastRelationshipActivationFailureDiagnostic;
	/** 最近几次关系主链激活失败的运行时 ring buffer。它只服务调试命令读取，不持久化，也不复制到网络。 */
	TArray<FActionAbilityRelationshipFailureDiagnostic> RecentRelationshipActivationFailureDiagnostics;
	/** 单调递增的失败序号，只服务运行时调试排序。 */
	uint64 RelationshipActivationFailureSequenceCounter = 0;
};
