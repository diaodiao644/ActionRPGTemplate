#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionProjectileTypes.h"
#include "ActionCombatFeedbackTypes.generated.h"

class AActor;
class UNiagaraSystem;
class USoundBase;

/** 高层战斗反馈事件分类，只回答这次反馈属于哪类正式事件，不表达 HUD、世界表现或命中链的长期运行态。 */
UENUM(BlueprintType)
enum class EActionCombatFeedbackEventType : uint8
{
	/** 命中解析器已经完成有效命中结算，并产出一份可供表现层消费的结果快照。 */
	HitResolved,
	/** 命中解析器明确忽略了这次命中，例如目标无效、重复命中或被规则拦截。 */
	HitIgnored,
	/** 单枚发射物已经生成，反馈事件只描述生成表现，不表示命中已经发生。 */
	ProjectileSpawned,
	/** 单枚发射物撞到世界阻挡并产出表现事件。 */
	ProjectileWorldBlocked,
	/** 单枚发射物生命周期结束并产出销毁表现事件。 */
	ProjectileDestroyed
};
/**
 * 通用战斗反馈事件的只读反馈事件快照。
 * 它只服务表现、HUD、蓝图、日志与调试消费，
 * 不反向作为新的正式状态源，也不自己推进命中或发射物生命周期。
 * 它是“一次反馈广播”的只读事件快照，不是 HUD 或反馈组件的持久状态容器。
 */
USTRUCT(BlueprintType)
struct FActionCombatFeedbackEvent
{
	GENERATED_BODY()

public:
	/** 这次反馈最终落到哪一类高层事件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	EActionCombatFeedbackEventType EventType = EActionCombatFeedbackEventType::HitIgnored;

	/** 命中主链最终返回的结果快照类型。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	EActionHitResultType HitResultType = EActionHitResultType::None;

	/** 这次反馈事件里的正式施加者快照。它通常来自命中载荷或发射物初始化上下文，不回头查询当前角色状态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<AActor> InstigatorActor = nullptr;

	/** 这次反馈事件里的直接来源 Actor 快照。它只描述事件来源，不反向改写武器或发射物状态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<AActor> SourceActor = nullptr;

	/** 这次反馈事件最终作用到的目标快照。发射物生成或纯销毁事件里可以为空。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** 命中结果或发射物生命周期事件里的最终落点位置。纯生成事件通常保持默认值。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	FVector ImpactLocation = FVector::ZeroVector;

	/** 命中结果或世界阻挡事件里的最终法线方向。纯生成 / 普通销毁事件通常保持默认值。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	FVector ImpactNormal = FVector::ZeroVector;

	/** 这次反馈事件最终落地时的基础伤害快照。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	float BaseDamage = 0.f;

	/** 这次反馈事件最终落地时的格挡体力消耗快照。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	float GuardStaminaCost = 0.f;

	/** 这次反馈事件最终落地时的削韧伤害快照。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	float PoiseDamage = 0.f;

	/** 命中结果快照里的正式伤害类型。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	EActionDamageType DamageType = EActionDamageType::Physical;

	/** 命中结果快照里的正式元素标签。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	FGameplayTag DamageElementTypeTag;

	/** 命中结果快照里的正式命中语义标签。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	FGameplayTag HitTag;

	/** 命中结果快照里的正式命中来源信息。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	FActionHitSourceInfo HitSource;

	/** 发射物事件快照里的正式发射物标签。它描述事件当时的弹体语义，不回头读取当前武器。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Projectile")
	FGameplayTag ProjectileTag;

	/** 发射物事件快照里这次配置最终来自哪条解析路径。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Projectile")
	EActionResolvedProjectileConfigSource ResolvedConfigSource =
		EActionResolvedProjectileConfigSource::None;

	/** 发射物事件快照里当前选中的配置标签结果。只在可切换发射物链路里通常有效。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Projectile")
	FGameplayTag SelectedProjectileConfigTag;

	/** 发射物事件快照里本次生成或反馈使用的 Socket 名。它来自生成上下文快照。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Projectile")
	FName SpawnSocketName = NAME_None;

	/** 发射物事件快照里这次销毁或阻挡反馈的正式原因。非发射物销毁事件通常保持 None。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Projectile")
	EActionProjectileDestroyReason DestroyReason = EActionProjectileDestroyReason::None;

	/** 表现桥接字段里的伤害数字样式结果。它只是展示指令，不是 HUD 状态源，也不反向改写命中结果。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Presentation")
	FGameplayTag DamageNumberStyleTag;

	/** 表现桥接字段里最终反馈给外层的特效资源指令。是否播放仍由消费方策略判断。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Presentation")
	TSoftObjectPtr<UNiagaraSystem> Effect;

	/** 表现桥接字段里最终反馈给外层的音效资源指令。是否播放仍由消费方策略判断。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Presentation")
	TSoftObjectPtr<USoundBase> Sound;

	/** 是否建议外层展示伤害数字的最终展示指令。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Presentation")
	bool bShouldShowDamageNumber = false;

	/** 是否建议外层播放命中特效的最终展示指令。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Presentation")
	bool bShouldPlayImpactEffect = false;

	/** 是否建议外层播放命中音效的最终展示指令。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback|Presentation")
	bool bShouldPlayImpactSound = false;
};
