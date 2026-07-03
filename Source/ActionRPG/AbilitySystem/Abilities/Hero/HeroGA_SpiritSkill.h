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
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;
	virtual FGameplayTag GetPrimaryInputTagForCombatReact() const override;
	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void ResolveAbilityCooldownConfig(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer& OutCooldownTags,
		float& OutCooldownDuration) const override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void OnHeroMontageCompleted(FName MontageContext) override;
	virtual void OnHeroMontageBlendOut(FName MontageContext) override;
	virtual void OnHeroMontageInterrupted(FName MontageContext) override;
	virtual void OnHeroMontageCancelled(FName MontageContext) override;

private:
	/** 读取当前这次激活真正绑定的 Spirit 输入标签。 */
	FGameplayTag GetCurrentSpiritSkillInputTag() const;

	/** 按指定 AbilitySpec 上下文解析这次 Spirit 技能真正应使用的冷却标签。 */
	FGameplayTag ResolveSpiritSkillCooldownTagForContext(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 解析当前装备灵武器的主动技能配置，并做最小运行时防御校验。 */
	bool ResolveCurrentSpiritSkillConfig(
		const UDataAsset_WeaponDefinition*& OutWeaponDefinition,
		FActionSpiritAbilityEntryConfig& OutEntryConfig,
		FString& OutFailureReason);

	/** 开始播放指定技能段，并把当前运行时命中/发射物状态切到这一段。 */
	bool StartSpiritSkillClip(int32 ClipIndex);

	/** 统一处理当前技能段完成、断链或被打断后的后续流转。 */
	void HandleSpiritSkillClipFinished(bool bWasCancelled);

	/** 当前运行态是否允许在同一次激活内直接接到下一段。 */
	bool CanAdvanceSpiritClipWithinCurrentActivation() const;

	/** 链窗口内命中同输入时，立即停旧段并切到下一段。 */
	bool TryStartImmediateNextSpiritClip();

	/** 只在真正需要时提交一次 Spirit 技能冷却。 */
	bool CommitSpiritSkillCooldownIfNeeded();

	/** 结束或中断时清掉当前 Spirit 输入对应的残留缓冲输入。 */
	void ClearBufferedSpiritSkillInputIfAny();

	/** 统一结束灵武器技能壳，避免蒙太奇多个回调重复收尾。 */
	void FinishSpiritSkillAbility(bool bWasCancelled);

private:
	/** 防止 Completed / BlendOut / Interrupted / Cancelled 多次重复结束 Ability。 */
	bool bAbilityFinished = false;

	/** 当前这次激活所使用的调试名。 */
	FString ActiveSpiritSkillDebugName;

	/** 当前这次激活真正绑定的 Spirit 输入标签。 */
	FGameplayTag ActiveSpiritInputTag;

	/** 当前这次激活是否把 Spirit Offensive 的命中/发射物运行时写进了攻击组件。 */
	bool bAppliedSpiritOffensiveRuntimeState = false;

	/** 当前正在执行的 Spirit Ability 条目快照。 */
	FActionSpiritAbilityEntryConfig ActiveSpiritAbilityEntryConfig;

	/** 当前这次激活对应的武器定义。 */
	const UDataAsset_WeaponDefinition* ActiveSpiritWeaponDefinition = nullptr;

	/** 当前技能段索引。 */
	int32 ActiveSkillClipIndex = INDEX_NONE;

	/** 当前技能段总数。 */
	int32 ActiveSkillClipCount = 0;

	/** 当前段结束后是否应推进到下一段。这里只表示“本次激活内的接段意图”，不替代组件侧持久索引。 */
	bool bPendingAdvanceToNextClip = false;

	/** 当前段的完成回调是否已经处理过，避免 Completed / BlendOut 重复收尾。 */
	bool bHandledCurrentClipFinish = false;

	/** 当前 Spirit 技能冷却是否已经正式提交。 */
	bool bSpiritCooldownCommitted = false;

	/** 当前是否正在执行 Spirit 自己发起的内部立即切段。 */
	bool bImmediateSpiritClipTransitionInProgress = false;
};
