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
 * 它维护的是“单枚发射物这一次飞行”的局部运行态，不是武器或攻击条目的正式状态源。
 * 这里的字段都只回答“这一枚弹体现在飞到哪一阶段”，不回头代表武器长期配置或当前攻击段正式状态。
 */
UCLASS()
class ACTIONRPG_API AActionProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AActionProjectileBase();

public:
	/** 用一份已经构建好的命中载荷模板初始化当前发射物。它只吃外部已解析好的输入，不反查攻击条目或武器资产。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Projectile")
	void InitializeProjectile(
		const FActionDamagePayload& InDamagePayloadTemplate,
		const FActionProjectileInitializationContext& InInitializationContext);

	/** 用配置层参数覆盖当前发射物的飞行与销毁行为。它写入的是单枚发射物这次飞行的局部策略快照。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Projectile")
	void ApplyProjectileConfig(const FActionProjectileConfig& InProjectileConfig);

	/** 发射物表现总线。它只广播表现事件，不反向充当命中状态源。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|Projectile", meta = (ToolTip = "发射物表现总线。它只广播这一枚弹体当前生命周期里的表现事件，不反向充当正式命中状态源。"))
	FOnProjectilePresentationEvent OnProjectilePresentationEvent;

	/** 蓝图子类可在这里统一挂接生成、命中、销毁等表现。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action|Projectile", meta = (ToolTip = "蓝图表现桥入口。它只消费当前表现事件快照，不推进命中解析、销毁资格或武器正式状态。"))
	void K2_OnProjectilePresentationEvent(FActionProjectilePresentationEvent PresentationEvent);

	/** 只读返回当前发射物是否已完成初始化。它只回答单枚实例是否拿到了生成时快照，不表示后续命中已经发生。 */
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

	/** 只读返回最近一次表现事件快照。它只是表现层最近一次广播缓存，不是新的正式命中状态。 */
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

	/** 发射物命中重叠开始回调。它只处理这枚实例当前飞行的碰撞输入，不重新解析攻击条目。 */
	UFUNCTION()
	void OnProjectileCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** 发射物命中世界阻挡后的统一回调。它只消费当前实例局部策略快照。 */
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	/** 根据当前命中目标，从初始化模板生成真正送入解析器的运行时载荷。它只补齐这次目标和方向，不回头重建第二份资产状态。 */
	bool BuildImpactDamagePayload(AActor* OtherActor, FActionDamagePayload& OutDamagePayload) const;

	/** 命中成功后给攻击方返还能量。它属于命中后的辅助收尾链。 */
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

	/** 根据当前快照组装一份统一的表现事件。结果只服务表现层消费，不替代单枚发射物自身运行态。 */
	FActionProjectilePresentationEvent BuildPresentationEvent(
		EActionProjectilePresentationEventType InEventType,
		AActor* InTargetActor = nullptr,
		const FVector& InImpactLocation = FVector::ZeroVector,
		const FVector& InImpactNormal = FVector::ZeroVector,
		EActionHitResultType InHitResultType = EActionHitResultType::None,
		bool bInWillDestroyAfterEvent = false,
		EActionProjectileDestroyReason InDestroyReason = EActionProjectileDestroyReason::None) const;

	/** 广播统一表现事件，并更新最后一次事件快照。`LastPresentationEvent` 只是最近一次广播缓存，不是新的正式命中状态。 */
	void BroadcastPresentationEvent(const FActionProjectilePresentationEvent& InPresentationEvent);

	/** 统一收口发射物销毁，避免多个分支直接裸 Destroy。 */
	void DestroyProjectileWithReason(
		EActionProjectileDestroyReason InDestroyReason,
		const FActionProjectilePresentationEvent* InBasePresentationEvent = nullptr);
	/** 确保发射物碰撞组件已经向 CollisionRuntime 注册正式槽位。 */
	void EnsureProjectileCollisionSlotRegistered();
	/** 把发射物切到当前飞行阶段需要的碰撞运行态。 */
	void ApplyProjectileCollisionRuntimePreset();

	/** 按事件类型读取当前生命周期对应的表现配置。 */
	const FActionProjectileLifecyclePresentationConfig& ResolveLifecyclePresentationConfig(
		EActionProjectilePresentationEventType InEventType) const;

	/** 清理这枚发射物本次飞行中已经命中过的目标。 */
	void ClearHitActors();

protected:
	/** 发射物主碰撞体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "发射物主碰撞体。它只服务这枚实例飞行期间的局部重叠检测。"))
	USphereComponent* CollisionComponent = nullptr;

	/** 发射物运动组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "发射物运动组件。它只服务当前这枚弹体的飞行表现，不代表武器长期配置。"))
	UProjectileMovementComponent* ProjectileMovementComponent = nullptr;

	/** 发射物本地碰撞运行态宿主。它只服务单枚实例飞行期间的碰撞桥接，不替代角色侧正式命中窗口状态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "发射物本地碰撞运行态宿主。它只服务单枚实例飞行期间的碰撞桥接，不替代角色侧正式命中窗口状态。"))
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

	/** 初始化时写入的命中载荷模板。它是单枚发射物这次飞行的局部输入，不是攻击条目资产快照。 */
	UPROPERTY(Transient, meta = (ToolTip = "初始化时写入的命中载荷模板。它只属于这枚发射物本次飞行，不是武器长期默认值或新的正式命中状态源。"))
	FActionDamagePayload DamagePayloadTemplate;

	/** 生成当帧下沉进来的只读初始化上下文。它只描述这次生成，不是攻击方长期状态。 */
	UPROPERTY(Transient, meta = (ToolTip = "生成当帧写入的一次性初始化上下文。它只描述这次发射物生成，不是攻击方长期状态。"))
	FActionProjectileInitializationContext InitializationContext;

	/** 最近一次广播给表现层的正式事件快照。它只是最近一次表现广播缓存。 */
	UPROPERTY(Transient, meta = (ToolTip = "最近一次广播给表现层的事件快照。它只是表现广播缓存，不是新的正式命中状态。"))
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

	/** 当前这枚发射物已经成功结算过多少个不同目标。它只属于单枚实例生命周期。 */
	UPROPERTY(Transient)
	int32 SuccessfulResolvedTargetCount = 0;

	/** 当前已经确定的销毁原因。它只表达当前实例生命周期收尾结果。 */
	UPROPERTY(Transient)
	EActionProjectileDestroyReason DestroyReason = EActionProjectileDestroyReason::None;

	/** Destroyed 事件只允许广播一次。它只是本次实例收尾保护标记。 */
	UPROPERTY(Transient)
	bool bHasBroadcastDestroyedEvent = false;

	/** 一枚发射物对同一个目标只命中一次，避免持续重叠导致多次结算。它是单枚发射物的局部去重表。 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** 当前飞行期间持有的碰撞 override 句柄。它只服务这枚实例的生命周期收尾，销毁时必须统一释放。 */
	FActionCollisionOverrideHandle ProjectileCollisionOverrideHandle;
};
