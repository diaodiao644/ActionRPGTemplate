#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_AbilityChainWindow.generated.h"

class UHeroCombatComponent;

/**
 * 通用攻击衔接窗口通知。
 * 它只负责在有效帧把“当前可以接下一段”的窗口桥接给正式宿主，
 * 并把白名单、攻击输入开关和缓冲输入消费请求交给正式战斗链处理，不在这里自持正式窗口状态。
 */
UCLASS(meta = (DisplayName = "Ability Chain Window"))
class ACTIONRPG_API UAnimNotifyState_AbilityChainWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_AbilityChainWindow();

	/** 返回编辑器内显示名，方便资产作者直接看出这是衔接窗口桥接通知。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在 Begin 帧桥接当前衔接窗口，并把攻击输入开关和缓冲输入消费请求交给正式宿主。 */
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	/** 在 End 帧关闭当前通知桥接的衔接窗口，并按配置回收攻击输入开关。 */
	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 从当前动画所属角色上解析 HeroCombatComponent。它只是通知消费宿主解析入口，不形成新的衔接状态源。 */
	UHeroCombatComponent* ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const;

protected:
	/** 当前衔接窗口允许响应的输入白名单。留空表示不限制输入种类。它只控制“允许哪些输入接下一段”，不负责取消到其它主动 GA。 */
	UPROPERTY(EditAnywhere, Category = "AbilityChainWindow", meta = (ToolTip = "当前衔接窗口允许响应的输入白名单。它只是本通知的局部配置：留空表示不限制输入种类；常用于限制只能接同一条 Attack 或某个 SpiritSkillX。"))
	FGameplayTagContainer AllowedInputTags;

	/** 窗口开启时是否恢复攻击输入。它只放开这段窗口中的攻击输入总开关，不定义下一段解析规则。 */
	UPROPERTY(EditAnywhere, Category = "AbilityChainWindow", meta = (ToolTip = "窗口开启时是否恢复攻击输入。它只是本通知的局部开窗策略，常用于允许轻击或 Spirit 在该帧段继续接下一段。"))
	bool bEnableAttackInputOnBegin = true;

	/** 窗口结束时是否重新关闭攻击输入，避免离开衔接帧段后仍然能继续接段。它只回收这一小段窗口的输入放开效果。 */
	UPROPERTY(EditAnywhere, Category = "AbilityChainWindow", meta = (ToolTip = "窗口结束时是否重新关闭攻击输入，避免离开衔接帧段后仍然能继续接段。它只回收这一小段窗口的输入放开效果。"))
	bool bDisableAttackInputOnEnd = true;

	/** 窗口开启瞬间是否立刻尝试消费一条已缓存输入。它只请求正式输入宿主消费，不保证一定成功接段。 */
	UPROPERTY(EditAnywhere, Category = "AbilityChainWindow", meta = (ToolTip = "窗口开启瞬间是否立刻尝试消费一条已缓存输入。它只是消费请求，不保证一定成功接段；适合把窗口前稍早按下的接段输入直接吃掉。"))
	bool bConsumeBufferedInputOnBegin = true;
};
