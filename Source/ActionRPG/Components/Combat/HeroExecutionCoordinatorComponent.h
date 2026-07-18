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
 * 目标侧窗口、victim lock、前段硬锁和 victim runtime 仍正式归 `ExecutionWindowComponent` 持有。
 * 它是执行者侧协调壳，不是目标侧处决窗口或 victim lock 的正式状态源。
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
	// 这一层回答“当前执行者是否能对谁发起处决，以及如何推进执行者侧流程”，
	// 不直接把目标 Actor 写成新的目标侧状态宿主。

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
	// 这里只负责执行者侧输入提交与处决后恢复，正式输入状态与回放仍回到 HeroCombatInputComponent。
	/** 处理一次 Execution 输入逻辑。它只负责执行者侧资格判断和输入提交，不直接修改目标窗口状态。 */
	bool HandleExecutionLogic(UActionAbilitySystemComponent* InActionASC, FGameplayTag InputTag);
	/** 向 GAS 提交一条正式处决能力输入。 */
	bool TryCommitExecutionAbilityInput(UActionAbilitySystemComponent* InActionASC);
	/** 请求在处决整条演出收尾后的下一帧恢复战斗输入。 */
	void RequestRecoverCombatInputAfterExecution();
	/** 执行上一帧登记的处决后输入恢复。 */
	void HandleDeferredCombatInputRecoveryAfterExecution();
	/** Hero 启动链重置时回零执行者侧处决运行态。 */
	void ResetRuntimeStateForHeroStartup();

protected:
	// 内部辅助
	// 这些 helper 只用于执行者侧查询、载荷构建和目标侧正式窗口交互，不形成新的目标窗口状态源。

	/** 解析拥有者战斗总控。处决协调组件只持有执行者侧局部运行态，公共输入门禁仍回到总控。 */
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;
	/** 解析拥有者输入运行态组件。它只提供正式输入状态与恢复协作，不替代处决资格状态源。 */
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;
	/** 解析装备域上下文组件，供处决伤害和武器上下文拼装读取。 */
	UHeroLoadoutContextComponent* GetOwningHeroLoadoutContextComponent() const;
	/** 解析目标锁定组件。处决目标搜索可优先消费它的正式锁定结果。 */
	UHeroTargetingComponent* GetOwningHeroTargetingComponent() const;
	/** 解析拥有者 Hero 角色。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	/** 从目标 Actor 上解析目标侧 ExecutionWindowComponent。它是目标窗口的正式宿主入口。 */
	UExecutionWindowComponent* GetExecutionWindowComponentFromTargetActor(AActor* InTargetActor) const;
	/** 判断当前是否被公共战斗状态阻止进入处决链。 */
	bool IsExecutionAbilityBlockedByCombatState(FString* OutFailureReason = nullptr) const;
	/** 判断目标当前是否仍处于允许激活处决的有效距离内。 */
	bool IsTargetWithinExecutionActivationRange(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	/** 判断当前武器与目标演出配置是否足以支撑这次处决。 */
	bool HasValidExecutionConfigForTarget(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	/** 判断当前执行者是否仍然合法持有这个目标的已预占处决窗口。 */
	bool HasValidReservedExecutionOwnership(AActor* InTargetActor, FString* OutFailureReason = nullptr) const;
	/** 读取当前武器的正式处决配置。留空通常表示当前武器不支持处决演出。 */
	const FActionExecutionConfig* GetCurrentWeaponExecutionConfig() const;
	/** 读取当前武器小类别标签，供目标侧演出和配置链对齐。 */
	FGameplayTag GetCurrentWeaponSubtypeTag() const;
	/** 解析本次处决搜索 / 面向校验应使用的前向向量。 */
	bool ResolveExecutionSearchForward(
		AActor* InTargetActor,
		FVector& OutSearchForward,
		FString* OutFailureReason = nullptr,
		bool bPreferReservedSearchForward = true) const;
	/** 缓存一次已预占目标对应的处决搜索朝向，供后续同链复用。 */
	void CacheReservedExecutionSearchForward(AActor* InTargetActor, const FVector& InSearchForward) const;
	/** 清空已缓存的处决搜索朝向运行态。 */
	void ClearReservedExecutionSearchForwardRuntime() const;
	/** 搜索当前最合适的处决目标。它只返回候选结果，不推进预占或提交。 */
	AActor* FindBestExecutionTarget(FString* OutFailureReason = nullptr) const;
	/** 构建一次处决命中正式要用的伤害载荷。它返回的是这次提交前的运行时载荷，不是资产快照或命中结果。 */
	bool BuildExecutionDamagePayload(
		AActor* InTargetActor,
		FActionDamagePayload& OutDamagePayload,
		FString* OutFailureReason = nullptr) const;
	/** 读取当前正式应结算的攻击力结果。它组合的是正式宿主数据，不复制第二套数值状态。 */
	float GetResolvedAttackPowerForCurrentLoadout() const;
	/** 处决进入双边演出前，统一打断目标当前仍激活的普通战斗能力。 */
	void CancelExecutionTargetActiveCombatAbilities(AActor* InTargetActor) const;
	/** 构建目标侧在处决准备 / 演出期间允许取消的能力白名单。 */
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
	// 这些字段都只服务当前执行者侧处决链的局部协调与时序，不构成目标侧长期正式状态源。
	/** 下一帧是否需要在处决收尾后恢复战斗输入。 */
	bool bDeferredCombatInputRecoveryAfterExecutionRequested = false;
	/** 当前是否缓存了一份与已预占目标对应的搜索朝向。 */
	mutable bool bHasReservedExecutionSearchForward = false;
	/** 已预占目标对应的搜索朝向缓存。 */
	mutable FVector ReservedExecutionSearchForward = FVector::ZeroVector;
	/** 当前搜索朝向缓存所对应的目标。 */
	mutable TWeakObjectPtr<AActor> ReservedExecutionSearchForwardTarget;
};
