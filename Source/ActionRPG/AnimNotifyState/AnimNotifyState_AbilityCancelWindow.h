#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_AbilityCancelWindow.generated.h"

class UHeroCombatComponent;
class UHeroCombatInputComponent;

/**
 * 通用动作取消窗口通知。
 * 用途：
 * 1. 让攻击 / 闪避 / 防御等蒙太奇按帧精确开放“可取消到下一个 GA”的时机；
 * 2. 把允许取消的输入白名单直接挂在动画上，避免继续在 C++ 里写死具体帧段；
 * 3. 需要允许攻击抢占时，也可以在这里顺带打开攻击输入总开关。
 */
UCLASS(meta = (DisplayName = "Ability Cancel Window"))
class ACTIONRPG_API UAnimNotifyState_AbilityCancelWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_AbilityCancelWindow();

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
	UHeroCombatInputComponent* ResolveHeroCombatInputComponent(USkeletalMeshComponent* MeshComp) const;

protected:
	/** 当前取消窗口允许响应的输入白名单。留空表示不限制输入种类。 */
	UPROPERTY(EditAnywhere, Category = "AbilityCancelWindow")
	FGameplayTagContainer AllowedInputTags;

	/** 窗口开启时是否顺带恢复攻击输入。用于闪避反击、格挡反击等需要攻击抢占的阶段。 */
	UPROPERTY(EditAnywhere, Category = "AbilityCancelWindow")
	bool bEnableAttackInputOnBegin = false;

	/** 窗口结束时是否重新关闭攻击输入。通常与 bEnableAttackInputOnBegin 成对使用。 */
	UPROPERTY(EditAnywhere, Category = "AbilityCancelWindow")
	bool bDisableAttackInputOnEnd = false;

	/** 窗口开启瞬间是否立即尝试消费一条已缓存输入。 */
	UPROPERTY(EditAnywhere, Category = "AbilityCancelWindow")
	bool bConsumeBufferedInputOnBegin = true;
};
