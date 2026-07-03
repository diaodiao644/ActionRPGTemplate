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
	bool TryResolveAttackExecutionConfigByRequestTag(
		const FGameplayTag& AttackRequestTag,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const;
	bool TryResolveAttackExecutionConfig(
		const FGameplayTag& InBranchTag,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const;

	/** 将攻击分支标签映射为蒙太奇上下文名，供动画播放与中断判断统一复用。 */
	static FName GetMontageContextNameForAttackBranch(const FGameplayTag& InBranchTag);

	// 攻击请求语义解析：
	// 负责把输入还原成正式攻击请求，并判断当前输入是否应升级成冲刺攻击。
	FGameplayTag ResolveScopedAttackAbilityInputTag(const FGameplayTag& AttackRequestTag) const;
	bool ShouldTreatAttackInputAsSprintAttack() const;
	FGameplayTag ResolveAttackRequestTag(FGameplayTag InputTag) const;
	bool TryResolveAttackBranchTag(FGameplayTag InputTag, FGameplayTag& OutBranchTag) const;
	bool TryResolveCurrentAttackExecutionConfig(
		FGameplayTag InputTag,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const;

	// 攻击执行开始同步：
	// 当一次攻击真正准备开播时，通过这里把本段解析结果、权限状态与局部运行时缓存写回组件。
	void ApplyResolvedAttackExecutionState(const FActionResolvedAttackExecutionConfig& InResolvedConfig);
	bool IsAttackExecutionStillAuthorized(
		const FGameplayTag& AttackRequestTag,
		bool bHasLocalDodgeCounterAuthorization) const;
	void ApplyAttackExecutionStartedForRequest(const FGameplayTag& AttackRequestTag);

	// 攻击命中配置与伤害载荷：
	// 负责保存当前攻击段命中参数，并在命中时组装即时伤害载荷。
	void SetCurrentAttackHitConfig(const FActionWeaponHitConfig& InHitConfig);
	void ClearCurrentAttackHitConfig();
	bool TryGetCurrentAttackHitConfigSnapshot(FActionWeaponHitConfig& OutHitConfig) const;
	void SetCurrentAttackProjectileSpawnConfig(const FActionProjectileSpawnConfig& InProjectileSpawnConfig);
	void ClearCurrentAttackProjectileSpawnConfig();
	bool TryGetCurrentAttackProjectileDebugInfo(FActionProjectileResolutionDebugInfo& OutDebugInfo) const;
	/** 基于当前攻击段运行时缓存，拼出一次正式命中结算要用的完整伤害载荷。 */
	bool TryBuildCurrentAttackDamagePayload(AActor* InTargetActor, FActionDamagePayload& OutDamagePayload) const;
	bool TryBuildCurrentAttackDamagePayloadForHitSource(
		AActor* InTargetActor,
		FName InPreferredHitSourceId,
		FName InPreferredSourceComponentName,
		FActionDamagePayload& OutDamagePayload) const;
	bool TrySpawnCurrentAttackProjectile();
	void RestoreComboIndexAfterFailedAttackStart();
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
	void ClearDeferredAttackRequests();
	void ResetRuntimeStateForHeroStartup();

protected:
	// 当前依赖访问助手：
	// 攻击组件统一通过这些入口访问角色、战斗总控、武器定义、武器实例与属性集。
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;
	UHeroLoadoutContextComponent* GetOwningHeroLoadoutContextComponent() const;
	UHeroLoadoutEffectComponent* GetOwningHeroLoadoutEffectComponent() const;
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	UDataAsset_WeaponDefinition* GetCurrentWeaponDefinition() const;
	AHeroWeaponBase* GetCurrentEquippedWeapon() const;
	UActionAttributeSetBase* GetOwningActionAttributeSet() const;
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
	UPROPERTY(Transient)
	bool bHasCurrentAttackHitConfig = false;

	/** 当前攻击段真正生效的命中配置；可能来自武器默认值，也可能来自分支覆写。 */
	UPROPERTY(Transient)
	FActionWeaponHitConfig CurrentAttackHitConfig;

	/** 当前攻击段是否启用了发射物生成。 */
	UPROPERTY(Transient)
	bool bHasCurrentAttackProjectileSpawnConfig = false;

	/** 当前攻击段真正生效的发射物生成配置。 */
	UPROPERTY(Transient)
	FActionProjectileSpawnConfig CurrentAttackProjectileSpawnConfig;

	// 连段与闪反运行时标记：
	// 负责记录连段索引是否应保留，以及闪避反击的本地授权是否已挂起。
	UPROPERTY(Transient)
	bool bPreserveComboIndexOnNextAttackFinalize = false;

	UPROPERTY(Transient)
	bool bPendingDodgeCounterExecutionAuthorization = false;

	// 下一帧延迟任务标记：
	// 防止同一帧重复登记输入恢复或缓冲消费请求。
	bool bDeferredCombatInputRecoveryAfterAttackRequested = false;
	bool bDeferredBufferedAttackInputConsumeRequested = false;
};
