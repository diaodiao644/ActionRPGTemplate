#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Animation/AnimEnums.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionCollisionTypes.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionType/ActionExecutionTypes.h"
#include "Components/PawnExtensionComponentBase.h"
#include "ExecutionWindowComponent.generated.h"

class UActionAttributeSetBase;
class UActionAbilitySystemComponent;
class UAnimMontage;
class ACharacter;
struct FActionExecutionRootMotionOverrideRuntime
{
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UAnimMontage> Montage;
	TEnumAsByte<ERootMotionMode::Type> PreviousRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	FName Reason = NAME_None;
	FActionCollisionOverrideHandle CapsulePawnPassThroughHandle;
	bool bActive = false;

	void Reset()
	{
		Character.Reset();
		Montage.Reset();
		PreviousRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
		Reason = NAME_None;
		CapsulePawnPassThroughHandle.Reset();
		bActive = false;
	}
};

/** 目标侧被处决演出配置。 */
USTRUCT(BlueprintType)
struct FActionExecutionVictimAnimConfig
{
	GENERATED_BODY()

public:
	/** 当前武器小类命中时，目标要播放的被处决蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Execution")
	TObjectPtr<UAnimMontage> VictimExecutionMontage = nullptr;

public:
	bool IsValidConfig() const
	{
		return ::IsValid(VictimExecutionMontage);
	}
};

/**
 * 处决窗口组件。
 * 组件职责：
 * 1. 接收来自战斗反应层的“可处决”触发信号，并打开 / 关闭处决窗口；
 * 2. 管理执行者对窗口的预占，以及目标作为处决受害者时的锁定保护；
 * 3. 给受击解析器、Ability 和 UI 提供统一的查询入口，避免各层各自维护处决状态。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UExecutionWindowComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UExecutionWindowComponent();

	/**
	 * 接收战斗反应事件，并决定是否打开或刷新处决窗口。
	 * 当前正式入口是 `Combat.Event.PoiseBreak`：
	 * 1. 目标被打出破韧后，由这里统一打开可处决窗口；
	 * 2. 若窗口已开但尚未被预占，则会顺带刷新超时计时；
	 * 3. 若运行时里残留了失效执行者引用，也会在这里顺手清理，避免窗口状态脏读。
	 */
	bool HandleCombatReactEvent(FGameplayTag EventTag, AActor* InstigatorActor);

	/**
	 * 查询处决窗口当前是否打开。
	 * 这个查询只回答“当前有没有一扇处决机会窗口仍然存在”，
	 * 不隐含“窗口归谁所有”“是否已被正式预占”或“目标是否已被锁定”这些更强语义。
	 */
	UFUNCTION(BlueprintPure, Category = "Action|Execution")
	bool IsExecutionWindowOpen() const { return ExecutionWindowRuntimeState.IsWindowOpen(); }

	/**
	 * 查询指定执行者当前是否可以对本目标发起处决。
	 * 这个查询只回答“资格是否成立”，不真正写入任何预占或锁定状态，
	 * 适合给 HeroCombatComponent、GA_Execution 或 UI 提示层做前置判定。
	 */
	UFUNCTION(BlueprintPure, Category = "Action|Execution")
	bool CanBeExecutedBy(AActor* InstigatorActor) const;

	/** C++ 诊断版处决资格查询：在返回布尔值的同时补充精确失败原因。 */
	bool CanBeExecutedByWithReason(AActor* InstigatorActor, FString* OutFailureReason = nullptr) const;

	/**
	 * 尝试预占处决窗口，并同步锁定处决受害者。
	 * 预占成功后，表示这次处决演出已经把目标“留给某个执行者”：
	 * 1. 其它执行者不能再抢同一窗口；
	 * 2. 外部伤害和效果会被受害者锁定保护拦截；
	 * 3. 窗口计时器会暂停，避免演出期间被自然超时关闭。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Execution")
	bool TryReserveExecutionWindow(AActor* InstigatorActor);

	/**
	 * 尝试正式提交当前处决窗口。
	 * 提交阶段要求“同一执行者已完成预占 + 受害者锁定”两项前置，
	 * 这样可以保证真正结算处决命中时，不会误消费别人的窗口或消费一个未锁定目标的空窗口。
	 * 当前语义是“把窗口标记为已被正式处决消费，并等待目标被处决蒙太奇结束后再正式关窗”，
	 * 不再等价于“命中当帧立刻 CloseExecutionWindow(true)”。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Execution")
	bool TryCommitExecutionWindow(AActor* InstigatorActor);

	/**
	 * 取消当前执行者对处决窗口的预占。
	 * 取消后会同时释放受害者锁定，并在窗口仍然有效时恢复超时倒计时。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Execution")
	void CancelReservedExecutionWindow(AActor* InstigatorActor);

	/**
	 * 主动打开处决窗口，并记录本次触发者。
	 * 当前主要由破韧事件调用，也保留给未来脚本化能力或测试入口直接触发。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Execution")
	void OpenExecutionWindow(AActor* InstigatorActor);

	/**
	 * 主动关闭处决窗口。
	 * bWasConsumed 用于区分两种完全不同的收尾语义：
	 * 1. `true`：窗口被正式处决消费，后续应走“处决触发”事件；
	 * 2. `false`：窗口自然过期或被取消，只做窗口关闭收尾。
	 * 无论哪种关闭来源，只要目标仍存活且允许恢复韧性，都会在收尾阶段把韧性恢复到正常值。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Execution")
	void CloseExecutionWindow(bool bWasConsumed);

	/** 破韧表现自然结束后恢复目标韧性，但不关闭当前处决窗口。窗口是否继续存在仍由处决窗口主链决定。 */
	void RestorePoiseAfterPoiseBreakPresentationEnd();

	/** 破韧表现自然结束且未进入正式处决消费链时，按未消费语义正式关闭窗口。 */
	void HandlePoiseBreakPresentationEndedWithoutExecution();

	/** 查询目标当前是否处于处决受害者锁定中。锁定成立时，外部普通伤害和效果应被视为干扰。 */
	UFUNCTION(BlueprintPure, Category = "Action|Execution")
	bool IsExecutionVictimLocked() const { return ExecutionWindowRuntimeState.HasExecutionVictimLock(); }

	/**
	 * 查询处决窗口当前是否仍由指定执行者预占。
	 * 这通常用于判断：某个执行者是不是已经把这次开放窗口正式接手成“专属处决机会”。
	 */
	bool IsExecutionWindowReservedBy(AActor* InstigatorActor) const;

	/**
	 * 查询处决受害者锁定是否来自指定执行者。
	 * 当这个判断成立时，说明目标当前正处于该执行者的处决保护演出链里。
	 */
	UFUNCTION(BlueprintPure, Category = "Action|Execution")
	bool IsExecutionVictimLockedBy(AActor* InstigatorActor) const;

	/** 查询当前处决受害者锁定的执行者。只用于诊断和归属对齐，不修改窗口状态。 */
	AActor* GetExecutionVictimLockInstigator() const;

	/**
	 * 主动释放处决受害者锁定。
	 * 这个入口通常由处决取消、处决演出收尾或异常清理流程调用，
	 * 用来把“目标暂时只属于某个执行者”的保护状态撤掉。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Execution")
	void ReleaseExecutionVictimLock(AActor* InstigatorActor);

	/** 处决已进入双边演出但未成功命中时的失败收尾：关闭窗口、释放锁并恢复 Poise。 */
	void AbortConsumedExecutionPresentation(AActor* InstigatorActor);

	/** 判断当前目标是否已经给指定武器小类配置了被处决演出。 */
	UFUNCTION(BlueprintPure, Category = "Action|Execution")
	bool HasVictimExecutionPresentationForWeaponSubtype(FGameplayTag WeaponSubtypeTag) const;

	/** 判断当前目标是否已经给指定武器小类配置了目标侧处决硬锁解除 Notify。 */
	UFUNCTION(BlueprintPure, Category = "Action|Execution")
	bool HasExecutionRecoveryUnlockNotifyForWeaponSubtype(FGameplayTag WeaponSubtypeTag) const;

	/** 判断指定执行者当前是否具备启动目标侧处决前准备链的最小数据。 */
	bool CanPrepareExecutionPresentationBy(
		AActor* InstigatorActor,
		const FGameplayTag& WeaponSubtypeTag,
		FString* OutFailureReason = nullptr) const;

	/** 正式开始目标侧“转向执行者 -> 等待完成 -> 准备开播”的准备链。 */
	bool TryBeginExecutionPresentationPreparation(
		AActor* InstigatorActor,
		const FGameplayTag& WeaponSubtypeTag,
		float TurnDurationSeconds,
		FString* OutFailureReason = nullptr);

	/** 查询当前目标侧的处决前准备是否已经完成。 */
	bool IsExecutionPresentationPreparationReady(AActor* InstigatorActor, FString* OutFailureReason = nullptr) const;

	/** 在准备完成后正式播放目标侧被处决蒙太奇。 */
	bool TryStartPreparedExecutionPresentation(AActor* InstigatorActor, FString* OutFailureReason = nullptr);

	/** 目标侧被处决蒙太奇结束回调。已消费处决链会在这里正式关窗并完成目标侧收尾。 */
	void HandleVictimExecutionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 输出目标侧处决前准备的当前运行态说明，供执行者侧轮询诊断复用。 */
	bool DescribeExecutionPresentationPreparationState(
		AActor* InstigatorActor,
		FString& OutDescription,
		FString* OutFailureReason = nullptr) const;

	/** 目标侧被处决蒙太奇内的专用 Notify 到达后，正式解除目标硬锁并恢复响应资格；不在这里直接关窗或恢复 Poise。 */
	void NotifyExecutionRecoveryUnlockFrame();

	/** 正式结束这次目标侧 victim 运行态。它只收 victim runtime，不直接裁决窗口是否消费。 */
	void FinalizeExecutionVictimRuntime(bool bWasInterrupted);

	/** 当前是否仍处于目标侧 victim 运行态。 */
	bool IsExecutionVictimRuntimeActive() const { return bExecutionVictimRuntimeActive; }

	/** 当前是否仍处于目标侧 victim 前段硬锁。 */
	bool IsExecutionVictimHardLocked() const { return bExecutionVictimRuntimeActive && bExecutionVictimHardLockActive; }

	/**
	 * 查询本次外部伤害或效果是否应被处决保护拦截。
	 * 这是受击解析器与处决系统之间的统一查询入口：
	 * 1. 若目标未被锁定，则放行；
	 * 2. 若目标已被锁定，则只允许锁定它的执行者打出的正式处决命中穿过；
	 * 3. 其它外部伤害、DOT、状态效果一律视为干扰并拦截。
	 */
	UFUNCTION(BlueprintPure, Category = "Action|Execution")
	bool ShouldBlockIncomingDamageOrEffect(AActor* InstigatorActor, bool bIsExecutionHit) const;

protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 向目标广播处决相关 GameplayEvent，供 Ability、UI 与监听系统共用同一条事件链。
	 * 这样处决打开、关闭、正式触发等阶段都能统一落到 GameplayEvent 体系里。
	 */
	void BroadcastExecutionEvent(FGameplayTag EventTag, AActor* InstigatorActor) const;

	/**
	 * 读取拥有者的属性集。
	 * 这里是处决窗口组件访问“目标是否存活、当前韧性是否需要恢复”等角色数值状态的统一入口。
	 */
	const UActionAttributeSetBase* GetOwningAttributeSet() const;

	/**
	 * 读取拥有者的 Action ASC。
	 * 逻辑层的处决窗口状态保存在组件内部，而 GAS 侧的“受害者锁定标签效果”则通过这个入口施加与移除。
	 */
	UActionAbilitySystemComponent* GetOwningActionAbilitySystemComponent() const;

	/** 读取拥有者角色。目标侧被处决转向与蒙太奇播放都统一从这里取宿主。 */
	ACharacter* GetOwningCharacter() const;

	/**
	 * 应用处决受害者锁定效果。
	 * 这个效果本质上是“处决演出期间的目标保护层”，让外部干扰有一条 GAS 侧可见的状态来源。
	 */
	void ApplyExecutionVictimLockEffect();

	/**
	 * 移除处决受害者锁定效果，并结束目标的处决保护状态。
	 * 它只负责 GAS 侧效果收尾；真正的逻辑层锁定关系由运行时状态结构自己维护。
	 */
	void RemoveExecutionVictimLockEffect();

	/**
	 * 按当前配置重启处决窗口超时计时器。
	 * 这个函数只在窗口仍然开放、且没有被正式演出接管时才有意义。
	 */
	void RestartExecutionWindowTimer();

	/**
	 * 处决窗口超时时的统一回调。
	 * 这里统一走“未消费关闭”语义，也就是会正常关闭窗口，但不会触发正式处决结算事件。
	 */
	UFUNCTION()
	void OnExecutionWindowExpired();

	/** 每帧推进目标朝向执行者的准备链，直到达到可正式开播状态。 */
	void UpdateExecutionPresentationPreparation(float DeltaTime);

	/** 判断当前准备链是否仍然归指定执行者持有并且上下文有效。 */
	bool CanContinueExecutionPresentationPreparation(AActor* InstigatorActor, FString* OutFailureReason = nullptr) const;

	/** 结束本轮目标侧处决准备运行时。 */
	void ClearExecutionPresentationPreparationRuntime();

	/** 按武器小类解析当前目标应使用的被处决演出配置。 */
	const FActionExecutionVictimAnimConfig* FindVictimExecutionAnimConfig(const FGameplayTag& WeaponSubtypeTag) const;

	/** 查询指定蒙太奇是否已带上目标侧处决硬锁解除 Notify。 */
	bool HasExecutionRecoveryUnlockNotify(const UAnimMontage* Montage) const;

	/** 目标侧处决蒙太奇播放期间临时启用 Montage Root Motion。 */
	bool BeginExecutionRootMotionOverride(UAnimMontage* Montage, FName Reason);
	void EndExecutionRootMotionOverride(UAnimMontage* Montage, FName Reason);

	/** 统一通过 GAS 属性修改链把目标韧性恢复到上限。 */
	void RestorePoiseToMaxForExecution(FName Reason, bool bOnlyIfAlive);

	/** 应用 / 移除目标处决恢复前段硬锁保护。 */
	void ApplyExecutionVictimHardLockEffect();
	void RemoveExecutionVictimHardLockEffect();

	/** 清掉本次目标侧 victim 运行态。 */
	void ClearExecutionVictimRuntime();

protected:
	/** 处决窗口持续时长。单位为秒，决定玩家从目标失衡到完成输入的可操作时间。 */
	UPROPERTY(EditDefaultsOnly, Category = "Action|Execution")
	float ExecutionWindowDuration = 4.f;

	/**
	 * 处决窗口关闭时是否自动恢复目标韧性。
	 * 当前语义是：只要窗口关闭时目标仍然存活，就允许在收尾阶段把韧性恢复到正常值。
	 * 这样无论窗口是自然关闭还是被正式处决消费，只要目标没死，后续都还能再次被正常破韧。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Action|Execution")
	bool bRestorePoiseOnClose = true;

	/**
	 * 处决窗口运行时状态。
	 * 这里保存窗口开关、最近触发者、当前预占者、受害者锁定者和超时句柄等运行时信息。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Execution")
	FActionExecutionWindowRuntimeState ExecutionWindowRuntimeState;

	/**
	 * 处决期间施加给目标的受害者锁定效果。
	 * 当前默认会授予一个“处决受害者锁定中”的状态标签，供受击保护和状态查询复用。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Execution")
	FActionCombatModifierEffectSpec ExecutionVictimLockCombatModifierEffect;

	/** 不同武器小类对应的目标侧被处决演出配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Execution")
	TMap<FGameplayTag, FActionExecutionVictimAnimConfig> VictimExecutionAnimConfigs;

	/** 处决恢复前段硬锁期间施加给目标的保护效果。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Execution")
	FActionCombatModifierEffectSpec ExecutionVictimHardLockCombatModifierEffect;

private:
	/**
	 * 当前受害者锁定效果句柄。
	 * 它是组件和 GAS 侧锁定效果之间的唯一连接点，
	 * 用来保证重复预占时不会重复叠层、重复清理时也能可靠找到并移除旧效果。
	 */
	FActiveGameplayEffectHandle ExecutionVictimLockEffectHandle;

	/** 当前是否正处于目标侧处决前准备阶段。 */
	bool bExecutionPresentationPreparationActive = false;

	/** 当前目标侧处决前准备是否已经完成。 */
	bool bExecutionPresentationPreparationReady = false;

	/** 当前这次目标侧处决前准备归属的执行者。 */
	TWeakObjectPtr<AActor> ExecutionPresentationPreparationInstigator;

	/** 当前这次目标侧处决前准备对应的武器小类。 */
	FGameplayTag ExecutionPresentationPreparationWeaponSubtypeTag;

	/** 当前这次目标侧处决前准备的已推进时长。 */
	float ExecutionPresentationPreparationElapsedSeconds = 0.f;

	/** 当前这次目标侧处决前准备要求的总转向时长。 */
	float ExecutionPresentationPreparationDurationSeconds = 0.f;

	/** 当前是否已经正式进入目标侧 victim 运行态。 */
	bool bExecutionVictimRuntimeActive = false;

	/** 当前 victim 运行态是否仍处于 Notify 开窗前的前段硬锁。 */
	bool bExecutionVictimHardLockActive = false;

	/** 当前恢复是否已经收到正式开窗 Notify。 */
	bool bExecutionRecoveryUnlockFrameReceived = false;

	/** 当前这次处决命中已正式消费窗口，但仍等待目标被处决蒙太奇结束后再真正关窗。 */
	bool bExecutionConsumedPendingVictimMontageEnd = false;

	/** 当前这次破韧表现结束后的韧性恢复是否已经正式执行过。 */
	bool bPoiseBreakPresentationRestoreApplied = false;

	/** 当前这次目标侧 victim 运行态归属的执行者。 */
	TWeakObjectPtr<AActor> ExecutionVictimRuntimeInstigator;

	/** 当前这次目标侧 victim 运行态对应的武器小类。 */
	FGameplayTag ExecutionVictimRuntimeWeaponSubtypeTag;

	/** 当前处决恢复前段硬锁保护效果句柄。 */
	FActiveGameplayEffectHandle ExecutionVictimHardLockEffectHandle;

	/** 目标侧处决 / 恢复蒙太奇 Root Motion 临时接管状态。 */
	FActionExecutionRootMotionOverrideRuntime ExecutionRootMotionOverrideRuntime;
};
