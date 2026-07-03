// 文件说明：声明英雄侧通用战斗反馈组件与对外广播接口。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionCombatFeedbackTypes.h"
#include "Components/ActorComponent.h"
#include "HeroCombatFeedbackComponent.generated.h"

class AActionProjectileBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnCombatFeedbackEvent,
	FActionCombatFeedbackEvent,
	CombatFeedbackEvent);

/**
 * 英雄侧通用战斗反馈组件。
 * 当前第一版只负责：
 * 1. 接收命中反馈与发射物生命周期反馈；
 * 2. 统一播放世界特效与音效；
 * 3. 对外广播可供 HUD / 蓝图消费的反馈事件。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroCombatFeedbackComponent();

public:
	/** 对外广播全部战斗反馈事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|CombatFeedback")
	FOnCombatFeedbackEvent OnCombatFeedbackEvent;

	/** 对外广播 HUD 默认应消费的反馈事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|CombatFeedback")
	FOnCombatFeedbackEvent OnCombatFeedbackHUDDisplayEvent;

	/** 接收一条已经组装完成的通用反馈事件。 */
	UFUNCTION(BlueprintCallable, Category = "Action|CombatFeedback")
	void HandleCombatFeedbackEvent(const FActionCombatFeedbackEvent& InCombatFeedbackEvent);

	/** 把发射物生命周期事件桥接成通用反馈事件。 */
	void HandleProjectilePresentationEvent(
		AActionProjectileBase* InProjectileActor,
		const FActionProjectilePresentationEvent& InPresentationEvent,
		const FActionProjectileLifecyclePresentationConfig& InLifecyclePresentationConfig);

protected:
	/** 默认规则下，哪些反馈事件需要继续转给 HUD。 */
	bool ShouldForwardEventToHUD(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 默认规则下，是否应该在世界里播放 Niagara。 */
	bool ShouldPlayWorldEffect(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 默认规则下，是否应该在世界里播放音效。 */
	bool ShouldPlayWorldSound(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 统一播放世界特效。 */
	void TryPlayWorldEffect(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 统一播放世界音效。 */
	void TryPlayWorldSound(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 解析当前反馈事件最合适的世界位置。 */
	FVector ResolveFeedbackWorldLocation(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;
};
