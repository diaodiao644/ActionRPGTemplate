#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "HeroExecutionCoordinatorComponent.generated.h"

struct FActionDamagePayload;
struct FActionExecutionConfig;
struct FActionHitResolveResult;
struct FGameplayTag;

class AActionHeroCharacter;
class UActionAbilitySystemComponent;
class UExecutionWindowComponent;
class UAnimMontage;
class UHeroCombatComponent;
class UHeroCombatInputComponent;
class UHeroLoadoutContextComponent;
class UHeroTargetingComponent;

/**
 * 英雄侧处决协调组件。
 * 负责维护处决目标查询、目标预占、处决伤害结算，以及处决结束后的输入恢复。
 * 这一层只处理“执行者侧”的处决流程，不负责目标窗口本身的开关、锁定和过期管理。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroExecutionCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UHeroCombatComponent;

public:
	UHeroExecutionCoordinatorComponent();

public:
	// 对外处决入口

	/** 查询指定目标当前是否满足执行者侧的处决资格，不写目标窗口状态。 */
	bool CanExecuteTarget(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	/** 为激活处决 Ability 查询本次最合适的目标，并做执行者侧最终资格确认。 */
	bool CanActivateExecutionAbility(AActor*& OutTargetActor, FString* OutFailureReason = nullptr) const;
	/** GAS 已激活后再按最新状态重验一次目标，避免正式起手时吃到旧目标缓存。 */
	bool RevalidateExecutionAbilityAfterActivation(
		AActor*& OutTargetActor,
		FString* OutFailureReason = nullptr) const;
	/** 搜索当前最合适的可处决目标，但不推进任何窗口预占或提交状态。 */
	bool TryFindExecutionTarget(AActor*& OutTargetActor, FString* OutFailureReason = nullptr) const;
	/** 把目标窗口正式预占给当前执行者，同时让目标进入处决受害者锁保护。 */
	bool TryReserveExecutionTarget(AActor* InTargetActor) const;
	/** 在命中真正成立后，把当前执行者对目标窗口的预占正式升级成“已消费待收尾”。 */
	bool TryCommitExecutionTarget(AActor* InTargetActor) const;
	/** 放弃当前执行者对目标窗口的预占，并把剩余窗口寿命交还给目标组件。 */
	void CancelReservedExecutionTarget(AActor* InTargetActor) const;
	/** 主动释放当前执行者持有的目标受害者锁，不直接改写其它执行者的目标状态。 */
	void ReleaseExecutionTargetVictimLock(AActor* InTargetActor) const;
	/** 当前双边处决演出已开始但未成功命中时，通知目标组件走 abort 收尾。 */
	void AbortConsumedExecutionTargetPresentation(AActor* InTargetActor) const;
	/** 命中帧正式结算一次已预占目标的处决伤害，并在成功后推进目标窗口提交。 */
	bool TryExecuteReservedExecutionTarget(AActor* InTargetActor, FActionHitResolveResult& OutResolveResult) const;
	/** 读取当前武器的执行者侧处决蒙太奇。 */
	UAnimMontage* GetExecutionMontageForCurrentWeapon() const;
	/** 读取当前武器要求目标侧转向执行者的准备时长。 */
	float GetExecutionVictimTurnDurationForCurrentWeapon() const;
	/** 读取当前武器要求双边处决开播前保持的水平距离。 */
	float GetExecutionStartDistanceForCurrentWeapon() const;
	/** 查询当前预占目标是否已满足启动双边处决演出的最小条件。 */
	bool CanStartReservedExecutionPresentation(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	/** 正式启动目标侧处决前准备链，例如目标转向执行者。 */
	bool TryBeginExecutionTargetPreparation(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	/** 查询目标侧处决前准备是否已经完成。 */
	bool IsExecutionTargetPreparationReady(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	/** 在准备完成后，正式请求目标组件启动目标侧被处决演出。 */
	bool TryStartExecutionTargetPresentation(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	/** 输出当前目标侧准备链状态摘要，供执行者侧轮询、调试和日志复用。 */
	bool DescribeExecutionTargetPreparationState(
		AActor* InTargetActor,
		FString& OutDescription,
		FString* OutFailureReason = nullptr) const;

	// 输入路由与恢复
	bool HandleExecutionLogic(UActionAbilitySystemComponent* InActionASC, FGameplayTag InputTag);
	bool TryCommitExecutionAbilityInput(UActionAbilitySystemComponent* InActionASC);
	void RequestRecoverCombatInputAfterExecution();
	void HandleDeferredCombatInputRecoveryAfterExecution();
	void ResetRuntimeStateForHeroStartup();

protected:
	// 内部辅助

	UHeroCombatComponent* GetOwningHeroCombatComponent() const;
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;
	UHeroLoadoutContextComponent* GetOwningHeroLoadoutContextComponent() const;
	UHeroTargetingComponent* GetOwningHeroTargetingComponent() const;
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	UExecutionWindowComponent* GetExecutionWindowComponentFromTargetActor(AActor* InTargetActor) const;
	bool IsExecutionAbilityBlockedByCombatState(FString* OutFailureReason = nullptr) const;
	bool IsTargetWithinExecutionActivationRange(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	bool HasValidExecutionConfigForTarget(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	bool HasValidReservedExecutionOwnership(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	const FActionExecutionConfig* GetCurrentWeaponExecutionConfig() const;
	FGameplayTag GetCurrentWeaponSubtypeTag() const;
	bool ResolveExecutionSearchForward(
		AActor* InTargetActor,
		FVector& OutSearchForward,
		FString* OutFailureReason = nullptr,
		bool bPreferReservedSearchForward = true) const;
	void CacheReservedExecutionSearchForward(AActor* InTargetActor, const FVector& InSearchForward) const;
	void ClearReservedExecutionSearchForwardRuntime() const;
	AActor* FindBestExecutionTarget(FString* OutFailureReason = nullptr) const;
	bool BuildExecutionDamagePayload(
		AActor* InTargetActor,
		FActionDamagePayload& OutDamagePayload,
		FString* OutFailureReason = nullptr) const;
	float GetResolvedAttackPowerForCurrentLoadout() const;
	void CancelExecutionTargetActiveCombatAbilities(AActor* InTargetActor) const;
	FGameplayTagContainer BuildExecutionTargetCancelableAbilityTags() const;

private:
	/** 处决搜索距离。 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution")
	float ExecutionSearchDistance = 240.f;

	/** 处决搜索半径。 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution")
	float ExecutionSearchRadius = 90.f;

	/** 处决搜索时相对角色的高度偏移。 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution")
	float ExecutionSearchHeightOffset = 50.f;

	/** 处决搜索允许的最大目标夹角。 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution")
	float ExecutionMaxTargetAngleDegrees = 70.f;

	// 运行时状态
	bool bDeferredCombatInputRecoveryAfterExecutionRequested = false;
	mutable bool bHasReservedExecutionSearchForward = false;
	mutable FVector ReservedExecutionSearchForward = FVector::ZeroVector;
	mutable TWeakObjectPtr<AActor> ReservedExecutionSearchForwardTarget;
};
