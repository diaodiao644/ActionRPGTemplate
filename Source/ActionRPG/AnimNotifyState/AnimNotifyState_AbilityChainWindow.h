#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_AbilityChainWindow.generated.h"

class UHeroCombatComponent;

/**
 * 攻击衔接窗口通知。
 * 用途：
 * 1. 让攻击蒙太奇在指定帧段内开放“接下一段攻击”的时机；
 * 2. 把允许衔接的输入白名单直接挂在动画上，而不是继续在代码里硬写具体帧段；
 * 3. 可选地在窗口开始时恢复攻击输入，并立刻尝试消费已经缓存的攻击输入。
 */
UCLASS(meta = (DisplayName = "Ability Chain Window"))
class ACTIONRPG_API UAnimNotifyState_AbilityChainWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_AbilityChainWindow();

	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 从当前动画所属角色上解析 HeroCombatComponent。 */
	UHeroCombatComponent* ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const;

protected:
	/** 当前衔接窗口允许响应的输入白名单。留空表示不限制输入种类。 */
	UPROPERTY(EditAnywhere, Category = "AbilityChainWindow")
	FGameplayTagContainer AllowedInputTags;

	/** 窗口开启时是否恢复攻击输入。常用于允许轻击接下一段攻击的帧段。 */
	UPROPERTY(EditAnywhere, Category = "AbilityChainWindow")
	bool bEnableAttackInputOnBegin = true;

	/** 窗口结束时是否再次关闭攻击输入，避免离开衔接帧段后还能继续接段。 */
	UPROPERTY(EditAnywhere, Category = "AbilityChainWindow")
	bool bDisableAttackInputOnEnd = true;

	/** 窗口开启瞬间是否立刻尝试消费一条已经缓存的输入。 */
	UPROPERTY(EditAnywhere, Category = "AbilityChainWindow")
	bool bConsumeBufferedInputOnBegin = true;
};
