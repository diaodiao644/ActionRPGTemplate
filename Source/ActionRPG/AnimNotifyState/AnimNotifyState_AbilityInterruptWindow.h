#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionAbilityTypes.h"
#include "AnimNotifyState_AbilityInterruptWindow.generated.h"

class UHeroCombatComponent;
class UHeroCombatInputComponent;
class UAnimMontage;

/**
 * AbilityInterruptWindow 的通知级 runtime 缓存。
 * 它只保存本次通知真正打开过的 interrupt-window owner 信息，
 * 让 Begin/End 能按 owner 成对收尾，而不是再次回退成全局裸开裸关。
 */
USTRUCT()
struct FAbilityInterruptWindowRuntimeEntry
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGameplayAbilitySpecHandle OwnerSpecHandle;

	UPROPERTY()
	TObjectPtr<UAnimMontage> OwnerMontage = nullptr;

	UPROPERTY()
	uint32 WindowSerial = 0;

	UPROPERTY()
	bool bOpenedWindow = false;
};

/**
 * 通用动作例外抢断窗口通知。
 * 它只负责在有效帧把“当前活跃主动 GA 是否显式开放一段例外抢断窗口”桥接给正式宿主，
 * 并把“允许哪些能力类别例外抢入”交给 HeroCombatComponent runtime 记录。
 * 它不是默认优先级关系源，也不按输入标签定义长期抢断关系。
 *
 * 它只负责在有效帧把“当前动作可被哪些输入抢占”的窗口桥接给正式宿主，
 * 并按需请求恢复攻击输入和消费缓冲输入，不在这里自持正式窗口或正式输入状态。
 */
UCLASS(meta = (DisplayName = "Ability Interrupt Window"))
class ACTIONRPG_API UAnimNotifyState_AbilityInterruptWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_AbilityInterruptWindow();

	/** 返回编辑器内显示名，方便资产作者直接看出这是例外抢断窗口桥接通知。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在 Begin 帧桥接当前例外抢断窗口，并把白名单、攻击输入开关和缓冲输入消费请求交给正式宿主。 */
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	/** 在 End 帧关闭当前通知桥接的例外抢断窗口，并按配置收回攻击输入开关。 */
	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 从当前动画所属角色上解析 HeroCombatComponent。它只是通知消费宿主解析入口，不形成新的窗口状态源。 */
	UHeroCombatComponent* ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const;

	/** 从当前动画所属角色上解析输入运行态组件，用于消费缓冲输入。它只是消费方解析 helper，不单独持有输入状态。 */
	UHeroCombatInputComponent* ResolveHeroCombatInputComponent(USkeletalMeshComponent* MeshComp) const;

	/** 解析当前通知真正要交给主动 GA 例外抢断窗口的能力类别白名单。这里始终按能力类别放行，不把输入标签当成长效关系源。 */
	TArray<EActionAbilityCategory> ResolveAllowedInterruptCategories() const;

protected:
	/** 当前例外抢断窗口允许抢入的主动 GA 能力类别白名单。留空表示本窗口不允许任何主动 GA 例外抢入。 */
	UPROPERTY(EditAnywhere, Category = "AbilityInterruptWindow", meta = (ToolTip = "当前主动 GA 例外抢断窗口允许哪些能力类别抢入。它只控制这段时间的例外抢断类别，不负责默认优先级，也不负责攻击连段衔接；留空表示本窗口不允许任何主动 GA 例外抢入。"))
	TArray<EActionAbilityCategory> AllowedInterruptCategories;

	/** 窗口开启时是否顺带恢复攻击输入。它只控制攻击输入总开关，不自动放宽其它窗口白名单。 */
	UPROPERTY(EditAnywhere, Category = "AbilityInterruptWindow", meta = (ToolTip = "窗口开启时是否顺带恢复攻击输入。它只是本通知的局部策略，常用于闪避反击、格挡反击等需要攻击抢占的阶段。"))
	bool bEnableAttackInputOnBegin = false;

	/** 窗口结束时是否重新关闭攻击输入。通常与 bEnableAttackInputOnBegin 成对使用。它只回收这一小段窗口的攻击输入放开效果。 */
	UPROPERTY(EditAnywhere, Category = "AbilityInterruptWindow", meta = (ToolTip = "窗口结束时是否重新关闭攻击输入。通常与 bEnableAttackInputOnBegin 成对使用；如果攻击输入本来就由外层持续开启，不应在这里只凭习惯强关。"))
	bool bDisableAttackInputOnEnd = false;

	/** 窗口开启瞬间是否立即尝试消费一条已缓存输入。它只请求正式输入宿主消费，不保证一定成功落地。 */
	UPROPERTY(EditAnywhere, Category = "AbilityInterruptWindow", meta = (ToolTip = "窗口开启瞬间是否立即尝试消费一条已缓存输入。它只是消费请求，不保证一定成功落地；适合让玩家提前按下的闪避/防御/攻击在开窗第一时间落地。"))
	bool bConsumeBufferedInputOnBegin = true;

private:
	/** 按 Mesh 缓存本次通知真正打开过的 interrupt-window owner 信息，避免多角色复用同一通知资产时互相覆盖。 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FAbilityInterruptWindowRuntimeEntry> CachedRuntimeByMesh;
};
