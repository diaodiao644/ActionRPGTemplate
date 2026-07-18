// 文件说明：声明灵武器专属主动技能壳 Ability。

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "HeroGA_SpiritSkill.generated.h"

/**
 * Spirit 主动技能壳只负责“当前这次激活”的短生命周期流程：
 * 解析这次输入命中的条目、启动本段动画、处理本段结束与 Ability 收尾。
 * 真正跨激活持久存在的段索引、待命态与超时计时器统一放在 HeroCombatComponent，
 * 这样同一个 Spirit 输入在插入 Attack / 其它 Spirit 后，仍能回到同一份正式状态源继续推进。
 */
UCLASS()
class ACTIONRPG_API UHeroGA_SpiritSkill : public UActionHeroGameplayAbility
{
	GENERATED_BODY()

public:
	UHeroGA_SpiritSkill();

protected:
	/** 在关系系统真正放行前，先校验当前 Spirit 输入、武器定义和条目配置。这里只做补充预检，不写持久 Spirit 状态。 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;
	/** 解析这次 Spirit 主动技能在受击/取消窗口规则里对应的正式输入标签。 */
	virtual FGameplayTag GetPrimaryInputTagForCombatReact() const override;
	/** 输入按下时的补充入口。它主要服务链窗口内立即切段，不单独形成新的输入状态源。 */
	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** Ability 激活入口：解析本次输入命中的 Spirit 条目并启动当前技能段。它不在这里持有跨激活 Spirit 状态源。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** 按这次激活真正命中的 Spirit 条目解析冷却标签与冷却时长。返回的是这次激活的提交口径，不是持久冷却状态。 */
	virtual void ResolveAbilityCooldownConfig(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer& OutCooldownTags,
		float& OutCooldownDuration) const override;

	/** Ability 结束入口：统一兜底当前段与当前激活的正式收尾，不持久保存跨激活 Spirit 状态。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** 当前段自然播完时的共享回调。它只负责汇流到统一段收尾口。 */
	virtual void OnHeroMontageCompleted(FName MontageContext) override;
	/** 当前段 BlendOut 时的共享回调。它只汇流到统一段收尾，不额外定义新的段状态。 */
	virtual void OnHeroMontageBlendOut(FName MontageContext) override;
	/** 当前段被中断时的共享回调。它只汇流到统一段收尾，不额外定义新的段状态。 */
	virtual void OnHeroMontageInterrupted(FName MontageContext) override;
	/** 当前段被取消时的共享回调。它只汇流到统一段收尾，不额外定义新的段状态。 */
	virtual void OnHeroMontageCancelled(FName MontageContext) override;

private:
	/** 显式回收当前 Spirit 段打开的链窗，避免内部切段或异常收尾时残留到下一段。 */
	void CloseSpiritChainWindowIfNeeded();

	/** 读取当前这次激活真正绑定的 Spirit 输入标签。它是本次激活局部快照，不替代组件侧跨激活持久状态。 */
	FGameplayTag GetCurrentSpiritSkillInputTag() const;

	/** 按指定 AbilitySpec 上下文解析这次 Spirit 技能真正应使用的冷却标签。它只解释“本次该挂什么冷却”，不提交冷却本身。 */
	FGameplayTag ResolveSpiritSkillCooldownTagForContext(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 解析当前装备灵武器的主动技能配置，并做最小运行时防御校验。成功后会产出本次激活要消费的局部条目快照；它不把条目结果保存成跨激活状态。 */
	bool ResolveCurrentSpiritSkillConfig(
		const UDataAsset_WeaponDefinition*& OutWeaponDefinition,
		FActionSpiritAbilityEntryConfig& OutEntryConfig,
		FString& OutFailureReason);

	/** 开始播放指定技能段，并把当前运行时命中/发射物状态切到这一段。它只处理“这一次”技能段切换，不持有跨激活段状态。 */
	bool StartSpiritSkillClip(int32 ClipIndex);

	/** 统一处理当前技能段完成、断链或被打断后的后续流转。它会决定是结束本次激活，还是推进到下一段；这里读写的都是本次激活局部流程标记。 */
	void HandleSpiritSkillClipFinished(bool bWasCancelled);

	/** 当前运行态是否允许在同一次激活内直接接到下一段。这里判断的是当前激活内的短时序资格，不替代组件侧持久段状态。 */
	bool CanAdvanceSpiritClipWithinCurrentActivation() const;

	/** 链窗口内命中同输入时，立即停旧段并切到下一段。它只服务本次激活内的立即切段。 */
	bool TryStartImmediateNextSpiritClip();

	/** 只在真正需要时提交一次 Spirit 技能冷却。它只负责本次激活内的冷却提交保护。 */
	bool CommitSpiritSkillCooldownIfNeeded();

	/** 结束或中断时清掉当前 Spirit 输入对应的残留缓冲输入，避免旧段输入串到后续动作。 */
	void ClearBufferedSpiritSkillInputIfAny();

	/** 统一结束灵武器技能壳，避免蒙太奇多个回调重复收尾。 */
	void FinishSpiritSkillAbility(bool bWasCancelled);

private:
	/** 防止 Completed / BlendOut / Interrupted / Cancelled 多次重复结束 Ability。 */
	bool bAbilityFinished = false;

	/** 当前这次激活所使用的调试名。它只服务日志和调试说明。 */
	FString ActiveSpiritSkillDebugName;

	/** 当前这次激活真正绑定的 Spirit 输入标签。它是当前激活局部快照，不是跨激活持久输入状态。 */
	FGameplayTag ActiveSpiritInputTag;

	/** 当前这次激活是否把 Spirit Offensive 的命中/发射物运行时写进了攻击组件。它只保护本次激活内的写入/回收边界。 */
	bool bAppliedSpiritOffensiveRuntimeState = false;

	/** 当前正在执行的 Spirit Ability 条目快照。它只描述本次激活命中的那一条 Spirit 配置。 */
	FActionSpiritAbilityEntryConfig ActiveSpiritAbilityEntryConfig;

	/** 当前这次激活对应的武器定义。它是当前激活局部读取结果，不替代正式已装备武器状态源。 */
	const UDataAsset_WeaponDefinition* ActiveSpiritWeaponDefinition = nullptr;

	/** 当前技能段索引。它是当前激活局部快照，不替代组件侧跨激活段索引。 */
	int32 ActiveSkillClipIndex = INDEX_NONE;

	/** 当前技能段总数。它只服务本次激活内的段切判定。 */
	int32 ActiveSkillClipCount = 0;

	/** 当前段结束后是否应推进到下一段。这里只表示“本次激活内的接段意图”，不替代组件侧持久索引。 */
	bool bPendingAdvanceToNextClip = false;

	/** 当前段的完成回调是否已经处理过，避免 Completed / BlendOut 重复收尾。 */
	bool bHandledCurrentClipFinish = false;

	/** 当前 Spirit 技能冷却是否已经正式提交。它只保护本次激活内不重复提交。 */
	bool bSpiritCooldownCommitted = false;

	/** 当前是否正在执行 Spirit 自己发起的内部立即切段。它只服务同次激活内的短时序保护。 */
	bool bImmediateSpiritClipTransitionInProgress = false;
};
