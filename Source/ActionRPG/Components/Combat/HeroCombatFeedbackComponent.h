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
 * 当前只负责：
 * 1. 接收已组装好的命中反馈与发射物生命周期反馈；
 * 2. 按默认规则播放世界特效与音效；
 * 3. 对外广播全量反馈总线和 HUD 默认消费总线。
 * 它不自持复杂正式运行态；当前只读源是收到的 `FActionCombatFeedbackEvent`，
 * 以及由发射物生命周期事件桥接出来的同类反馈快照。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroCombatFeedbackComponent();

public:
	/** 对外广播全部战斗反馈事件。这里是全量总线，不额外替表现层做筛选。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|CombatFeedback", meta = (ToolTip = "对外广播全部战斗反馈事件。它只是全量事件总线，不保存新的命中、发射物或 HUD 正式状态。"))
	FOnCombatFeedbackEvent OnCombatFeedbackEvent;

	/** 对外广播 HUD 默认应消费的反馈事件。它是默认过滤后的展示总线，不是第二套正式反馈状态。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|CombatFeedback", meta = (ToolTip = "对外广播 HUD 默认应消费的反馈事件。它只是默认展示总线，不是新的长期正式状态源。"))
	FOnCombatFeedbackEvent OnCombatFeedbackHUDDisplayEvent;

	/** 接收一条已经组装完成的通用反馈事件。这里消费的是外部已定稿的只读快照，不重新推导命中或发射物状态。 */
	UFUNCTION(BlueprintCallable, Category = "Action|CombatFeedback", meta = (ToolTip = "接收一条外部已组装完成的通用反馈事件。它只消费定稿事件快照，不重新推导命中、伤害或发射物状态。"))
	void HandleCombatFeedbackEvent(const FActionCombatFeedbackEvent& InCombatFeedbackEvent);

	/**
	 * 把发射物生命周期事件桥接成通用反馈事件。
	 * 它只负责“发射物表现事件 -> 统一反馈事件”的转换与转发，
	 * 不回头修改发射物生命周期、命中结果或 HUD 状态。
	 */
	void HandleProjectilePresentationEvent(
		AActionProjectileBase* InProjectileActor,
		const FActionProjectilePresentationEvent& InPresentationEvent,
		const FActionProjectileLifecyclePresentationConfig& InLifecyclePresentationConfig);

protected:
	/** 默认规则下，哪些反馈事件需要继续转给 HUD。它只做默认展示过滤，不创建第二套 HUD 反馈状态。 */
	bool ShouldForwardEventToHUD(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 默认规则下，是否应该在世界里播放 Niagara。它只判断表现消费策略，不承担业务资格判断。 */
	bool ShouldPlayWorldEffect(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 默认规则下，是否应该在世界里播放音效。它也只服务表现消费策略。 */
	bool ShouldPlayWorldSound(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 统一播放世界特效。它只消费事件快照里的表现指令，不反向保存表现运行态。 */
	void TryPlayWorldEffect(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 统一播放世界音效。世界表现播放结果不形成新的长期运行态。 */
	void TryPlayWorldSound(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;

	/** 解析当前反馈事件最合适的世界位置。它只做表现落点解析，不反向决定命中语义。 */
	FVector ResolveFeedbackWorldLocation(const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const;
};
