#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionAnimationTypes.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionProjectileTypes.h"
#include "Components/ActorComponent.h"
#include "HeroAttackComponent.generated.h"

class AActionHeroCharacter;
class AActionProjectileBase;
class AHeroWeaponBase;
class UActionAbilitySystemComponent;
class UActionAttributeSetBase;
class UAnimMontage;
class UDataAsset_WeaponDefinition;
class UHeroCombatComponent;
class UHeroCombatInputComponent;
class UHeroLoadoutContextComponent;
class UHeroLoadoutEffectComponent;
class USkeletalMeshComponent;

/**
 * 英雄攻击组件。
 * 这一层先把攻击请求解析、攻击上下文缓存、攻击输入提交与攻击收尾恢复
 * 从 HeroCombatComponent 中拆出来，降低总战斗组件的职责密度。
 * 它是攻击请求解析、当前攻击段执行期快照和攻击恢复链的正式宿主，
 * 但这些缓存都只服务“一次攻击执行期”，不替代 WeaponDefinition 静态资产入口、
 * HeroCombatComponent 的高层输入总分发，或 HeroCombatInputComponent 的正式输入状态。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroAttackComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UHeroCombatComponent;

public:
	UHeroAttackComponent();

public:
	// 攻击请求解析与执行配置查询：
	// 负责把输入或请求标签解析成攻击分支、蒙太奇上下文以及当前段执行配置。
	// 返回的是“这次读取到的执行计划”，不是长期攻击状态。
	bool TryResolveAttackExecutionConfigByRequestTag(
		const FGameplayTag& AttackRequestTag,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const;
	bool TryResolveAttackExecutionConfig(
		const FGameplayTag& InBranchTag,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const;

	/** 将攻击分支标签映射为蒙太奇上下文名，供动画播放、结束回调和中断判断统一复用。 */
	static FName GetMontageContextNameForAttackBranch(const FGameplayTag& InBranchTag);

	// 攻击请求语义解析：
	// 负责把输入还原成正式攻击请求，并判断当前输入是否应升级成冲刺攻击。
	// 这一层只解释“当前该走哪条攻击入口”，不在这里落长期命中或连段结果。
	FGameplayTag ResolveScopedAttackAbilityInputTag(const FGameplayTag& AttackRequestTag) const;
	bool ShouldTreatAttackInputAsSprintAttack() const;
	FGameplayTag ResolveAttackRequestTag(FGameplayTag InputTag) const;
	bool TryResolveAttackBranchTag(FGameplayTag InputTag, FGameplayTag& OutBranchTag) const;
	bool TryResolveCurrentAttackExecutionConfig(
		FGameplayTag InputTag,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const;

	// 攻击执行开始同步：
	// 当一次攻击真正准备开播时，通过这里把本段解析结果、权限状态与局部运行时缓存写回组件。
	/** 同步一次已经解析完成的攻击执行状态。它写入的是“这一次攻击”的局部快照，不形成跨激活持久状态源。 */
	void ApplyResolvedAttackExecutionState(const FActionResolvedAttackExecutionConfig& InResolvedConfig);
	/** 在真正开播前复核这次攻击当前是否仍被正式允许执行。 */
	bool IsAttackExecutionStillAuthorized(
		const FGameplayTag& AttackRequestTag,
		bool bHasLocalDodgeCounterAuthorization) const;
	/** 标记指定攻击请求已经正式进入执行链。 */
	void ApplyAttackExecutionStartedForRequest(const FGameplayTag& AttackRequestTag);

	// 攻击命中配置与伤害载荷：
	// 负责保存当前攻击段命中参数，并在命中时组装即时伤害载荷。
	// 这里消费的是“当前攻击段已生效配置”，不反向创造第二套武器资产状态。
	void SetCurrentAttackHitConfig(const FActionWeaponHitConfig& InHitConfig);
	void ClearCurrentAttackHitConfig();
	/** 读取当前攻击段命中配置快照。它只服务本次攻击周期，不代表武器长期默认值。 */
	bool TryGetCurrentAttackHitConfigSnapshot(FActionWeaponHitConfig& OutHitConfig) const;
	void SetCurrentAttackProjectileSpawnConfig(const FActionProjectileSpawnConfig& InProjectileSpawnConfig);
	void ClearCurrentAttackProjectileSpawnConfig();
	/** 输出当前攻击段发射物解析结果，主要服务调试与日志，不代表发射物已经正式生成。 */
	bool TryGetCurrentAttackProjectileDebugInfo(FActionProjectileResolutionDebugInfo& OutDebugInfo) const;
	/** 基于当前攻击段运行时缓存，拼出一次正式命中结算要用的完整伤害载荷。 */
	bool TryBuildCurrentAttackDamagePayload(AActor* InTargetActor, FActionDamagePayload& OutDamagePayload) const;
	bool TryBuildCurrentAttackDamagePayloadForHitSource(
		AActor* InTargetActor,
		FName InPreferredHitSourceId,
		FName InPreferredSourceComponentName,
		FActionDamagePayload& OutDamagePayload) const;
	bool TrySpawnCurrentAttackProjectile();
	/** 攻击开播失败时恢复连段索引，避免这次失败把上一段正式结果冲掉。 */
	void RestoreComboIndexAfterFailedAttackStart();
	/** 收尾当前攻击局部运行态，并按条件决定是否回零连段索引。 */
	void FinalizeAttackExecutionRuntime(bool bShouldResetComboIndexOnFinalizeIfNotChained);

	// 连段保留与闪反授权：
	// 负责记录“这次结束后是否保留连段索引”以及闪避反击的局部授权状态。
	void MarkPreserveComboIndexOnNextAttackFinalize();
	bool ShouldPreserveComboIndexOnNextAttackFinalize() const;
	bool ConsumePreserveComboIndexOnNextAttackFinalize();

	/** 当前是否还保留着一份“仅对下一次攻击生效”的闪避反击授权。 */
	bool HasPendingDodgeCounterExecutionAuthorization() const;
	void SetPendingDodgeCounterExecutionAuthorization(bool bAuthorized);
	bool ConsumePendingDodgeCounterExecutionAuthorization();

	// 攻击输入缓冲与恢复：
	// 负责攻击输入的即时门禁、写入缓冲、恢复消费以及 Held / Released 回放。
	/** 当前这一帧是否允许把攻击输入直接送去激活 Ability，不经过缓冲恢复。 */
	bool CanActivateAttackInputNow(const FGameplayTag InputTag) const;
	/** 当前若不能立刻出招，这条攻击输入是否值得先缓存下来，等待后续恢复链再消费。 */
	bool ShouldBufferAttackInputNow(const FGameplayTag& InputTag) const;
	bool TryQueueBufferedAttackInput(
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent,
		bool bCanBuffer,
		FGameplayTag ResolvedAttackRequestTag = FGameplayTag());
	/** 已经写入缓冲的攻击输入，在当前时刻是否满足正式恢复消费条件。 */
	bool CanConsumeBufferedAttackInputNow(const FActionBufferedInput& BufferedInput) const;
	/** 把一条已缓存的攻击输入重新翻译成正式请求，并按当前门禁尝试恢复执行。 */
	bool TryConsumeBufferedAttackInput();
	bool HandleAttackInputReleased(const FGameplayTag& InputTag);
	bool CanReplayHeldAttackInput(const FGameplayTag& InputTag) const;
	bool TryReplayHeldAttackInputAfterRecovery();
	bool TryResolveBufferedAttackConsumeSpec(
		const FActionBufferedInput& BufferedInput,
		FGameplayTag& OutAttackRequestTag,
		EActionInputEvent& OutTriggerInputEvent) const;
	bool TryResolveHeldReplayAttackSpec(
		const FGameplayTag& InputTag,
		FGameplayTag& OutAttackRequestTag,
		EActionInputEvent& OutTriggerInputEvent) const;

	// 输入事件到攻击请求的最终分发：
	// 这里把 Pressed / Held / Released 阶段统一翻译成一次真正的攻击请求，并决定是否交给 ASC 激活 Ability。
	// 它属于攻击子链分发，不替代 HeroCombatComponent 的高层输入总入口。
	FGameplayTag PrepareAttackRequestTagForEvent(
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent,
		FGameplayTag ResolvedAttackRequestTag = FGameplayTag());
	bool ProcessAttackInput(
		UActionAbilitySystemComponent* InActionASC,
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent,
		bool bCanBuffer,
		FGameplayTag ResolvedAttackRequestTag = FGameplayTag());
	bool TryHandleAttackInputByEvent(
		UActionAbilitySystemComponent* InActionASC,
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent,
		FGameplayTag ResolvedAttackRequestTag = FGameplayTag());
	/** 普通切武真实换装完成且 WeaponSwitch 已退场后，正式提交一次默认轻攻击首段请求。它继续走现有攻击主链，不单独伪造第二套攻击状态。 */
	bool TryTriggerDefaultLightAttackAfterNormalWeaponSwitch(UActionAbilitySystemComponent* InActionASC);
	bool HandleAttackInput(
		UActionAbilitySystemComponent* InActionASC,
		FGameplayTag InputTag,
		EActionInputEvent InputEvent,
		FGameplayTag ResolvedAttackRequestTag = FGameplayTag());

	// 下一帧延迟恢复：
	// 负责把攻击结束后的输入恢复与缓冲消费延迟到下一帧，避开本帧状态收尾时序冲突。
	void RequestRecoverCombatInputAfterAttack();
	void RequestConsumeBufferedAttackInputOnNextTick();
	void HandleDeferredCombatInputRecoveryAfterAttack();
	void HandleDeferredBufferedAttackInputConsume();
	/** 清掉攻击链登记过的下一帧延迟请求，避免旧请求串到新一轮攻击。 */
	void ClearDeferredAttackRequests();
	/** Hero 启动链重置时回零本组件局部攻击运行态。 */
	void ResetRuntimeStateForHeroStartup();

protected:
	// 当前依赖访问助手：
	// 攻击组件统一通过这些入口访问角色、战斗总控、武器定义、武器实例与属性集。
	// 这些 getter 都只是稳定宿主解析入口，不是新的状态源。
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;
	UHeroLoadoutContextComponent* GetOwningHeroLoadoutContextComponent() const;
	UHeroLoadoutEffectComponent* GetOwningHeroLoadoutEffectComponent() const;
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	UDataAsset_WeaponDefinition* GetCurrentWeaponDefinition() const;
	AHeroWeaponBase* GetCurrentEquippedWeapon() const;
	UActionAttributeSetBase* GetOwningActionAttributeSet() const;
	/** 读取当前正式应结算的攻击力结果。它组合的是正式宿主数据，不复制第二套数值状态。 */
	float GetResolvedAttackPowerForCurrentLoadout() const;
	bool ResolveCurrentAttackSourceHitConfig(FActionWeaponHitConfig& OutHitConfig) const;
	bool ResolveCurrentAttackProjectileConfig(
		FActionProjectileConfig& OutProjectileConfig,
		EActionResolvedProjectileConfigSource* OutResolvedSource = nullptr,
		FGameplayTag* OutSelectedProjectileConfigTag = nullptr) const;
	FTransform ResolveCurrentAttackProjectileSpawnTransform(const FActionProjectileSpawnConfig& InProjectileSpawnConfig) const;
	void FillCurrentAttackHitSourceInfo(
		FActionHitSourceInfo& InOutHitSourceInfo,
		FName InPreferredHitSourceId = NAME_None,
		FName InPreferredSourceComponentName = NAME_None) const;

	// 内部状态判定与共用小工具：
	// 负责判断当前是否处于攻击链相关状态、延迟请求是否合法以及闪反升级条件。
	bool IsAttackChainRelevantState() const;
	bool IsDeferredAttackRequestStillValid(const FGameplayTag& AttackRequestTag) const;
	void CancelActiveAttackAbilityIfNeeded(UActionAbilitySystemComponent* InActionASC) const;
	bool ShouldPromoteAttackInputToDodgeCounter(const FGameplayTag& InputTag) const;

private:
	// 当前攻击段命中运行时：
	// 这组数据只在一次攻击执行周期内有效，供命中检测、伤害结算与受击解析器载荷拼装读取。
	// 它们是正式攻击执行期快照，不是 WeaponDefinition 默认模板。
	UPROPERTY(Transient)
	bool bHasCurrentAttackHitConfig = false;

	/** 当前攻击段真正生效的命中配置；可能来自武器默认值，也可能来自分支覆写。 */
	UPROPERTY(Transient)
	FActionWeaponHitConfig CurrentAttackHitConfig;

	/** 当前攻击段是否启用了发射物生成。它只表示本次执行期是否有解析结果，不表示发射物一定已经生成。 */
	UPROPERTY(Transient)
	bool bHasCurrentAttackProjectileSpawnConfig = false;

	/** 当前攻击段真正生效的发射物生成配置。 */
	UPROPERTY(Transient)
	FActionProjectileSpawnConfig CurrentAttackProjectileSpawnConfig;

	// 连段与闪反运行时标记：
	// 负责记录连段索引是否应保留，以及闪避反击的本地授权是否已挂起。
	// 它们都只是当前或下一次攻击生效的局部运行时标记。
	UPROPERTY(Transient)
	bool bPreserveComboIndexOnNextAttackFinalize = false;

	UPROPERTY(Transient)
	bool bPendingDodgeCounterExecutionAuthorization = false;

	// 下一帧延迟任务标记：
	// 防止同一帧重复登记输入恢复或缓冲消费请求。
	// 它们只服务攻击收尾时序，不构成新的输入状态源。
	/** 下一帧是否需要在攻击收尾后恢复战斗输入。 */
	bool bDeferredCombatInputRecoveryAfterAttackRequested = false;
	/** 下一帧是否需要再次尝试消费一条缓冲攻击输入。 */
	bool bDeferredBufferedAttackInputConsumeRequested = false;
};
