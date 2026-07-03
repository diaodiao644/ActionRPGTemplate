// 文件说明：声明基础发射物类型与命中逻辑入口。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionCollisionTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionProjectileTypes.h"
#include "GameFramework/Actor.h"
#include "ActionProjectileBase.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UActionCollisionRuntimeComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnProjectilePresentationEvent,
	FActionProjectilePresentationEvent,
	PresentationEvent);

/**
 * 基础发射物。
 * 当前版本只负责：
 * 1. 承接一次已经构建好的命中载荷模板；
 * 2. 飞行过程中检测重叠；
 * 3. 命中时补齐方向与来源后送入受击解析器。
 */
UCLASS()
class ACTIONRPG_API AActionProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AActionProjectileBase();

public:
	/** 用一份已经构建好的命中载荷模板初始化当前发射物。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Projectile")
	void InitializeProjectile(
		const FActionDamagePayload& InDamagePayloadTemplate,
		const FActionProjectileInitializationContext& InInitializationContext);

	/** 用配置层参数覆盖当前发射物的飞行与销毁行为。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Projectile")
	void ApplyProjectileConfig(const FActionProjectileConfig& InProjectileConfig);

	/** 发射物表现总线。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|Projectile")
	FOnProjectilePresentationEvent OnProjectilePresentationEvent;

	/** 蓝图子类可在这里统一挂接生成、命中、销毁等表现。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action|Projectile")
	void K2_OnProjectilePresentationEvent(FActionProjectilePresentationEvent PresentationEvent);

	/** 只读返回当前发射物是否已完成初始化。 */
	UFUNCTION(BlueprintPure, Category = "Action|Projectile")
	bool IsProjectileInitialized() const
	{
		return bProjectileInitialized;
	}

	/** 只读返回这枚发射物当前累计成功命中过多少个不同目标。 */
	UFUNCTION(BlueprintPure, Category = "Action|Projectile")
	int32 GetSuccessfulResolvedTargetCount() const
	{
		return SuccessfulResolvedTargetCount;
	}

	/** 只读返回当前发射物已经确定的销毁原因。 */
	UFUNCTION(BlueprintPure, Category = "Action|Projectile")
	EActionProjectileDestroyReason GetProjectileDestroyReason() const
	{
		return DestroyReason;
	}

	/** 只读返回最近一次表现事件快照。 */
	UFUNCTION(BlueprintPure, Category = "Action|Projectile")
	FActionProjectilePresentationEvent GetLastPresentationEvent() const
	{
		return LastPresentationEvent;
	}

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void LifeSpanExpired() override;
	virtual void Destroyed() override;

	UFUNCTION()
	void OnProjectileCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	/** 根据当前命中目标，从初始化模板生成真正送入解析器的运行时载荷。 */
	bool BuildImpactDamagePayload(AActor* OtherActor, FActionDamagePayload& OutDamagePayload) const;

	/** 命中成功后给攻击方返还能量。 */
	void RewardInstigatorSpecialWeaponSwitchEnergy(
		const FActionDamagePayload& DamagePayload,
		const FActionHitResolveResult& ResolveResult) const;

	/** 命中成功后按当前策略决定是否继续飞行或销毁。 */
	void HandleSuccessfulResolvedTargetHit(
		AActor* OtherActor,
		const FActionHitResolveResult& ResolveResult,
		const FVector& ImpactLocation,
		const FVector& ImpactNormal);

	/** 判断当前是否已经达到成功命中目标上限。 */
	bool HasReachedSuccessfulHitLimit() const;

	/** 根据当前快照组装一份统一的表现事件。 */
	FActionProjectilePresentationEvent BuildPresentationEvent(
		EActionProjectilePresentationEventType InEventType,
		AActor* InTargetActor = nullptr,
		const FVector& InImpactLocation = FVector::ZeroVector,
		const FVector& InImpactNormal = FVector::ZeroVector,
		EActionHitResultType InHitResultType = EActionHitResultType::None,
		bool bInWillDestroyAfterEvent = false,
		EActionProjectileDestroyReason InDestroyReason = EActionProjectileDestroyReason::None) const;

	/** 广播统一表现事件，并更新最后一次事件快照。 */
	void BroadcastPresentationEvent(const FActionProjectilePresentationEvent& InPresentationEvent);

	/** 统一收口发射物销毁，避免多个分支直接裸 Destroy。 */
	void DestroyProjectileWithReason(
		EActionProjectileDestroyReason InDestroyReason,
		const FActionProjectilePresentationEvent* InBasePresentationEvent = nullptr);
	void EnsureProjectileCollisionSlotRegistered();
	void ApplyProjectileCollisionRuntimePreset();

	/** 按事件类型读取当前生命周期对应的表现配置。 */
	const FActionProjectileLifecyclePresentationConfig& ResolveLifecyclePresentationConfig(
		EActionProjectilePresentationEventType InEventType) const;

	/** 清理这枚发射物本次飞行中已经命中过的目标。 */
	void ClearHitActors();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USphereComponent* CollisionComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UProjectileMovementComponent* ProjectileMovementComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UActionCollisionRuntimeComponent* ActionCollisionRuntimeComponent = nullptr;

	/** 当前发射物是否已经完成初始化。 */
	UPROPERTY(Transient)
	bool bProjectileInitialized = false;

	/** 当前发射物命中有效目标后的生命周期策略快照。 */
	UPROPERTY(Transient)
	EActionProjectileSuccessfulHitPolicy SuccessfulHitPolicy =
		EActionProjectileSuccessfulHitPolicy::DestroyImmediately;

	/** 当前发射物撞到世界阻挡后的生命周期策略快照。 */
	UPROPERTY(Transient)
	EActionProjectileWorldBlockPolicy WorldBlockPolicy =
		EActionProjectileWorldBlockPolicy::IgnoreWorld;

	/** 当前发射物最多允许成功命中多少个不同目标。小于等于 0 视为不限制。 */
	UPROPERTY(Transient)
	int32 MaxSuccessfulTargetHitCount = 0;

	/** 初始化时写入的命中载荷模板。 */
	UPROPERTY(Transient)
	FActionDamagePayload DamagePayloadTemplate;

	/** 生成当帧下沉进来的只读初始化上下文。 */
	UPROPERTY(Transient)
	FActionProjectileInitializationContext InitializationContext;

	/** 最近一次广播给表现层的正式事件快照。 */
	UPROPERTY(Transient)
	FActionProjectilePresentationEvent LastPresentationEvent;

	/** 当前发射物生成成功时的生命周期表现配置。 */
	UPROPERTY(Transient)
	FActionProjectileLifecyclePresentationConfig SpawnPresentationConfig;

	/** 当前发射物撞到世界阻挡时的生命周期表现配置。 */
	UPROPERTY(Transient)
	FActionProjectileLifecyclePresentationConfig WorldBlockedPresentationConfig;

	/** 当前发射物销毁时的生命周期表现配置。 */
	UPROPERTY(Transient)
	FActionProjectileLifecyclePresentationConfig DestroyedPresentationConfig;

	/** 当前这枚发射物已经成功结算过多少个不同目标。 */
	UPROPERTY(Transient)
	int32 SuccessfulResolvedTargetCount = 0;

	/** 当前已经确定的销毁原因。 */
	UPROPERTY(Transient)
	EActionProjectileDestroyReason DestroyReason = EActionProjectileDestroyReason::None;

	/** Destroyed 事件只允许广播一次。 */
	UPROPERTY(Transient)
	bool bHasBroadcastDestroyedEvent = false;

	/** 一枚发射物对同一个目标只命中一次，避免持续重叠导致多次结算。 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	FActionCollisionOverrideHandle ProjectileCollisionOverrideHandle;
};
