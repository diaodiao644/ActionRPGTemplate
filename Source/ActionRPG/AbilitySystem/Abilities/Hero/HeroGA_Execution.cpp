// 文件说明：实现英雄处决 Ability，把处决窗口、处决演出、无敌状态与命中帧伤害串成一条完整链路。

#include "AbilitySystem/Abilities/Hero/HeroGA_Execution.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "Debug/ActionDebugHelper.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroGA_Execution, Log, All);

UHeroGA_Execution::UHeroGA_Execution()
	: Super()
{
	// 处决 Ability 由独立输入显式触发，不复用普通攻击输入。
	ActivationPolicy = EActionAbilityActivationPolicy::OnInput;
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_Execution);
	ActivationOwnedTags.AddTag(ActionGameplayTags::State_Ability_Execution_Active);
	CombatReactAbilityRule.bAllowActivationDuringRecoveryCancelWindow = true;

	// 处决期间执行者自身的保护也统一走持续修正效果。
	// 这样执行者无敌、状态查询和后续诊断都能复用现有 GE / 状态效果入口，
	// 而不用额外开一条只服务处决的特殊状态通道。
	ExecutionInvulnerabilityCombatModifierEffect.Duration = 10.f;
	ExecutionInvulnerabilityCombatModifierEffect.StatusEffectTag =
		ActionGameplayTags::StatusEffect_Combat_ExecutionInvulnerable;
	ExecutionInvulnerabilityCombatModifierEffect.GrantedTags.AddTag(
		ActionGameplayTags::State_Combat_ExecutionInvulnerable);
}

bool UHeroGA_Execution::ValidateRelationshipActivationPreconditions(FString& OutFailureReason)
{
	// 关系预检阶段只补充“这条处决主链现在有没有资格被放行”的判断。
	// 这里不真正写预占窗口、执行者保护或目标锁，避免预检失败时留下半套处决副作用。
	// 处决的“能否接管当前其它主动 GA”仍由 ASC 关系矩阵负责，这里只确认它自己当前是否具备即时起手资格。
	if (!ValidateHeroRuntimeObjects(OutFailureReason, false, true))
	{
		return false;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCombatComponent)
	{
		// 没有战斗组件时，处决链既拿不到目标查询入口，也无法提交后续预占 / 命中 / 收尾逻辑。
		OutFailureReason = TEXT("hero combat component is invalid");
		return false;
	}

	AActor* ExecutionTargetActor = nullptr;
	FString ExecutionFailureReason;
	// 这里只验证“当前是否存在一个合法的可处决目标”，
	// 真正的预占提交仍要等 ActivateAbility 里再次按最新状态执行，避免资格检查和正式进入之间被外部流程抢先改写。
	if (!HeroCombatComponent->CanActivateExecutionAbility(ExecutionTargetActor, &ExecutionFailureReason))
	{
		OutFailureReason = ExecutionFailureReason.IsEmpty()
			? TEXT("execution target is not available")
			: ExecutionFailureReason;
		return false;
	}

	if (!HeroCombatComponent->GetExecutionMontageForCurrentWeapon())
	{
		// 处决当前要求“有目标 + 有当前武器对应的处决演出”同时成立。
		// 否则即便逻辑上可以处决，也会在运行时卡在没有表现资源的半状态里。
		OutFailureReason = TEXT("execution montage is missing");
		return false;
	}

	return true;
}

void UHeroGA_Execution::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 这批字段都只服务“本次处决实例”的局部时序。
	// 正式处决状态仍在组件侧；GA 本地只缓存目标、预占、命中和收尾是否已发生，方便统一收口。
	// 每次激活前都把运行时状态重置干净，避免上一轮处决被打断后的残留状态污染新流程。
	bExecutionWindowReserved = false;
	bExecutionHitApplied = false;
	bExecutionStarted = false;
	bExecutionPreparationPending = false;
	bAbilityFinished = false;
	ActiveExecutionMontage = nullptr;
	ExecutionPreparationStartWorldTime = -1.f;
	ExecutionPreparationPollCount = 0;
	LastExecutionPreparationStageText.Reset();
	ExecutionInvulnerabilityEffectHandle.Invalidate();
	CachedExecutionTargetActor.Reset();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExecutionPreparationPollTimerHandle);
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCombatComponent)
	{
		Debug::Print(TEXT("[GA][Execution] Begin Failed: CombatComponent 无效"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 处决已经在 GAS 里正式激活后，这里只做“运行时二次复核”：
	// 继续按最新战斗状态重解析目标，保证玩家按键瞬间使用的是最新目标；
	// 但不再重复经过输入门禁，避免 ExecutionActive 刚挂上就把自己反向挡回去。
	AActor* ExecutionTargetActor = nullptr;
	FString ExecutionFailureReason;
	if (!HeroCombatComponent->RevalidateExecutionAbilityAfterActivation(ExecutionTargetActor, &ExecutionFailureReason))
	{
		const FString ResolvedFailureReason = ExecutionFailureReason.IsEmpty()
			? TEXT("当前没有可处决目标，或目标不满足处决条件。")
			: ExecutionFailureReason;
		Debug::Print(
			FString::Printf(TEXT("[GA][Execution] Activation Revalidate Failed: %s"), *ResolvedFailureReason),
			FColor::Red,
			2.0f);
		UE_LOG(LogHeroGA_Execution, Warning, TEXT("处决激活后二次复核失败：%s"), *ResolvedFailureReason);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][Execution] Begin Target=%s"),
			*GetNameSafe(ExecutionTargetActor)),
		FColor::Cyan,
		2.0f);

	// 先走 GAS 标准提交，确保消耗、冷却与可激活条件都已正式生效。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		Debug::Print(TEXT("[GA][Execution] Begin Failed: CommitAbility 失败"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Commit 放在正式预占前，目的是先把这次处决的标准能力成本与资格固定下来，
	// 避免出现“已经抢占了目标，但能力自身提交最终失败”的半状态。

	// 预占目标的处决窗口，确保当前演出期间资格不会被其他流程抢占。
	// 这一步和真正落伤不是一回事：
	// 这里只是把“这个目标现在由本次处决流程接管”先锁定下来。
	// 一旦这一步成功，后续无论演出是否完整播完，都必须走完整的窗口回退和目标锁释放链。
	if (!HeroCombatComponent->TryReserveExecutionTarget(ExecutionTargetActor))
	{
		Debug::Print(TEXT("[GA][Execution] Begin Failed: 目标预占失败"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CachedExecutionTargetActor = ExecutionTargetActor;
	bExecutionWindowReserved = true;
	SetExecutionPerformerInputLock(true);
	UpdateExecutionPreparationStage(
		FString::Printf(TEXT("已成功预占目标：Target=%s"), *GetNameSafe(ExecutionTargetActor)),
		FColor::Cyan);
	// 到这里说明“当前这次处决已经正式接管了目标”，
	// 但还没有真正开始双边处决演出；正式开播要等目标侧转向准备完成后再进入。

	// 进入处决后立即切断普通攻击衔接能力，
	// 避免玩家在处决演出首帧又把普通攻击、闪避反击等缓冲输入带进来。
	if (UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo())
	{
		HeroDefenseComponent->ClearDodgeCounterAvailability();
	}
	HeroCombatComponent->SetAttackEnabled(false);
	HeroCombatComponent->ClearAbilityWindowsForAuthoritativeTakeover();

	// 正式进入处决流程后，执行者获得无敌，避免处决演出被外部伤害或效果打断。
	// 受害者侧的锁定与保护由战斗组件在预占/正式处决链里统一处理，这里只负责执行者自身保护。
	SetExecutionInvulnerabilityEnabled(true);
	// 从这里开始，执行者保护与受害者保护是两套来源：
	// 1. 执行者无敌由本 GA 直接挂在自己身上；
	// 2. 受害者锁定由目标侧处决组件和战斗组件统一维护。
	// 两边分开维护的好处是：后续即使要单独扩展“执行者免控”和“受害者免受外部干扰”，
	// 也不需要把两种保护硬绑成同一个实现点。

	if (!TryBeginExecutionPreparation())
	{
		FinishExecutionAbility(true);
	}
}

void UHeroGA_Execution::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		if (ActiveExecutionMontage)
		{
			HeroCombatComponent->EndMontageRootMotionOverride(
				Cast<ACharacter>(GetAvatarActorFromActorInfo()),
				ActiveExecutionMontage,
				TEXT("ExecutionEndAbility"));
		}

		const FActionRunningAnimationReactGuardContext& RunningContext =
			HeroCombatComponent->GetRunningAnimationReactGuardContext();
		if (RunningContext.Semantic == EActionRunningAnimationSemantic::Execution
			&& RunningContext.Montage == ActiveExecutionMontage)
		{
			HeroCombatComponent->ClearRunningAnimationReactGuardContext();
		}
	}

	ActiveExecutionMontage = nullptr;

	// 处决既可能正常播完，也可能被更高优先级行为或外部系统强制结束。
	// 因此这里必须做兜底收口，不能假设蒙太奇回调一定会把完整结束链都走到。
	// 处决 GA 可能被外部系统直接结束，收尾不能只依赖蒙太奇回调。
	if (!bAbilityFinished && (bExecutionWindowReserved || bExecutionStarted || bExecutionPreparationPending))
	{
		// 只有真的开始过处决流程，才需要兜底收尾。
		// 如果前置校验阶段就失败，则不应误执行一套“释放窗口 / 广播结束”的处决收尾链。
		FinalizeExecutionState(bWasCancelled);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHeroGA_Execution::OnHeroMontageCompleted(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][Execution] Montage Completed"), FColor::Yellow, 2.0f);

	// 蒙太奇自然完成时只负责收尾。
	// 如果命中帧 Notify 没有被触发，则本次处决视为未成功命中并按取消流程处理。
	// 这里不直接结算伤害；真正消费窗口的入口始终只有命中帧。
	FinishExecutionAbility(false);
}

void UHeroGA_Execution::OnHeroMontageBlendOut(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][Execution] Montage BlendOut"), FColor::Yellow, 2.0f);

	// BlendOut 与 Completed 都可能被触发，因此这里只走统一收尾。
	// 必须依赖统一防重入收口，而不能在这里再单独推导“这次是否已经完整结束”。
	FinishExecutionAbility(false);
}

void UHeroGA_Execution::OnHeroMontageInterrupted(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][Execution] Montage Interrupted"), FColor::Red, 2.0f);
	FinishExecutionAbility(true);
}

void UHeroGA_Execution::OnHeroMontageCancelled(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][Execution] Montage Cancelled"), FColor::Red, 2.0f);
	FinishExecutionAbility(true);
}

void UHeroGA_Execution::NotifyExecutionHitFrame()
{
	// 命中帧是正式消费处决窗口的唯一入口。
	// 处决伤害不放在蒙太奇结束时处理，而是交给 AnimNotify 在精确命中时机提交。
	// 只有在处决真正开始且 Ability 还没结束时，才允许命中帧落伤。
	// 这样即使 Notify 因为时序问题晚到，也不会在处决已经收尾后又补打一段处决伤害。
	if (!bExecutionStarted || bAbilityFinished)
	{
		Debug::Print(TEXT("[GA][Execution] HitFrame Ignored"), FColor::Yellow, 2.0f);
		return;
	}

	TryApplyExecutionHit();
}

bool UHeroGA_Execution::TryApplyExecutionHit()
{
	if (bExecutionHitApplied)
	{
		// 命中帧允许被重复触发，但真正的处决结算只能成功一次。
		// 因此这里把“已经结算过”视为安全跳过，而不是当成错误。
		Debug::Print(TEXT("[GA][Execution] HitFrame Skipped: 已经结算过"), FColor::Yellow, 2.0f);
		return true;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	AActor* ExecutionTargetActor = CachedExecutionTargetActor.Get();
	if (!HeroCombatComponent || !IsValid(ExecutionTargetActor))
	{
		// 到命中帧时，执行者侧战斗组件和目标引用仍然必须有效。
		// 少任一项，都说明这次处决已经失去安全结算上下文，只能判失败而不能硬落伤。
		// 这里故意不再尝试重新找目标，因为命中帧必须只命中“本次处决一开始锁定的那一个目标”。
		Debug::Print(TEXT("[GA][Execution] Hit Failed: CombatComponent/Target 无效"), FColor::Red, 2.0f);
		return false;
	}

	// 只有预占成功的目标，才允许正式提交并结算一次处决命中。
	// 真正的伤害、处决击杀、失衡窗口关闭等后续语义，都应当从这一刻开始生效。
	// 也正因为如此，处决伤害不放在蒙太奇结束时，而是放在 AnimNotify 精确命中帧。
	// 命中帧也只能结算“本次开始时锁定并预占的那个目标”，不会在这里重新找一个新目标来补打。
	FActionHitResolveResult ResolveResult;
	if (!HeroCombatComponent->TryExecuteReservedExecutionTarget(ExecutionTargetActor, ResolveResult))
	{
		// 这里仅保留总入口失败提示，具体原因由协调组件的独立日志给出。
		// 目标窗口提交失败、目标侧 abort 和 Poise 恢复都必须由目标组件主链执行，
		// 执行者侧 GA 不能在这里临时拼一套“看起来像成功结束”的假收尾。
		Debug::Print(TEXT("[GA][Execution] Hit Failed"), FColor::Red, 2.0f);
		return false;
	}

	bExecutionHitApplied = true;
	bExecutionWindowReserved = false;
	// 命中成功后，窗口从“由本次处决持有但尚未消费”转成“已经正式消费完成”。
	// 之后收尾链就不该再走预占回退，而只负责关保护、广播结束和恢复输入。
	BroadcastHeroCombatEvent(ActionGameplayTags::Player_Event_Execution_Hit);
	Debug::Print(
		FString::Printf(
			TEXT("[GA][Execution] Hit Success Target=%s"),
			*GetNameSafe(ExecutionTargetActor)),
		FColor::Green,
		2.0f);

	return true;
}

bool UHeroGA_Execution::TryBeginExecutionPreparation()
{
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	AActor* ExecutionTargetActor = CachedExecutionTargetActor.Get();
	if (!HeroCombatComponent || !IsValid(ExecutionTargetActor))
	{
		UpdateExecutionPreparationStage(TEXT("准备阶段失败并进入统一取消：CombatComponent 或目标无效。"), FColor::Red, true);
		return false;
	}

	FString BeginPreparationFailureReason;
	if (!HeroCombatComponent->TryBeginExecutionTargetPreparation(ExecutionTargetActor, &BeginPreparationFailureReason))
	{
		UpdateExecutionPreparationStage(
			FString::Printf(
				TEXT("准备阶段失败并进入统一取消：%s"),
				BeginPreparationFailureReason.IsEmpty()
					? TEXT("目标侧处决前准备启动失败。")
					: *BeginPreparationFailureReason),
			FColor::Red,
			true);
		return false;
	}

	ExecutionPreparationStartWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ExecutionPreparationPollCount = 0;
	UpdateExecutionPreparationStage(
		FString::Printf(TEXT("已成功启动目标侧准备链：Target=%s"), *GetNameSafe(ExecutionTargetActor)),
		FColor::Cyan);

	// 目标侧准备链可能在当前帧就已经完成，也可能要等若干帧转向后才能正式开播。
	// 因此这里先立即尝试一次“能不能直接进入双边演出”，
	// 不成立时再转入轮询，而不是默认一定要多等一帧。
	FString PreparationFailureReason;
	if (TryStartExecutionPresentationIfReady(&PreparationFailureReason))
	{
		return true;
	}

	FString PreparationStateDescription;
	FString PreparationStateFailureReason;
	HeroCombatComponent->DescribeExecutionTargetPreparationState(
		ExecutionTargetActor,
		PreparationStateDescription,
		&PreparationStateFailureReason);
	UpdateExecutionPreparationStage(
		FString::Printf(
			TEXT("当前等待目标准备完成：%s"),
			PreparationStateFailureReason.IsEmpty()
				? (PreparationFailureReason.IsEmpty() ? TEXT("目标侧处决前准备尚未完成。") : *PreparationFailureReason)
				: *PreparationStateFailureReason),
		FColor::Yellow);
	RequestContinueExecutionPreparationNextTick();
	return true;
}

void UHeroGA_Execution::HandleDeferredExecutionPreparation()
{
	if (!bExecutionPreparationPending)
	{
		return;
	}

	bExecutionPreparationPending = false;

	if (bAbilityFinished || bExecutionStarted || !bExecutionWindowReserved)
	{
		return;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	AActor* ExecutionTargetActor = CachedExecutionTargetActor.Get();
	if (!HeroCombatComponent || !IsValid(ExecutionTargetActor))
	{
		UpdateExecutionPreparationStage(TEXT("准备阶段失败并进入统一取消：CombatComponent 或目标无效。"), FColor::Red, true);
		FinishExecutionAbility(true);
		return;
	}

	++ExecutionPreparationPollCount;

	FString PreparationFailureReason;
	if (!HeroCombatComponent->CanStartReservedExecutionPresentation(ExecutionTargetActor, &PreparationFailureReason))
	{
		UpdateExecutionPreparationStage(
			FString::Printf(
				TEXT("准备阶段失败并进入统一取消：%s"),
				PreparationFailureReason.IsEmpty() ? TEXT("预占资格或范围已失效。") : *PreparationFailureReason),
			FColor::Red,
			true);
		FinishExecutionAbility(true);
		return;
	}

	FString TryStartFailureReason;
	if (TryStartExecutionPresentationIfReady(&TryStartFailureReason))
	{
		return;
	}

	FString PreparationStateDescription;
	FString PreparationStateFailureReason;
	HeroCombatComponent->DescribeExecutionTargetPreparationState(
		ExecutionTargetActor,
		PreparationStateDescription,
		&PreparationStateFailureReason);
	if (HasExecutionPreparationTimedOut())
	{
		UpdateExecutionPreparationStage(
			FString::Printf(
				TEXT("准备阶段失败并进入统一取消：处决前准备超时，已等待 %.2f 秒。%s"),
				GetExecutionPreparationElapsedSeconds(),
				PreparationStateDescription.IsEmpty()
					? (TryStartFailureReason.IsEmpty() ? TEXT("目标侧处决前准备仍未完成。") : *TryStartFailureReason)
					: *PreparationStateDescription),
			FColor::Red,
			true);
		FinishExecutionAbility(true);
		return;
	}

	UpdateExecutionPreparationStage(
		FString::Printf(
			TEXT("当前等待目标准备完成：%s"),
			PreparationStateFailureReason.IsEmpty()
				? (TryStartFailureReason.IsEmpty() ? TEXT("目标侧处决前准备尚未完成。") : *TryStartFailureReason)
				: *PreparationStateFailureReason),
		FColor::Yellow);
	RequestContinueExecutionPreparationNextTick();
}

bool UHeroGA_Execution::TryStartExecutionPresentationIfReady(FString* OutFailureReason)
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (bAbilityFinished || bExecutionStarted)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = bAbilityFinished
				? TEXT("处决 Ability 已进入正式收尾。")
				: TEXT("处决演出已经正式开始。");
		}
		return false;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	AActor* ExecutionTargetActor = CachedExecutionTargetActor.Get();
	if (!HeroCombatComponent || !IsValid(ExecutionTargetActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("CombatComponent 或当前处决目标无效。");
		}
		return false;
	}

	FString PreparationFailureReason;
	if (!HeroCombatComponent->CanStartReservedExecutionPresentation(ExecutionTargetActor, &PreparationFailureReason))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = PreparationFailureReason.IsEmpty()
				? TEXT("预占资格或范围已失效。")
				: PreparationFailureReason;
		}
		return false;
	}

	FString PreparationReadyFailureReason;
	if (!HeroCombatComponent->IsExecutionTargetPreparationReady(ExecutionTargetActor, &PreparationReadyFailureReason))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = PreparationReadyFailureReason.IsEmpty()
				? TEXT("目标侧处决前准备尚未完成。")
				: PreparationReadyFailureReason;
		}
		return false;
	}

	UpdateExecutionPreparationStage(
		FString::Printf(
			TEXT("已满足目标准备完成，准备进入双边演出：Target=%s Poll=%d Waited=%.2f 秒"),
			*GetNameSafe(ExecutionTargetActor),
			ExecutionPreparationPollCount,
			GetExecutionPreparationElapsedSeconds()),
		FColor::Cyan);

	FString FaceTargetFailureReason;
	if (!FaceExecutionTargetImmediately(&FaceTargetFailureReason))
	{
		const FString FailureReason = FaceTargetFailureReason.IsEmpty()
			? TEXT("执行者在处决开播前面向目标失败。")
			: FaceTargetFailureReason;
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
		UpdateExecutionPreparationStage(
			FString::Printf(TEXT("准备阶段失败并进入统一取消：%s"), *FailureReason),
			FColor::Red,
			true);
		FinishExecutionAbility(true);
		return true;
	}

	UpdateExecutionPreparationStage(
		FString::Printf(
			TEXT("目标转向完成，执行者已面向目标，准备开始双边演出：Target=%s"),
			*GetNameSafe(ExecutionTargetActor)),
		FColor::Cyan);

	// 双边都完成转向后，才允许按当前武器的静态处决配置补一次瞬时起手距离。
	// 这一步只服务开播前站位桥接：失败时不取消预占，不回滚准备链，而是直接按现有站位继续开播。
	TryApplyExecutionStartDistanceAdjustment();

	UAnimMontage* ExecutionMontage = HeroCombatComponent->GetExecutionMontageForCurrentWeapon();
	if (!ExecutionMontage)
	{
		const FString FailureReason = TEXT("当前武器处决蒙太奇为空。");
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
		UpdateExecutionPreparationStage(
			FString::Printf(TEXT("准备阶段失败并进入统一取消：%s"), *FailureReason),
			FColor::Red,
			true);
		FinishExecutionAbility(true);
		return true;
	}

	FString StartVictimPresentationFailureReason;
	if (!HeroCombatComponent->TryStartExecutionTargetPresentation(ExecutionTargetActor, &StartVictimPresentationFailureReason))
	{
		const FString FailureReason = StartVictimPresentationFailureReason.IsEmpty()
			? TEXT("目标侧被处决演出启动失败。")
			: StartVictimPresentationFailureReason;
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
		UpdateExecutionPreparationStage(
			FString::Printf(TEXT("准备阶段失败并进入统一取消：%s"), *FailureReason),
			FColor::Red,
			true);
		FinishExecutionAbility(true);
		return true;
	}

	// 目标转向完成后，才正式进入双边处决演出。
	// 从这一刻开始，执行者蒙太奇与目标侧被处决蒙太奇一起进入命中帧等待链。
	HeroCombatComponent->UpdateRunningAnimMontage(ExecutionMontage);
	HeroCombatComponent->SetRunningAnimationReactGuardContext(
		ExecutionMontage,
		EActionRunningAnimationSemantic::Execution,
		0);
	ActiveExecutionMontage = ExecutionMontage;
	HeroCombatComponent->BeginMontageRootMotionOverride(
		Cast<ACharacter>(GetAvatarActorFromActorInfo()),
		ExecutionMontage,
		TEXT("ExecutionPerformer"));
	if (!PlayHeroMontage(ExecutionMontage, TEXT("PlayExecutionMontage"), TEXT("Execution")))
	{
		const FString FailureReason = FString::Printf(
			TEXT("处决蒙太奇播放失败。Montage=%s"),
			*GetNameSafe(ExecutionMontage));
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
		UpdateExecutionPreparationStage(
			FString::Printf(TEXT("准备阶段失败并进入统一取消：%s"), *FailureReason),
			FColor::Red,
			true);
		FinishExecutionAbility(true);
		return true;
	}

	// 处决从 Idle 直接正式开播时，也要立即把角色带入 Combo。
	// 当前武器挂点继续只认 CombatMode，从而让处决武器动画使用 WeaponSocket 表现。
	HeroCombatComponent->EnterComboModeImmediatelyForActivePresentation();
	bExecutionStarted = true;
	bExecutionPreparationPending = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExecutionPreparationPollTimerHandle);
	}

	BroadcastHeroCombatEvent(ActionGameplayTags::Player_Event_Execution_Begin);
	Debug::Print(TEXT("[GA][Execution] Montage Begin"), FColor::Cyan, 2.0f);
	return true;
}

bool UHeroGA_Execution::FaceExecutionTargetImmediately(FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(GetAvatarActorFromActorInfo());
	AActor* ExecutionTargetActor = CachedExecutionTargetActor.Get();
	if (!HeroCharacter || !IsValid(ExecutionTargetActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = !HeroCharacter
				? TEXT("执行者角色无效，无法在处决开播前面向目标。")
				: TEXT("当前处决目标无效，无法在处决开播前面向目标。");
		}
		return false;
	}

	FVector ToTarget = ExecutionTargetActor->GetActorLocation() - HeroCharacter->GetActorLocation();
	ToTarget.Z = 0.f;

	float DesiredYaw = HeroCharacter->GetActorRotation().Yaw;
	if (!ToTarget.IsNearlyZero())
	{
		DesiredYaw = ToTarget.GetSafeNormal().Rotation().Yaw;
	}
	else if (const AController* HeroController = HeroCharacter->GetController())
	{
		DesiredYaw = HeroController->GetControlRotation().Yaw;
	}

	const FRotator DesiredYawRotation(0.f, DesiredYaw, 0.f);
	HeroCharacter->SetActorRotation(DesiredYawRotation);

	return true;
}

void UHeroGA_Execution::TryApplyExecutionStartDistanceAdjustment()
{
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(GetAvatarActorFromActorInfo());
	AActor* ExecutionTargetActor = CachedExecutionTargetActor.Get();
	if (!HeroCombatComponent || !HeroCharacter || !IsValid(ExecutionTargetActor))
	{
		return;
	}

	const float DesiredDistance = HeroCombatComponent->GetExecutionStartDistanceForCurrentWeapon();
	if (DesiredDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector PerformerLocation = HeroCharacter->GetActorLocation();
	const FVector TargetLocation = ExecutionTargetActor->GetActorLocation();
	FVector HorizontalOffset = TargetLocation - PerformerLocation;
	HorizontalOffset.Z = 0.f;

	const float CurrentDistance = HorizontalOffset.Size();
	if (DesiredDistance <= CurrentDistance + KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector PerformerMoveDirection = (-HorizontalOffset).GetSafeNormal();
	if (PerformerMoveDirection.IsNearlyZero())
	{
		PerformerMoveDirection = (-HeroCharacter->GetActorForwardVector()).GetSafeNormal2D();
	}

	if (PerformerMoveDirection.IsNearlyZero())
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Execution] StartDistance skipped. Target=%s Current=%.2f Desired=%.2f Reason=invalid_direction"),
				*GetNameSafe(ExecutionTargetActor),
				CurrentDistance,
				DesiredDistance),
			FColor::Yellow,
			1.5f);
		return;
	}

	const float ExtraDistance = DesiredDistance - CurrentDistance;
	const FVector DesiredPerformerLocation = PerformerLocation + (PerformerMoveDirection * ExtraDistance);
	FString PerformerMoveFailureReason;
	if (TryMoveExecutionActorToLocation(HeroCharacter, DesiredPerformerLocation, &PerformerMoveFailureReason))
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Execution] StartDistance adjusted by performer. Target=%s Current=%.2f Desired=%.2f"),
				*GetNameSafe(ExecutionTargetActor),
				CurrentDistance,
				DesiredDistance),
			FColor::Cyan,
			1.5f);
		return;
	}

	FVector VictimMoveDirection = HorizontalOffset.GetSafeNormal();
	if (VictimMoveDirection.IsNearlyZero())
	{
		VictimMoveDirection = HeroCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	if (VictimMoveDirection.IsNearlyZero())
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Execution] StartDistance skipped. Target=%s Current=%.2f Desired=%.2f PerformerFailure=%s VictimDirectionInvalid"),
				*GetNameSafe(ExecutionTargetActor),
				CurrentDistance,
				DesiredDistance,
				*PerformerMoveFailureReason),
			FColor::Yellow,
			1.5f);
		return;
	}

	const FVector DesiredTargetLocation = TargetLocation + (VictimMoveDirection * ExtraDistance);
	FString VictimMoveFailureReason;
	if (TryMoveExecutionActorToLocation(ExecutionTargetActor, DesiredTargetLocation, &VictimMoveFailureReason))
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][Execution] StartDistance adjusted by victim fallback. Target=%s Current=%.2f Desired=%.2f PerformerFailure=%s"),
				*GetNameSafe(ExecutionTargetActor),
				CurrentDistance,
				DesiredDistance,
				*PerformerMoveFailureReason),
			FColor::Cyan,
			1.5f);
		return;
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][Execution] StartDistance skipped. Target=%s Current=%.2f Desired=%.2f PerformerFailure=%s VictimFailure=%s"),
			*GetNameSafe(ExecutionTargetActor),
			CurrentDistance,
			DesiredDistance,
			*PerformerMoveFailureReason,
			*VictimMoveFailureReason),
		FColor::Yellow,
		1.5f);
}

bool UHeroGA_Execution::TryMoveExecutionActorToLocation(
	AActor* InActor,
	const FVector& InTargetLocation,
	FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!IsValid(InActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("actor_invalid");
		}
		return false;
	}

	if (InTargetLocation.ContainsNaN())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("target_location_invalid");
		}
		return false;
	}

	if (ACharacter* Character = Cast<ACharacter>(InActor))
	{
		if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}

	const FVector OriginalLocation = InActor->GetActorLocation();
	const FVector FinalLocation(InTargetLocation.X, InTargetLocation.Y, OriginalLocation.Z);
	FHitResult SweepHitResult;
	if (InActor->SetActorLocation(FinalLocation, true, &SweepHitResult, ETeleportType::TeleportPhysics))
	{
		return true;
	}

	if (OutFailureReason)
	{
		*OutFailureReason = SweepHitResult.bBlockingHit
			? FString::Printf(TEXT("blocked_by_%s"), *GetNameSafe(SweepHitResult.GetActor()))
			: TEXT("set_actor_location_failed");
	}
	return false;
}

void UHeroGA_Execution::SetExecutionPerformerInputLock(const bool bLocked)
{
	AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(GetAvatarActorFromActorInfo());
	if (!HeroCharacter)
	{
		return;
	}

	if (AController* HeroController = HeroCharacter->GetController())
	{
		HeroController->SetIgnoreMoveInput(bLocked);
		HeroController->SetIgnoreLookInput(bLocked);
	}

	if (bLocked)
	{
		if (UCharacterMovementComponent* MovementComponent = HeroCharacter->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}
}

void UHeroGA_Execution::RequestContinueExecutionPreparationNextTick()
{
	if (bExecutionPreparationPending || !GetWorld())
	{
		return;
	}

	bExecutionPreparationPending = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleDeferredExecutionPreparation);
}

void UHeroGA_Execution::SetExecutionInvulnerabilityEnabled(bool bEnabled)
{
	// 这条链只负责执行者自身的保护开关。
	// 目标侧锁定与保护由目标和战斗组件另一条链维护，不能和执行者无敌混成同一份状态源。
	UActionAbilitySystemComponent* ActionASC = GetActionAbilitySystemComponentFromActorInfo();
	if (!ActionASC)
	{
		return;
	}

	if (bEnabled)
	{
		if (!ExecutionInvulnerabilityEffectHandle.IsValid())
		{
			// 处决期间执行者通过专用执行保护效果获得无敌。
			// 这里单独记录句柄，是因为这份无敌不走通用基类自动移除链，而要在处决收尾时精确关闭。
			ExecutionInvulnerabilityEffectHandle =
				ActionASC->ApplyExecutionProtectionEffect(ExecutionInvulnerabilityCombatModifierEffect);
		}

		// 已经开过无敌时重复进入这里直接视为幂等成功，避免重复叠层。
		// 这能覆盖“某些分支重复请求开启保护”的情况，而不会把执行者无敌叠成多层。
		return;
	}

	if (ExecutionInvulnerabilityEffectHandle.IsValid())
	{
		// 关闭无敌时按句柄精确移除，避免误伤同标签的其它运行时效果。
		ActionASC->RemoveActiveGameplayEffect(ExecutionInvulnerabilityEffectHandle);
		ExecutionInvulnerabilityEffectHandle.Invalidate();
	}
}

void UHeroGA_Execution::FinishExecutionAbility(bool bWasCancelled)
{
	if (bAbilityFinished)
	{
		return;
	}

	// 所有准备结束的分支都先从这里进入，再统一做一次正式收尾。
	// 先收状态、再 K2_EndAbility，可以避免蓝图或外部结束顺序把处决链留成半套。
	FinalizeExecutionState(bWasCancelled);
	K2_EndAbility();
}

void UHeroGA_Execution::FinalizeExecutionState(bool bWasCancelled)
{
	if (bAbilityFinished)
	{
		return;
	}

	// 这里才是处决所有结束路径最终汇合到的正式收口点。
	// 无论是命中后自然播完，还是没命中就中断，都必须在这里统一回退窗口、释放锁和恢复输入。
	// 无论是正常结束还是中断，都必须先解除执行者自身的处决无敌状态。
	bExecutionPreparationPending = false;
	ExecutionPreparationStartWorldTime = -1.f;
	ExecutionPreparationPollCount = 0;
	LastExecutionPreparationStageText.Reset();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExecutionPreparationPollTimerHandle);
	}
	SetExecutionPerformerInputLock(false);
	SetExecutionInvulnerabilityEnabled(false);

	// 目标侧窗口、victim lock、硬锁和 Poise 收尾现在统一交给目标自己的 ExecutionWindowComponent。
	// 执行者侧 GA 在命中成功后不再提前取消预占或释放目标锁，否则会把目标侧 victim montage 主链抢先拆掉。
	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		if (ActiveExecutionMontage)
		{
			HeroCombatComponent->EndMontageRootMotionOverride(
				Cast<ACharacter>(GetAvatarActorFromActorInfo()),
				ActiveExecutionMontage,
				TEXT("ExecutionFinalize"));
		}

		HeroCombatComponent->SetAttackEnabled(true);

		if (bExecutionWindowReserved)
		{
			if (bExecutionHitApplied)
			{
				// 命中成功后，这次处决窗口的正式收尾权已经交给目标侧 victim montage。
				// 这里只撤掉“当前 GA 持有过这次预占”的本地记账，不再主动取消或释放目标状态。
			}
			else if (bExecutionStarted && !bExecutionHitApplied)
			{
				// 处决已经进入双边演出但最终没有落伤时，目标侧也必须按一次失败演出收尾。
				// 这里不能只取消预占或释放 victim lock，否则窗口不会关闭，Poise 恢复链也不会执行。
				HeroCombatComponent->AbortConsumedExecutionTargetPresentation(CachedExecutionTargetActor.Get());
			}
			else
			{
				// 如果命中帧始终没有真正落伤，则把已经预占的窗口资格退回去。
				// 这保证“预占成功但演出失败”的情况不会把目标永久卡在不可再次处决的状态里。
				// 也就是说，预占只是“临时接管资格”，不是“一旦开始就必须强行消费成功”。
				HeroCombatComponent->CancelReservedExecutionTarget(CachedExecutionTargetActor.Get());
			}

			// 一旦回退完成，就说明当前 GA 不再持有那次处决机会。
			bExecutionWindowReserved = false;
		}
		else if (!bExecutionHitApplied)
		{
			// 即使这次处决还没正式落伤，也必须主动通知目标侧释放受害者锁。
			// 否则目标可能保持“只允许这位执行者命中”的异常保护状态。
			HeroCombatComponent->ReleaseExecutionTargetVictimLock(CachedExecutionTargetActor.Get());
		}
	}

	if (bExecutionStarted)
	{
		// 只有在处决尚未真正命中时，才把这次流程视为取消。
		// 一旦命中帧已经成功落伤，后续蒙太奇即便被中断，也不再广播 Cancelled。
		// 避免监听端同时收到 Hit 和 Cancelled。
		if (!bExecutionHitApplied)
		{
			// 只有“没命中就结束”的情况才是取消。
			// 如果已经在命中帧成功结算，再播 Cancelled 会把监听端语义搞乱。
			BroadcastHeroCombatEvent(ActionGameplayTags::Player_Event_Execution_Cancelled);
		}

		// 无论成功还是取消，只要处决真正进入过执行态，都要有一条统一的 End 事件作为演出收尾信号。
		// 这样监听端可以把 Hit / Cancelled 当成分支语义，再把 End 当成总收口语义使用。
		BroadcastHeroCombatEvent(ActionGameplayTags::Player_Event_Execution_End);
	}

	bAbilityFinished = true;

	Debug::Print(
		FString::Printf(
			TEXT("[GA][Execution] End Cancelled=%d HitApplied=%d Target=%s"),
			bWasCancelled ? 1 : 0,
			bExecutionHitApplied ? 1 : 0,
			*GetNameSafe(CachedExecutionTargetActor.Get())),
		FColor::Green,
		2.0f);

	if (UHeroCombatInputComponent* HeroCombatInputComponent = GetHeroCombatInputComponentFromActorInfo())
	{
		HeroCombatInputComponent->ClearBufferedInputIfMatchesTag(ActionGameplayTags::InputTag_GameplayAbility_Execution);
		HeroCombatInputComponent->ClearInputStateByTag(ActionGameplayTags::InputTag_GameplayAbility_Execution);
	}

	// 处决结束后立刻尝试消费一帧缓冲输入，让后续攻击或闪避衔接更顺畅。
	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		// 这里仍然走战斗组件统一恢复链，而不是 Ability 自己直接读取输入，
		// 目的是让处决后的输入恢复时序继续和攻击、闪避、防御保持一致。
		// 因此 RequestRecoverCombatInputAfterExecution() 只负责恢复时序，不等于当场立即重放输入。
		HeroCombatComponent->RequestRecoverCombatInputAfterExecution();
	}
}

void UHeroGA_Execution::UpdateExecutionPreparationStage(
	const FString& InStageText,
	const FColor& InColor,
	const bool bLogAsWarning)
{
	if (InStageText.IsEmpty() || LastExecutionPreparationStageText == InStageText)
	{
		return;
	}

	LastExecutionPreparationStageText = InStageText;
	Debug::Print(FString::Printf(TEXT("[GA][Execution] %s"), *InStageText), InColor, 2.0f);

	if (bLogAsWarning)
	{
		UE_LOG(LogHeroGA_Execution, Warning, TEXT("%s"), *InStageText);
	}
	else
	{
		UE_LOG(LogHeroGA_Execution, Log, TEXT("%s"), *InStageText);
	}
}

float UHeroGA_Execution::GetExecutionPreparationElapsedSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World || ExecutionPreparationStartWorldTime < 0.f)
	{
		return 0.f;
	}

	return FMath::Max(World->GetTimeSeconds() - ExecutionPreparationStartWorldTime, 0.f);
}

bool UHeroGA_Execution::HasExecutionPreparationTimedOut() const
{
	return ExecutionPreparationTimeoutSeconds > 0.f
		&& ExecutionPreparationStartWorldTime >= 0.f
		&& GetExecutionPreparationElapsedSeconds() >= ExecutionPreparationTimeoutSeconds;
}
