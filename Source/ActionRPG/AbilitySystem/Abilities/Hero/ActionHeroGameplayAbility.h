// 文件说明：声明英雄专用 GameplayAbility 基类，统一封装角色/控制器/战斗组件访问、
// 蒙太奇播放分发、临时战斗修正效果、输入缓冲衔接与受击调试辅助。

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/ActionGameplayAbilityBase.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionHeroGameplayAbility.generated.h"

class AActionHeroCharacter;
class AActionPlayerController;
class UAbilityTask_PlayMontageAndWait;
class UHeroAttackComponent;
class UHeroCombatComponent;
class UHeroCombatInputComponent;
class UHeroLoadoutContextComponent;
class UHeroDefenseComponent;
class UHeroLoadoutRuntimeComponent;
class UHeroWeaponSwitchComponent;

/**
 * 英雄战斗 GA 的共享桥接层。
 * 它负责把通用 Ability 关系系统接到英雄角色、战斗组件、输入组件和临时战斗修正效果，
 * 让子类把重心放在自己的业务流程上，而不是重复解析同一套英雄运行时依赖。
 */
UCLASS()
class ACTIONRPG_API UActionHeroGameplayAbility : public UActionGameplayAbilityBase
{
	GENERATED_BODY()

public:
	/** 英雄战斗 GA 基础构造。默认启用关系系统和受击规则系统。 */
	UActionHeroGameplayAbility();

	/** 解析这条英雄战斗 Ability 在受击 / 取消窗口规则里对应的正式输入标签。 */
	virtual FGameplayTag GetPrimaryInputTagForCombatReact() const override;

	/** 从当前 ActorInfo 解析并缓存英雄角色。这里只做对象桥接，不形成新的正式状态源。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	AActionHeroCharacter* GetHeroCharacterFromActorInfo();

	/** 从英雄角色读取并缓存当前玩家控制器。避免同一段 Ability 链内反复 Cast/查找。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	AActionPlayerController* GetHeroControllerFromActorInfo();

	/** 从英雄角色读取并缓存战斗组件。后续输入缓冲、战斗事件广播都会走这里。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	UHeroCombatComponent* GetHeroCombatComponentFromActorInfo();

	/** 从英雄角色读取并缓存输入运行态组件。CombatInput 正式状态与缓冲恢复链都应优先直连这里。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	UHeroCombatInputComponent* GetHeroCombatInputComponentFromActorInfo();

	/** 从英雄角色读取并缓存装备域 runtime 组件。Ability 层若只需要已加载定义或实例就绪态，应优先直连这里。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	UHeroLoadoutRuntimeComponent* GetHeroLoadoutRuntimeComponentFromActorInfo();

	/** 从英雄角色读取并缓存装备域 context 组件。Ability 层若只需要属性缓存或发射物标签，应优先直连这里。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	UHeroLoadoutContextComponent* GetHeroLoadoutContextComponentFromActorInfo();

	/** 从英雄角色的战斗组件继续读取攻击组件。攻击 GA 应优先直接依赖攻击组件，而不是通过总控再转发一层。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	UHeroAttackComponent* GetHeroAttackComponentFromActorInfo();

	/** 从英雄角色的战斗组件继续读取防御组件。闪避/防御 GA 应优先直接依赖防御组件。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	UHeroDefenseComponent* GetHeroDefenseComponentFromActorInfo();

	/** 从英雄角色读取切武组件。切武 GA 应优先直接依赖切武组件，而不是继续通过战斗总控转发。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	UHeroWeaponSwitchComponent* GetHeroWeaponSwitchComponentFromActorInfo();

	/** 汇总当前 Ability 和受击组件的中文调试信息。它是联调解释链，不是规则本身。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability")
	FString DescribeCurrentCombatReactDebug() const;

	/** 把当前 Ability 的受击调试信息打印到屏幕和日志。用于运行时快速定位规则命中结果。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability")
	void PrintCurrentCombatReactDebug() const;

protected:
	/**
	 * 统一补一层英雄战斗 Ability 的激活前置判断。
	 * 这里主要解决“这个 GA 是否真的跑在英雄角色身上，以及当前受击语义是否允许开”这两类共性问题。
	 */
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/**
	 * 统一清理英雄战斗 GA 在运行期间附加的临时战斗修正效果。
	 * 这样子类只负责声明“何时挂效果”，不需要各自再维护一套结束时回收逻辑。
	 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/**
	 * 统一创建并绑定蒙太奇任务，减少各个 Ability 里的重复样板代码。
	 * 子类只需要关心自己的上下文名与回调分支，不需要各自再绑一套 Completed / Interrupted 委托。
	 */
	bool PlayHeroMontage(UAnimMontage* InMontage, const FName TaskName, const FName MontageContext);

	/**
	 * 停掉当前这条英雄 Ability 自己创建的共享蒙太奇任务，并按需中止当前运行中的战斗蒙太奇。
	 * 该入口只服务能力内部切段或受控换段，不扩成 public 动画控制接口。
	 */
	bool StopActiveHeroMontageTaskAndCurrentMontage(bool bClearRunningMontageReference);

	/** 通过战斗组件向外广播一个战斗事件标签。 */
	void BroadcastHeroCombatEvent(FGameplayTag EventTag);

	/** 请求战斗组件立刻尝试消费输入缓冲。最终是否真的能放，仍由正式输入组件裁决。 */
	bool ConsumeHeroBufferedInput();

	/** 请求战斗组件在下一帧再次尝试消费输入缓冲。适合恢复尾帧或演出延迟释放输入的场景。 */
	void RequestHeroBufferedInputConsumeNextTick();

	/**
	 * 给当前英雄 Ability 挂上一份临时战斗修正效果，并在 Ability 结束时自动移除。
	 * 适合霸体、无敌、易伤、处决强化等只应在本次 Ability 生命周期内存在的效果。
	 */
	bool ApplyHeroCombatModifierEffect(const FActionCombatModifierEffectSpec& EffectSpec);

	/** 蒙太奇完成回调，子类按上下文实现逻辑。 */
	virtual void OnHeroMontageCompleted(FName MontageContext);
	virtual void OnHeroMontageBlendOut(FName MontageContext);
	virtual void OnHeroMontageInterrupted(FName MontageContext);
	virtual void OnHeroMontageCancelled(FName MontageContext);

	/**
	 * 统一校验英雄运行时对象是否齐全，并返回便于日志输出的失败原因。
	 * 这样攻击、闪避、处决等子类在开头做前置检查时，可以直接复用同一套对象完整性判断。
	 */
	bool ValidateHeroRuntimeObjects(FString& OutFailureReason, bool bRequireController, bool bRequireCombatComponent);

private:
	/** 清掉当前 Ability 生命周期内附加的全部临时战斗修正效果。 */
	void ClearHeroCombatModifierEffects();

private:
	/** 共享蒙太奇任务的 Completed 回调入口。 */
	UFUNCTION()
	void HandleSharedMontageCompleted();

	/** 共享蒙太奇任务的 BlendOut 回调入口。 */
	UFUNCTION()
	void HandleSharedMontageBlendOut();

	/** 共享蒙太奇任务的 Interrupted 回调入口。 */
	UFUNCTION()
	void HandleSharedMontageInterrupted();

	/** 共享蒙太奇任务的 Cancelled 回调入口。 */
	UFUNCTION()
	void HandleSharedMontageCancelled();

private:
	/** 英雄战斗组件缓存，避免同一段 Ability 生命周期里反复 FindComponent / Cast。 */
	TWeakObjectPtr<UHeroCombatComponent> CachedHeroCombatComponent;

	/** 英雄输入运行态组件缓存。 */
	TWeakObjectPtr<UHeroCombatInputComponent> CachedHeroCombatInputComponent;

	/** 英雄装备域 runtime 组件缓存。 */
	TWeakObjectPtr<UHeroLoadoutRuntimeComponent> CachedHeroLoadoutRuntimeComponent;

	/** 英雄装备域 context 组件缓存。 */
	TWeakObjectPtr<UHeroLoadoutContextComponent> CachedHeroLoadoutContextComponent;

	/** 英雄角色缓存。 */
	TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;

	/** 玩家控制器缓存。 */
	TWeakObjectPtr<AActionPlayerController> CachedHeroController;

	/** 当前激活中的共享蒙太奇任务。 */
	TWeakObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	/** 当前 Ability 生命周期内施加过的临时战斗修正效果句柄集合。 */
	TArray<FActiveGameplayEffectHandle> ActiveHeroCombatModifierEffectHandles;

	/** 当前共享蒙太奇任务对应的上下文名。 */
	FName ActiveMontageContext = NAME_None;
};
