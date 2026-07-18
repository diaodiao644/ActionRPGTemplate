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
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UHeroAttackComponent;
class UHeroCombatComponent;
class UHeroCombatInputComponent;
class UHeroLoadoutContextComponent;
class UHeroDefenseComponent;
class UHeroLoadoutRuntimeComponent;
class UHeroWeaponSwitchComponent;

/**
 * Hero 共享 GameplayAbility 能力壳。
 * 它负责把通用 Ability 规则解释基类接到 Hero 角色、核心组件、共享蒙太奇任务、
 * 输入缓冲消费桥和临时战斗修正效果句柄管理上，
 * 让具体 `HeroGA_*` 子类把重心放在自己的业务流程，而不是重复解析同一套 Hero 运行时依赖。
 * 它不是攻击、防御、切武、处决等具体子链的正式状态源。
 */
UCLASS()
class ACTIONRPG_API UActionHeroGameplayAbility : public UActionGameplayAbilityBase
{
	GENERATED_BODY()

public:
	/** 英雄战斗 GA 基础构造。默认启用关系系统和受击规则系统。 */
	UActionHeroGameplayAbility();

	/** 解析这条英雄战斗 Ability 在受击 / 取消窗口规则里对应的正式输入标签。它只服务关系裁决、恢复窗口和调试解释。 */
	virtual FGameplayTag GetPrimaryInputTagForCombatReact() const override;

	/** 从当前 ActorInfo 解析并缓存英雄角色。这里只做对象桥接，不形成新的正式状态源。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从当前 ActorInfo 解析并缓存英雄角色。它只是 Hero 能力壳的稳定宿主解析入口，不会创建新的角色运行态，也不替代任何正式状态源。"))
	AActionHeroCharacter* GetHeroCharacterFromActorInfo();

	/** 从英雄角色读取并缓存当前玩家控制器。避免同一段 Ability 链内反复 Cast/查找。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从英雄角色读取并缓存当前玩家控制器。它主要服务输入、朝向和本地玩家相关查询，不表示控制器一定拥有新的正式业务状态。"))
	AActionPlayerController* GetHeroControllerFromActorInfo();

	/** 从英雄角色读取并缓存战斗组件。后续输入缓冲、战斗事件广播都会走这里。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从英雄角色读取并缓存战斗组件。它是 Hero 能力壳读取正式战斗状态、输入缓冲和战斗事件广播的首选宿主入口。"))
	UHeroCombatComponent* GetHeroCombatComponentFromActorInfo();

	/** 从英雄角色读取并缓存输入运行态组件。CombatInput 正式状态与缓冲恢复链都应优先直连这里。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从英雄角色读取并缓存输入运行态组件。它持有输入缓冲、Held 回放和输入门禁运行态，不建议在 Ability 里平行维护第二套输入状态。"))
	UHeroCombatInputComponent* GetHeroCombatInputComponentFromActorInfo();

	/** 从英雄角色读取并缓存装备域 runtime 组件。Ability 层若只需要已加载定义或实例就绪态，应优先直连这里。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从英雄角色读取并缓存装备域 runtime 组件。它主要提供已加载定义、实例就绪态和预热结果查询，不等于当前正式已装备结果本身。"))
	UHeroLoadoutRuntimeComponent* GetHeroLoadoutRuntimeComponentFromActorInfo();

	/** 从英雄角色读取并缓存装备域 context 组件。Ability 层若只需要属性缓存或发射物标签，应优先直连这里。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从英雄角色读取并缓存装备域 context 组件。它主要提供属性缓存、武器语义镜像和发射物标签上下文查询，不替代 WeaponDefinition 顶层正式语义入口。"))
	UHeroLoadoutContextComponent* GetHeroLoadoutContextComponentFromActorInfo();

	/** 从英雄角色的战斗组件继续读取攻击组件。攻击 GA 应优先直接依赖攻击组件，而不是通过总控再转发一层。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从英雄角色继续解析攻击组件。它是攻击主链的正式宿主之一；这里读到的是宿主入口，不代表本次攻击资格已经自动成立。"))
	UHeroAttackComponent* GetHeroAttackComponentFromActorInfo();

	/** 从英雄角色的战斗组件继续读取防御组件。闪避/防御 GA 应优先直接依赖防御组件。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从英雄角色继续解析防御组件。它是闪避和防御主链的正式宿主之一；这里读到的是宿主入口，不等于当前一定允许进入防御或闪避。"))
	UHeroDefenseComponent* GetHeroDefenseComponentFromActorInfo();

	/** 从英雄角色读取切武组件。切武 GA 应优先直接依赖切武组件，而不是继续通过战斗总控转发。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "从英雄角色读取切武组件。它是固定槽切换事务和切武演出的正式宿主入口；这里不把请求壳或 UI 快照误读成真实切武状态。"))
	UHeroWeaponSwitchComponent* GetHeroWeaponSwitchComponentFromActorInfo();

	/** 汇总当前 Ability 和受击组件的中文调试信息。它是联调解释链，不是规则本身。 */
	UFUNCTION(BlueprintPure, Category = "Action|Ability", meta = (ToolTip = "汇总当前 Ability 与受击组件的中文调试信息。它只服务联调解释，不推进关系裁决，也不形成新的正式状态源。"))
	FString DescribeCurrentCombatReactDebug() const;

	/** 把当前 Ability 的受击调试信息打印到屏幕和日志。用于运行时快速定位规则命中结果。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Ability", meta = (ToolTip = "把当前 Ability 的受击调试信息打印到屏幕和日志。它只服务排错，不会修改当前 Ability、受击状态或输入状态。"))
	void PrintCurrentCombatReactDebug() const;

protected:
	/**
	 * 统一补一层 Hero 能力的激活前置判断。
	 * 这里主要解决“这个 GA 是否真的跑在 Hero 角色身上，以及共性受击语义是否允许开”这两类问题，
	 * 不替代具体子类自己的业务资格判断。
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
	 * 子类只需要关心自己的上下文名与回调分支，不需要各自再绑一套
	 * `OnCompleted / OnBlendOut / OnInterrupted / OnCancelled` 委托。
	 * 这里是共享 AbilityTask 委托绑定桥：
	 * 只负责“创建任务 + 绑定四类回调 + 激活任务”，
	 * 不承担新的正式业务状态源职责，也不直接替子类决定业务收尾。
	 */
	bool PlayHeroMontage(UAnimMontage* InMontage, const FName TaskName, const FName MontageContext);

	/**
	 * 停掉当前这条英雄 Ability 自己创建的共享蒙太奇任务，并按需中止当前运行中的战斗蒙太奇。
	 * 该入口只服务能力内部切段或受控换段，不扩成 public 动画控制接口，
	 * 也不替代正式 Combat/Weapon/Spirit 状态的收尾。
	 */
	bool StopActiveHeroMontageTaskAndCurrentMontage(bool bClearRunningMontageReference);

	/** 通过战斗组件向外广播一个战斗事件标签。它只负责事件桥接，不把事件本身或广播成功结果当成状态源。 */
	void BroadcastHeroCombatEvent(FGameplayTag EventTag);

	/** 请求正式输入链立刻尝试消费输入缓冲。这里仅发起桥接请求，最终是否真的能放仍由 `HeroCombatInputComponent / HeroCombatComponent` 裁决。 */
	bool ConsumeHeroBufferedInput();

	/** 请求正式输入链在下一帧再次尝试消费输入缓冲。适合恢复尾帧或演出延迟释放输入的场景。 */
	void RequestHeroBufferedInputConsumeNextTick();

public:
	/** 只读返回当前这次 Hero 共享蒙太奇桥缓存的蒙太奇资源。它只服务 owner 解析，不是新的动画状态源。 */
	UAnimMontage* GetCurrentHeroMontageAsset() const;

	/** 只读返回当前这条 Hero Ability 的 SpecHandle。它只服务 owner 配对和调试，不替代 ASC 正式管理。 */
	FGameplayAbilitySpecHandle GetCurrentHeroAbilitySpecHandle() const;

	/** 判断当前这条 Hero Ability 是否仍持有指定 Hero 共享蒙太奇资源。 */
	bool OwnsHeroMontage(const UAnimMontage* InMontage) const;

protected:
	/** 把 Hero 共享能力壳当前会在 cancel 时一并释放的 owner tags 追加到输出里，供 ASC 取消前预测门复用。 */
	virtual void GetPredictedOwnedTagsToReleaseOnRelationshipCancel(
		FGameplayTagContainer& OutOwnedTags) const override;

	/**
	 * 给当前英雄 Ability 挂上一份临时战斗修正效果，并在 Ability 结束时自动移除。
	 * 适合霸体、无敌、易伤、处决强化等只应在本次 Ability 生命周期内存在的效果。
	 */
	bool ApplyHeroCombatModifierEffect(const FActionCombatModifierEffectSpec& EffectSpec);

	/** 蒙太奇完成回调，子类按上下文实现逻辑。它只把共享任务结果转发给子类，不强行替子类定义业务收尾。 */
	virtual void OnHeroMontageCompleted(FName MontageContext);
	/** 蒙太奇 BlendOut 回调。 */
	virtual void OnHeroMontageBlendOut(FName MontageContext);
	/** 蒙太奇中断回调。 */
	virtual void OnHeroMontageInterrupted(FName MontageContext);
	/** 蒙太奇取消回调。 */
	virtual void OnHeroMontageCancelled(FName MontageContext);

	/**
	 * 统一校验英雄运行时对象是否齐全，并返回便于日志输出的失败原因。
	 * 这样攻击、闪避、处决等子类在开头做前置检查时，可以直接复用同一套对象完整性判断。
	 */
	bool ValidateHeroRuntimeObjects(FString& OutFailureReason, bool bRequireController, bool bRequireCombatComponent);

private:
	/** 清掉当前 Ability 生命周期内附加的全部临时战斗修正效果。它只负责共享效果句柄回收，不推进子类自己的正式收尾。 */
	void ClearHeroCombatModifierEffects();

private:
	/** 共享蒙太奇任务的 Completed 回调入口。它只消费 AT 回调并继续分发给子类，不把回调本身写成新的正式状态机。 */
	UFUNCTION()
	void HandleSharedMontageCompleted();

	/** 共享蒙太奇任务的 BlendOut 回调入口。它只服务共享回调分发，不单独定义业务收尾。 */
	UFUNCTION()
	void HandleSharedMontageBlendOut();

	/** 共享蒙太奇任务的 Interrupted 回调入口。它只表示这次共享任务收到了中断结果，不额外创造第二套蒙太奇运行态。 */
	UFUNCTION()
	void HandleSharedMontageInterrupted();

	/** 共享蒙太奇任务的 Cancelled 回调入口。它只消费任务层取消结果，再交给子类决定具体业务收尾。 */
	UFUNCTION()
	void HandleSharedMontageCancelled();

private:
	/** 英雄战斗组件缓存。它只是 Hero 共享能力壳的宿主解析缓存，不构成第二套战斗状态源。 */
	TWeakObjectPtr<UHeroCombatComponent> CachedHeroCombatComponent;

	/** 英雄输入运行态组件缓存。它只减少重复解析，不额外持有输入运行态。 */
	TWeakObjectPtr<UHeroCombatInputComponent> CachedHeroCombatInputComponent;

	/** 英雄装备域 runtime 组件缓存。它只缓存宿主入口，不把 runtime ready 结果写成 Ability 自己的状态。 */
	TWeakObjectPtr<UHeroLoadoutRuntimeComponent> CachedHeroLoadoutRuntimeComponent;

	/** 英雄装备域 context 组件缓存。它只缓存上下文读取入口，不复制属性缓存或发射物标签状态。 */
	TWeakObjectPtr<UHeroLoadoutContextComponent> CachedHeroLoadoutContextComponent;

	/** 英雄角色缓存。它只是稳定宿主解析结果，不构成第二套角色运行态。 */
	TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;

	/** 玩家控制器缓存。它只是共享查询缓存，不构成新的输入或朝向状态源。 */
	TWeakObjectPtr<AActionPlayerController> CachedHeroController;

	/** 当前激活中的共享蒙太奇任务。它只是本次 Ability 生命周期内的 AbilityTask 句柄与委托绑定宿主，不单独表达战斗正式状态。 */
	TWeakObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	/** 当前 Ability 生命周期内施加过的临时战斗修正效果句柄集合。它们只服务本次 Ability 自动回收，不构成长期战斗状态。 */
	TArray<FActiveGameplayEffectHandle> ActiveHeroCombatModifierEffectHandles;

	/** 当前 Ability 生命周期内通过共享战斗修正入口额外挂到 owner 身上的标签集合。它只服务取消前预测，不反向替代正式效果状态。 */
	FGameplayTagContainer ActiveHeroCombatModifierGrantedTags;

	/** 当前共享蒙太奇任务对应的上下文名。它主要服务 AbilityTask 回调分流和调试解释，不是新的蒙太奇业务状态源。 */
	FName ActiveMontageContext = NAME_None;

	/** 当前这次 Hero 共享蒙太奇桥缓存的蒙太奇资源。它只服务 owner 解析和旧通知安全配对。 */
	TWeakObjectPtr<UAnimMontage> ActiveHeroMontageAsset;
};
