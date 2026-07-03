#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "ActionType/ActionAnimationTypes.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "HeroGA_Attack.generated.h"

/**
 * 攻击 Ability 共享基类。
 * 这层只负责“本次攻击激活”的通用时序：
 * 1. 校验角色、战斗组件、当前武器定义与请求标签是否有效；
 * 2. 按请求标签解析攻击分支、连段索引与动画配置；
 * 3. 统一播放真正攻击蒙太奇，正式启动一次攻击执行；
 * 4. 在动作结束时把收尾统一交回攻击组件与战斗组件。
 *
 * 它不持有长期攻击状态源。
 * 正式攻击运行态、连段推进、命中窗口与最终重置，仍由 HeroAttackComponent /
 * HeroCombatComponent 维护；这个 GA 只缓存“本次是否已正式出招、
 * 结束时是否需要回退连段”这类短生命周期时序标记。
 *
 * “这是空手攻击、近战攻击、远程攻击还是混合攻击”这类差异，
 * 不在这个基类里硬编码，而是由各个派生 GA 在构造时写入：
 * 1. 自己对应的攻击请求标签；
 * 2. 自己对应的 AbilityTag；
 * 3. 自己只允许在哪个固定武器槽中被激活。
 */
UCLASS()
class ACTIONRPG_API UHeroGA_Attack : public UActionHeroGameplayAbility
{
	GENERATED_BODY()

public:
	UHeroGA_Attack();

public:
	/** Ability 激活入口：重置本次攻击的局部时序状态，并驱动完整的攻击解析与执行流程。 */
	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Ability 结束入口：把所有结束路径统一收束到一次正式收尾，再交给父类结束能力。 */
	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** 在关系系统真正放行前，先校验当前武器槽、武器定义、分支配置与特殊请求资格。这里只做补充预检，不写正式运行态。 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;

	/** 蒙太奇自然播完时的回调。它不直接写业务状态，只负责把结束路径汇总到统一收尾口。 */
	virtual void OnHeroMontageCompleted(FName MontageContext) override;

	/** 蒙太奇进入 BlendOut 时的回调。某些攻击会先收到 BlendOut 再收到 Completed，因此这里只做汇流。 */
	virtual void OnHeroMontageBlendOut(FName MontageContext) override;

	/** 蒙太奇被中断时的回调。 */
	virtual void OnHeroMontageInterrupted(FName MontageContext) override;

	/** 蒙太奇被取消时的回调。 */
	virtual void OnHeroMontageCancelled(FName MontageContext) override;

	/** 对外统一的收尾入口，避免多个蒙太奇回调重复触发正式收尾，再统一回到 EndAbility。 */
	void FinishAttackAbility();

	/** 真正执行攻击收尾：清理本次攻击的正式运行态、伤害上下文与连段最终决策。 */
	void FinalizeAttackAbility();

	/** 读取这条攻击 GA 当前绑定的输入标签，仅用于调试和日志输出。 */
	FGameplayTag GetCurrentAttackInputTag() const;

	/** 结合当前武器定义与请求标签，解析并执行一次完整攻击。这一步负责把“这次想按什么键出什么招”翻译成具体分支、蒙太奇、命中配置与连段推进方式。 */
	bool TryResolveAndExecuteAttack();

	/** 执行一份已经解析完成的攻击配置。进入这里说明分支解析已经成功，后续只负责正式播放、写运行时状态并推进组件状态源。 */
	bool ExecuteResolvedAttack(const FActionResolvedAttackExecutionConfig& InResolvedConfig);

	/** 冲刺攻击起手时，优先按当前移动输入方向修正角色朝向。 */
	bool TryApplySprintFacingFromMoveInput();

	/** 供派生类写入自己的攻击请求标签。 */
	void SetAttackRequestTag(const FGameplayTag& InRequestTag);

	/** 供派生类写入自己的专属 AbilityTag。 */
	void SetSpecificAttackAbilityTag(const FGameplayTag& InAbilityTag);

	/** 供派生类写入自己所属的固定武器槽。 */
	void SetExpectedLoadoutSlot(EHeroWeaponLoadoutSlot InExpectedLoadoutSlot);

	/** 派生类构造时一次性写入请求标签、能力标签与目标武器槽。 */
	void InitializeAttackAbility(
		const FGameplayTag& InRequestTag,
		const FGameplayTag& InAbilityTag,
		EHeroWeaponLoadoutSlot InExpectedLoadoutSlot);

protected:
	/** 当前 GA 绑定的攻击请求标签。它只定义“这次请求哪类攻击”，不直接等于最终解析出的分支。 */
	FGameplayTag AttackRequestTag;

	/** 当前 GA 绑定的专属 AbilityTag。主要用于调试、关系系统与运行时能力定位。 */
	FGameplayTag SpecificAttackAbilityTag;

	/** 当前 GA 只允许在哪个固定武器槽中被激活。Invalid 表示不额外限制。 */
	EHeroWeaponLoadoutSlot ExpectedLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	/** 防止多个蒙太奇结束回调重复进入攻击收尾。它只描述“本次收尾是否已经执行过”。 */
	bool bAttackAbilityFinished = false;

	/**
	 * 标记本次攻击结束时，若没有成功衔接到下一段，
	 * 是否需要在最终收尾阶段自动重置连段索引。
	 * 这是延后决策标记，不是起手时立即执行的重置命令。
	 */
	bool bShouldResetComboIndexOnFinalizeIfNotChained = false;

	/** 标记本次是否已经真正开始播放攻击蒙太奇。它用来区分“只过渡了”与“正式出招了”。 */
	bool bAttackExecutionStarted = false;

	/** 标记本次激活是否已经尝试过一次简单转向辅助，避免同次激活内重复纠正朝向。 */
	bool bSimpleTurnAssistAttempted = false;

	/**
	 * 记录这次闪避反击请求是否已经在激活阶段通过资格校验。
	 * 这样就算本次激活中间插入别的短时序检查，也不会把原本合法的反击机会误判丢失。
	 * 这份缓存只服务本次激活；一旦真正进入攻击执行，就不再继续保留。
	 */
	bool bDodgeCounterExecutionAuthorized = false;
};
