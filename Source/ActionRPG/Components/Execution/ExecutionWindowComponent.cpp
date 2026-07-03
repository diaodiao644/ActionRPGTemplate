// 文件说明：实现处决窗口组件逻辑，负责处决窗口开关、窗口预占、受害者锁定与外部干扰拦截。

#include "Components/Execution/ExecutionWindowComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "AnimNotify/AnimNotify_ExecutionRecoveryUnlock.h"
#include "AbilitySystemInterface.h"
#include "ActionGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/ActionCharacterBase.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "Components/Collision/ActionCollisionRuntimeComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	const TCHAR* LexToStringExecutionRootMotionMode(const ERootMotionMode::Type RootMotionMode)
	{
		switch (RootMotionMode)
		{
		case ERootMotionMode::NoRootMotionExtraction:
			return TEXT("NoRootMotionExtraction");
		case ERootMotionMode::IgnoreRootMotion:
			return TEXT("IgnoreRootMotion");
		case ERootMotionMode::RootMotionFromEverything:
			return TEXT("RootMotionFromEverything");
		case ERootMotionMode::RootMotionFromMontagesOnly:
			return TEXT("RootMotionFromMontagesOnly");
		default:
			return TEXT("Unknown");
		}
	}
}

UExecutionWindowComponent::UExecutionWindowComponent()
	: Super()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// 受害者锁定效果默认比处决窗口持续得更久，
	// 这样窗口被预占后，即便演出链跨了多个蒙太奇阶段，也不会因为效果过早结束而失去保护。
	ExecutionVictimLockCombatModifierEffect.Duration = 10.f;
	ExecutionVictimLockCombatModifierEffect.StatusEffectTag =
		ActionGameplayTags::StatusEffect_Combat_ExecutionVictimLocked;
	ExecutionVictimLockCombatModifierEffect.GrantedTags.AddTag(
		ActionGameplayTags::State_Combat_ExecutionVictimLocked);

	ExecutionVictimHardLockCombatModifierEffect.Duration = 10.f;
	ExecutionVictimHardLockCombatModifierEffect.StatusEffectTag =
		ActionGameplayTags::StatusEffect_Combat_ExecutionVictimHardLock;
	ExecutionVictimHardLockCombatModifierEffect.GrantedTags.AddTag(
		ActionGameplayTags::State_Combat_ExecutionVictim_HardLock);
}

void UExecutionWindowComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateExecutionPresentationPreparation(DeltaTime);
}

bool UExecutionWindowComponent::HandleCombatReactEvent(FGameplayTag EventTag, AActor* InstigatorActor)
{
	// 先清理运行时里已经失效的弱引用。
	// 这一步很关键：如果上一次执行者 Actor 已经销毁，但状态还残留，
	// 后续资格判断会把窗口错误地视为“已预占 / 已锁定”。
	if (ExecutionWindowRuntimeState.bExecutionWindowReserved
		&& !ExecutionWindowRuntimeState.ReservedExecutionInstigator.IsValid())
	{
		ExecutionWindowRuntimeState.ClearReservation();
	}

	if (ExecutionWindowRuntimeState.bExecutionVictimLocked
		&& !ExecutionWindowRuntimeState.ExecutionVictimLockInstigator.IsValid())
	{
		ExecutionWindowRuntimeState.ClearExecutionVictimLock();
		RemoveExecutionVictimLockEffect();
	}

	if (ExecutionWindowRuntimeState.IsWindowOpen()
		&& !ExecutionWindowRuntimeState.HasReservedExecution())
	{
		// 窗口已经打开但还没人真正接手时，
		// 每次新的战斗反应事件都允许把剩余时间重新刷新，避免玩家刚打出失衡窗口就被时长吃掉。
		RestartExecutionWindowTimer();
	}

	if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
	{
		// 当前版本先把“破韧”作为打开处决窗口的统一入口。
		OpenExecutionWindow(InstigatorActor);
		return true;
	}

	return false;
}

bool UExecutionWindowComponent::CanBeExecutedBy(AActor* InstigatorActor) const
{
	return CanBeExecutedByWithReason(InstigatorActor, nullptr);
}

bool UExecutionWindowComponent::CanBeExecutedByWithReason(AActor* InstigatorActor, FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	// 这个函数是整条处决链最基础的“资格闸门”：
	// 1. 先把运行时里可能残留的脏状态清掉；
	// 2. 再检查窗口是否存在、目标是否仍然有效；
	// 3. 最后检查这次处决资格是否仍然归属于当前执行者。
	// 只要其中任何一步不成立，就不允许继续往预占或提交阶段推进。
	// 这一组清理与 HandleCombatReactEvent 中的逻辑保持一致。
	// 原因是资格查询会被很多入口直接调用，不能假设外部一定先走过事件入口。
	if (ExecutionWindowRuntimeState.bExecutionWindowReserved
		&& !ExecutionWindowRuntimeState.ReservedExecutionInstigator.IsValid())
	{
		UExecutionWindowComponent* MutableThis = const_cast<UExecutionWindowComponent*>(this);
		MutableThis->ExecutionWindowRuntimeState.ClearReservation();
	}

	if (ExecutionWindowRuntimeState.bExecutionVictimLocked
		&& !ExecutionWindowRuntimeState.ExecutionVictimLockInstigator.IsValid())
	{
		UExecutionWindowComponent* MutableThis = const_cast<UExecutionWindowComponent*>(this);
		MutableThis->ExecutionWindowRuntimeState.ClearExecutionVictimLock();
		MutableThis->RemoveExecutionVictimLockEffect();
	}

	if (ExecutionWindowRuntimeState.IsWindowOpen()
		&& !ExecutionWindowRuntimeState.HasReservedExecution())
	{
		// 只要窗口还处于“开放但无人接手”的阶段，就允许资格查询顺手刷新一次超时。
		// 这样玩家在窗口边缘反复进入检测范围时，不会因为纯查询动作把窗口白白耗尽。
		UExecutionWindowComponent* MutableThis = const_cast<UExecutionWindowComponent*>(this);
		MutableThis->RestartExecutionWindowTimer();
	}

	if (!ExecutionWindowRuntimeState.IsWindowOpen() || !IsValid(InstigatorActor))
	{
		// 没有窗口，或执行者本身已经无效时，资格直接不成立。
		if (OutFailureReason)
		{
			*OutFailureReason = !ExecutionWindowRuntimeState.IsWindowOpen()
				? FString::Printf(TEXT("目标 %s 当前没有可用处决窗口。"), *GetNameSafe(GetOwner()))
				: TEXT("当前执行者无效，无法消费处决窗口。");
		}
		return false;
	}

	const UActionAttributeSetBase* AttributeSet = GetOwningAttributeSet();
	if (!AttributeSet || !AttributeSet->IsAlive())
	{
		// 被处决目标若已经死亡，就不应该再继续提供处决资格。
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("目标 %s 已死亡，不能继续作为处决目标。"), *GetNameSafe(GetOwner()));
		}
		return false;
	}

	// 若当前窗口仍记录着合法触发者，则只允许这次真正打出失衡的人消费该窗口。
	if (AActor* LastExecutionInstigator = ExecutionWindowRuntimeState.GetLastExecutionInstigator())
	{
		if (LastExecutionInstigator != InstigatorActor)
		{
			// 当前窗口若仍然记着“是谁打出了这次失衡”，
			// 那就只允许同一执行者继续推进处决，避免多人或多来源同时争抢同一个窗口语义。
			if (OutFailureReason)
			{
				*OutFailureReason = FString::Printf(
					TEXT("目标 %s 的处决窗口当前归触发者 %s 所有，当前执行者 %s 不能消费。"),
					*GetNameSafe(GetOwner()),
					*GetNameSafe(LastExecutionInstigator),
					*GetNameSafe(InstigatorActor));
			}
			return false;
		}
	}

	// 若窗口已被别的执行者预占，则后续只允许同一执行者继续提交或取消。
	if (ExecutionWindowRuntimeState.HasReservedExecution()
		&& !ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor))
	{
		// 预占一旦成立，窗口所有权就已经从“开放资格”进入“专属资格”。
		// 后续任何不是这个执行者发起的推进请求，都要在这里被挡掉。
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 的处决窗口已被其他执行者 %s 预占。"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(ExecutionWindowRuntimeState.ReservedExecutionInstigator.Get()));
		}
		return false;
	}

	// 若目标已被另一名执行者锁定为“处决受害者”，则当前执行者不能再抢占。
	if (ExecutionWindowRuntimeState.HasExecutionVictimLock()
		&& !ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor))
	{
		// 受害者锁定比单纯预占更强。
		// 它代表目标已经进入某个执行者的处决演出保护中，此时别人连“尝试接管”都不应该被允许。
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 当前已被其他执行者 %s 锁定为处决受害者。"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(ExecutionWindowRuntimeState.ExecutionVictimLockInstigator.Get()));
		}
		return false;
	}

	return true;
}

bool UExecutionWindowComponent::TryReserveExecutionWindow(AActor* InstigatorActor)
{
	// 预占阶段的意义是：
	// 1. 先把“谁有资格处决”从开放状态收口到某一个执行者；
	// 2. 同时给目标挂上受害者锁定保护，阻止外部干扰；
	// 3. 暂停窗口计时，让后续演出链在一个稳定状态里继续运行。
	// 预占阶段同样先收口失效引用，避免把一个实际上已经脏掉的旧状态继续向下推进成正式锁定。
	if (ExecutionWindowRuntimeState.bExecutionWindowReserved
		&& !ExecutionWindowRuntimeState.ReservedExecutionInstigator.IsValid())
	{
		ExecutionWindowRuntimeState.ClearReservation();
	}

	if (ExecutionWindowRuntimeState.bExecutionVictimLocked
		&& !ExecutionWindowRuntimeState.ExecutionVictimLockInstigator.IsValid())
	{
		ExecutionWindowRuntimeState.ClearExecutionVictimLock();
		RemoveExecutionVictimLockEffect();
	}

	if (ExecutionWindowRuntimeState.IsWindowOpen()
		&& !ExecutionWindowRuntimeState.HasReservedExecution())
	{
		// 进入预占流程前若窗口还是开放态，就把超时再向后顺一下，
		// 避免“玩家刚碰到可处决条件，下一帧开始预占时窗口却恰好超时”的边界问题。
		RestartExecutionWindowTimer();
	}

	if (!CanBeExecutedBy(InstigatorActor))
	{
		// 预占不重复发明自己的资格规则，统一复用资格闸门结果。
		return false;
	}

	if (ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor)
		&& ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor))
	{
		// 同一执行者重复进入预占时视为幂等成功：
		// 1. 不重复改写运行时状态；
		// 2. 只在锁定效果句柄丢失时补回保护层；
		// 3. 这样可以支持演出链里多次校验或重复调用同一入口。
		if (!ExecutionVictimLockEffectHandle.IsValid())
		{
			ApplyExecutionVictimLockEffect();
		}

		return true;
	}

	// 处决开始演出后，先暂停窗口超时，避免演出期间窗口被计时器提前关闭。
	if (GetWorld())
	{
		// 一旦正式预占，窗口就不再交给自然倒计时管理，
		// 而是交给后续提交、取消或演出收尾主动关闭。
		GetWorld()->GetTimerManager().ClearTimer(ExecutionWindowRuntimeState.ExecutionWindowTimerHandle);
	}

	ExecutionWindowRuntimeState.ReserveWindow(InstigatorActor);
	ExecutionWindowRuntimeState.LockExecutionVictim(InstigatorActor);
	// 顺序上先写运行时状态，再补 GAS 侧锁定效果。
	// 这样即便效果施加链未来被扩展，内部逻辑层也已经先拿到了稳定的“谁预占了窗口、谁锁定了受害者”结论。
	ApplyExecutionVictimLockEffect();
	return true;
}

bool UExecutionWindowComponent::TryCommitExecutionWindow(AActor* InstigatorActor)
{
	// 提交阶段的职责非常单一：
	// 只负责确认“这次正式命中是否有权消费当前窗口”，
	// 一旦成立，就把窗口标记为“已正式消费，等待目标被处决蒙太奇结束后再关窗”。
	if (ExecutionWindowRuntimeState.bExecutionWindowReserved
		&& !ExecutionWindowRuntimeState.ReservedExecutionInstigator.IsValid())
	{
		ExecutionWindowRuntimeState.ClearReservation();
	}

	if (ExecutionWindowRuntimeState.bExecutionVictimLocked
		&& !ExecutionWindowRuntimeState.ExecutionVictimLockInstigator.IsValid())
	{
		ExecutionWindowRuntimeState.ClearExecutionVictimLock();
		RemoveExecutionVictimLockEffect();
	}

	if (ExecutionWindowRuntimeState.IsWindowOpen()
		&& !ExecutionWindowRuntimeState.HasReservedExecution())
	{
		// 提交前若窗口仍是开放态，也先刷新一次计时，
		// 保证“从资格检查切到真正命中提交”的这一小段间隔不会被超时打断。
		RestartExecutionWindowTimer();
	}

	if (!IsValid(InstigatorActor) || !ExecutionWindowRuntimeState.IsWindowOpen())
	{
		// 正式命中时，执行者无效或窗口已经不在，都不能再消费窗口。
		return false;
	}

	// 正式提交阶段只接受“已经由同一执行者预占并锁定”的处决窗口。
	// 这里不再复用 CanBeExecutedBy 的“目标必须存活”判断，
	// 因为处决命中本身就可能在提交前一帧把目标打死；若仍要求存活，会导致命中成功后无法消费窗口。
	if (!ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor)
		|| !ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor))
	{
		// 正式结算时必须同时满足“我预占了窗口”和“目标锁定归我所有”。
		// 少任意一个条件，都说明这次命中不该拥有消费窗口的权限。
		return false;
	}

	// 到这里并不会立刻 CloseExecutionWindow(true)。
	// 当前正式语义是：命中帧只把窗口升级成“已消费，等待目标侧被处决蒙太奇收尾”，
	// 真正的关窗、广播和 Poise 恢复仍统一交给 victim montage end / abort 链处理。
	bExecutionConsumedPendingVictimMontageEnd = true;
	return true;
}

void UExecutionWindowComponent::CancelReservedExecutionWindow(AActor* InstigatorActor)
{
	if (!ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor))
	{
		return;
	}

	ClearExecutionPresentationPreparationRuntime();
	bExecutionConsumedPendingVictimMontageEnd = false;

	// 这里先撤销“资格归属”，再释放受害者锁。
	// 这样外部系统如果在中间帧查询，会先看到“当前已不再由这个执行者占有窗口”，
	// 不会短暂读到“资格已经取消，但窗口仍然声称归他所有”的混合状态。
	ExecutionWindowRuntimeState.ClearReservation();
	ReleaseExecutionVictimLock(InstigatorActor);

	// 取消处决后，把剩余资格重新还给当前窗口，并重新开始计时。
	if (ExecutionWindowRuntimeState.IsWindowOpen())
	{
		// 只有窗口本体还在时才恢复倒计时。
		// 若窗口本来已经关闭，则这里不应擅自把一次已结束的处决机会重新续回来。
		RestartExecutionWindowTimer();
	}
}

void UExecutionWindowComponent::OpenExecutionWindow(AActor* InstigatorActor)
{
	if (!GetWorld() || !GetOwner())
	{
		// 没有世界或拥有者时，窗口既无法计时也没有广播对象，直接放弃打开。
		return;
	}

	const UActionAttributeSetBase* AttributeSet = GetOwningAttributeSet();
	if (!AttributeSet || !AttributeSet->IsAlive())
	{
		// 已经死亡的目标不应再进入处决窗口。
		return;
	}

	if (ExecutionWindowRuntimeState.bExecutionWindowReserved
		&& !ExecutionWindowRuntimeState.ReservedExecutionInstigator.IsValid())
	{
		ExecutionWindowRuntimeState.ClearReservation();
	}

	if (ExecutionWindowRuntimeState.bExecutionVictimLocked
		&& !ExecutionWindowRuntimeState.ExecutionVictimLockInstigator.IsValid())
	{
		ExecutionWindowRuntimeState.ClearExecutionVictimLock();
		RemoveExecutionVictimLockEffect();
	}

	if (ExecutionWindowRuntimeState.HasReservedExecution()
		|| ExecutionWindowRuntimeState.HasExecutionVictimLock())
	{
		// 只要窗口已经被某人正式接手，就不再允许“刷新打开”。
		// 否则会把处决中的专属窗口错误地重新当成开放窗口处理。
		return;
	}

	ClearExecutionPresentationPreparationRuntime();
	bExecutionConsumedPendingVictimMontageEnd = false;
	bPoiseBreakPresentationRestoreApplied = false;

	const bool bWasWindowAlreadyOpen = ExecutionWindowRuntimeState.IsWindowOpen();
	AActor* PreviousInstigator = ExecutionWindowRuntimeState.GetLastExecutionInstigator();
	ExecutionWindowRuntimeState.OpenWindow(InstigatorActor);
	// 打开窗口后立刻重建超时来源，确保“当前这次可处决机会”从这一刻重新开始完整计时。
	RestartExecutionWindowTimer();

	// 若只是同一触发者刷新持续时间，则不重复广播打开事件，避免 UI 与监听逻辑重复响应。
	// 这里故意把“刷新计时”和“广播打开”拆成两层：
	// 1. 计时需要每次都刷新，保证窗口寿命总是从最新一次失衡重新开始；
	// 2. 广播只在语义真的变成“新窗口”时发，避免界面提示和能力任务被重复拉起。
	if (!bWasWindowAlreadyOpen || PreviousInstigator != InstigatorActor)
	{
		BroadcastExecutionEvent(ActionGameplayTags::Combat_Event_ExecutionWindow_Open, InstigatorActor);
	}
}

void UExecutionWindowComponent::CloseExecutionWindow(bool bWasConsumed)
{
	if (!GetOwner())
	{
		return;
	}

	ClearExecutionPresentationPreparationRuntime();

	// 关闭窗口时，不管是被消费还是自然结束，第一步都先停掉超时计时器。
	// 否则同一窗口在关闭后仍可能被旧计时器再次触发一次过期回调。
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExecutionWindowRuntimeState.ExecutionWindowTimerHandle);
	}

	if (bExecutionVictimRuntimeActive)
	{
		FinalizeExecutionVictimRuntime(false);
	}

	if (ExecutionRootMotionOverrideRuntime.bActive)
	{
		EndExecutionRootMotionOverride(nullptr, TEXT("CloseExecutionWindow"));
	}

	const bool bWasOpenBeforeClose = ExecutionWindowRuntimeState.IsWindowOpen();
	AActor* LastExecutionInstigator = ExecutionWindowRuntimeState.GetLastExecutionInstigator();
	// CloseWindow 会一并清掉“窗口打开 / 预占 / 受害者锁定”等运行时状态，
	// 因此下面需要提前缓存“关闭前是否打开”和“最后触发者是谁”，供事件广播继续使用。
	// 这也是为什么事件广播被放在 CloseWindow 之后但缓存读取又要放在它之前：
	// 真正的逻辑状态要先收口干净，对外广播则仍要带上关闭前的上下文。
	ExecutionWindowRuntimeState.CloseWindow();
	ExecutionWindowRuntimeState.ClearExecutionVictimLock();
	RemoveExecutionVictimLockEffect();
	// 从这一行开始，组件内部已经把窗口、预占和受害者锁都视为正式结束。
	// 因此后面的广播只负责“通知外界刚才发生了什么”，
	// 不再允许外部从本组件读取到一份仍处于旧窗口状态的中间态。

	if (bWasConsumed)
	{
		// 只有真正被处决消费时，才广播 Execution_Triggered。
		// 这条事件通常会被处决 Ability、镜头演出或战斗统计链当作正式结算信号。
		BroadcastExecutionEvent(
			ActionGameplayTags::Combat_Event_Execution_Triggered,
			LastExecutionInstigator);
	}

	if (bWasOpenBeforeClose)
	{
		// 无论是消费还是自然结束，只要窗口之前确实打开过，都要广播一次“窗口已关闭”。
		// 这样 UI 提示层和监听组件不需要再区分关闭来源，就能统一撤掉可处决提示。
		BroadcastExecutionEvent(
			ActionGameplayTags::Combat_Event_ExecutionWindow_Close,
			LastExecutionInstigator);
	}

	const UActionAttributeSetBase* AttributeSet = GetOwningAttributeSet();
	const bool bOwnerAlive = AttributeSet && AttributeSet->IsAlive();
	const bool bShouldRestorePoise = bOwnerAlive && (bWasConsumed || bRestorePoiseOnClose);
	FName RestoreReason(TEXT("target_dead"));
	if (!AttributeSet)
	{
		RestoreReason = FName(TEXT("missing_attribute_set"));
	}
	else if (bOwnerAlive)
	{
		RestoreReason = bWasConsumed
			? FName(TEXT("consumed_execution_force_restore"))
			: (bRestorePoiseOnClose
				? FName(TEXT("natural_close_restore_enabled"))
				: FName(TEXT("restore_disabled_for_natural_close")));
	}

	if (bShouldRestorePoise)
	{
		RestorePoiseToMaxForExecution(RestoreReason, true);
	}
	else
	{
		const float PoiseBefore = AttributeSet ? AttributeSet->GetPoise() : 0.f;
		const float MaxPoise = AttributeSet ? AttributeSet->GetMaxPoise() : 0.f;
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[ExecutionWindow] PoiseRestore skipped owner=%s source=CloseWindow consumed=%s restore_on_close=%s alive=%s reason=%s poise_before=%.2f max_poise=%.2f poise_after=%.2f used_asc_path=false"),
			*GetNameSafe(GetOwner()),
			bWasConsumed ? TEXT("true") : TEXT("false"),
			bRestorePoiseOnClose ? TEXT("true") : TEXT("false"),
			bOwnerAlive ? TEXT("true") : TEXT("false"),
			*RestoreReason.ToString(),
			PoiseBefore,
			MaxPoise,
			PoiseBefore);
	}
	// 若目标已经死亡，则这里仍然不会恢复韧性。
	// 这样能避免死亡目标在处决收尾后又被补出一份“数值上恢复正常”的假状态。

	// 清掉这次窗口残留的触发者，避免后续读取到陈旧数据。
	ExecutionWindowRuntimeState.ClearLastExecutionInstigator();
	bExecutionConsumedPendingVictimMontageEnd = false;
	bPoiseBreakPresentationRestoreApplied = false;
}

void UExecutionWindowComponent::RestorePoiseAfterPoiseBreakPresentationEnd()
{
	if (!ExecutionWindowRuntimeState.IsWindowOpen()
		|| bExecutionConsumedPendingVictimMontageEnd
		|| bExecutionVictimRuntimeActive
		|| ExecutionWindowRuntimeState.HasReservedExecution()
		|| ExecutionWindowRuntimeState.HasExecutionVictimLock()
		|| bPoiseBreakPresentationRestoreApplied)
	{
		return;
	}

	RestorePoiseToMaxForExecution(FName(TEXT("poise_break_presentation_end")), true);
	bPoiseBreakPresentationRestoreApplied = true;
}

void UExecutionWindowComponent::HandlePoiseBreakPresentationEndedWithoutExecution()
{
	if (!ExecutionWindowRuntimeState.IsWindowOpen() || bExecutionConsumedPendingVictimMontageEnd)
	{
		return;
	}

	// 这个入口只服务“PoiseBreak 已经把处决窗口打开，但最终没有进入正式处决消费”的那条收尾链。
	// 它不是普通恢复动画的附属逻辑，而是目标侧窗口主链对“破韧机会未被消费”的正式裁决点。
	CloseExecutionWindow(false);
	RestorePoiseToMaxForExecution(FName(TEXT("poise_break_window_closed_force_restore")), true);
}

bool UExecutionWindowComponent::IsExecutionVictimLockedBy(AActor* InstigatorActor) const
{
	// 这里直接走运行时状态查询，不附带任何副作用。
	// 适合给受击保护、处决 Ability 或 UI 做“当前锁定是否归某执行者所有”的快速判断。
	return ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor);
}

AActor* UExecutionWindowComponent::GetExecutionVictimLockInstigator() const
{
	return ExecutionWindowRuntimeState.GetExecutionVictimLockInstigator();
}

bool UExecutionWindowComponent::IsExecutionWindowReservedBy(AActor* InstigatorActor) const
{
	// 和受害者锁定查询一样，这里只回答“预占关系是否存在”，不顺手改动任何状态。
	return ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor);
}

void UExecutionWindowComponent::ReleaseExecutionVictimLock(AActor* InstigatorActor)
{
	if (!ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor))
	{
		return;
	}

	ClearExecutionPresentationPreparationRuntime();

	// 先收 victim runtime，再结束 root motion override，最后才真正释放目标锁。
	// 这样可以保证：外部系统先看到“目标不再处于被处决演出运行态”，
	// 再看到“目标不再归某个执行者锁定”，不会短暂读到一份顺序颠倒的半状态。
	FinalizeExecutionVictimRuntime(false);
	EndExecutionRootMotionOverride(nullptr, TEXT("ExecutionVictimLockReleased"));

	// 只有当前确实持有这把锁的执行者，才有资格释放它。
	// 这保证了其它外部系统无法误把仍在进行中的处决保护提前拆掉。
	ExecutionWindowRuntimeState.ClearExecutionVictimLock();
	// 逻辑层锁定释放后，GAS 侧的锁定标签也必须同步移除。
	// 否则外部系统会继续把目标误判为“仍处于处决保护中”。
	RemoveExecutionVictimLockEffect();
}

void UExecutionWindowComponent::AbortConsumedExecutionPresentation(AActor* InstigatorActor)
{
	if (!IsValid(InstigatorActor))
	{
		return;
	}

	if (!ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor)
		&& !ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor))
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[ExecutionWindow] ExecutionPresentation aborted owner=%s instigator=%s window_open=%s reserved_by_instigator=%s locked_by_instigator=%s reason=execution_started_without_hit"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(InstigatorActor),
		ExecutionWindowRuntimeState.IsWindowOpen() ? TEXT("true") : TEXT("false"),
		ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor) ? TEXT("true") : TEXT("false"),
		ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor) ? TEXT("true") : TEXT("false"));

	// abort 语义固定为：
	// 1. 双边处决已经开始，但命中帧没有成功把窗口正式消费；
	// 2. 因此目标侧要按“未消费失败链”回收 victim runtime、目标锁和 Poise；
	// 3. 这个裁决必须由目标组件自己执行，不能由执行者侧直接模拟一套收尾结果。
	FinalizeExecutionVictimRuntime(true);
	EndExecutionRootMotionOverride(nullptr, TEXT("ExecutionPresentationAborted"));
	bExecutionConsumedPendingVictimMontageEnd = false;
	if (ExecutionWindowRuntimeState.IsWindowOpen())
	{
		CloseExecutionWindow(false);
		RestorePoiseToMaxForExecution(FName(TEXT("execution_presentation_aborted_force_restore")), true);
	}
	else
	{
		ExecutionWindowRuntimeState.ClearReservation();
		ExecutionWindowRuntimeState.ClearExecutionVictimLock();
		RemoveExecutionVictimLockEffect();
		RestorePoiseToMaxForExecution(FName(TEXT("execution_presentation_aborted_force_restore")), true);
	}
}

bool UExecutionWindowComponent::HasVictimExecutionPresentationForWeaponSubtype(const FGameplayTag WeaponSubtypeTag) const
{
	const FActionExecutionVictimAnimConfig* VictimAnimConfig = FindVictimExecutionAnimConfig(WeaponSubtypeTag);
	return VictimAnimConfig && VictimAnimConfig->IsValidConfig();
}

bool UExecutionWindowComponent::HasExecutionRecoveryUnlockNotifyForWeaponSubtype(const FGameplayTag WeaponSubtypeTag) const
{
	const FActionExecutionVictimAnimConfig* VictimAnimConfig = FindVictimExecutionAnimConfig(WeaponSubtypeTag);
	return VictimAnimConfig
		&& VictimAnimConfig->IsValidConfig()
		&& HasExecutionRecoveryUnlockNotify(VictimAnimConfig->VictimExecutionMontage);
}

bool UExecutionWindowComponent::CanPrepareExecutionPresentationBy(
	AActor* InstigatorActor,
	const FGameplayTag& WeaponSubtypeTag,
	FString* OutFailureReason) const
{
	if (!CanBeExecutedByWithReason(InstigatorActor, OutFailureReason))
	{
		return false;
	}

	if (!WeaponSubtypeTag.IsValid())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前武器 WeaponSubtypeTag 无效，无法解析目标侧被处决动画。");
		}
		return false;
	}

	if (!HasVictimExecutionPresentationForWeaponSubtype(WeaponSubtypeTag))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 缺少 WeaponSubtypeTag=%s 对应的被处决动画配置。"),
				*GetNameSafe(GetOwner()),
				*WeaponSubtypeTag.ToString());
		}
		return false;
	}

	const FActionExecutionVictimAnimConfig* VictimAnimConfig = FindVictimExecutionAnimConfig(WeaponSubtypeTag);
	if (!VictimAnimConfig || !HasExecutionRecoveryUnlockNotify(VictimAnimConfig->VictimExecutionMontage))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 的被处决蒙太奇 %s 缺少 ExecutionRecoveryUnlock Notify。"),
				*GetNameSafe(GetOwner()),
				VictimAnimConfig ? *GetNameSafe(VictimAnimConfig->VictimExecutionMontage) : TEXT("None"));
		}
		return false;
	}

	return true;
}

bool UExecutionWindowComponent::TryBeginExecutionPresentationPreparation(
	AActor* InstigatorActor,
	const FGameplayTag& WeaponSubtypeTag,
	const float TurnDurationSeconds,
	FString* OutFailureReason)
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!IsValid(InstigatorActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("目标侧处决前准备的执行者引用无效。");
		}
		return false;
	}

	if (!ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 当前并未被执行者 %s 预占，无法启动处决前准备。"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(InstigatorActor));
		}
		return false;
	}

	if (!ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 的 victim lock 当前不归执行者 %s 所有，无法启动处决前准备。"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(InstigatorActor));
		}
		return false;
	}

	if (!HasVictimExecutionPresentationForWeaponSubtype(WeaponSubtypeTag))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 缺少 WeaponSubtypeTag=%s 对应的被处决动画配置，无法启动处决前准备。"),
				*GetNameSafe(GetOwner()),
				*WeaponSubtypeTag.ToString());
		}
		return false;
	}

	ACharacter* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("目标 %s 不是有效 Character，无法执行处决前转向。"), *GetNameSafe(GetOwner()));
		}
		return false;
	}

	ExecutionPresentationPreparationInstigator = InstigatorActor;
	ExecutionPresentationPreparationWeaponSubtypeTag = WeaponSubtypeTag;
	ExecutionPresentationPreparationElapsedSeconds = 0.f;
	ExecutionPresentationPreparationDurationSeconds = FMath::Max(TurnDurationSeconds, 0.f);
	bExecutionPresentationPreparationActive = true;
	bExecutionPresentationPreparationReady = false;

	UpdateExecutionPresentationPreparation(0.f);
	return bExecutionPresentationPreparationActive || bExecutionPresentationPreparationReady;
}

bool UExecutionWindowComponent::IsExecutionPresentationPreparationReady(AActor* InstigatorActor, FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!bExecutionPresentationPreparationReady)
	{
		if (OutFailureReason)
		{
			FString PreparationStateDescription;
			DescribeExecutionPresentationPreparationState(InstigatorActor, PreparationStateDescription, OutFailureReason);
			if (OutFailureReason->IsEmpty())
			{
				*OutFailureReason = PreparationStateDescription.IsEmpty()
					? TEXT("目标侧处决前准备尚未完成。")
					: PreparationStateDescription;
			}
		}
		return false;
	}

	if (!ExecutionPresentationPreparationInstigator.IsValid())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("目标侧处决前准备已标记为 Ready，但执行者引用已失效。");
		}
		return false;
	}

	if (ExecutionPresentationPreparationInstigator.Get() != InstigatorActor)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标侧处决前准备当前归执行者 %s 所有，当前执行者 %s 不能直接接管。"),
				*GetNameSafe(ExecutionPresentationPreparationInstigator.Get()),
				*GetNameSafe(InstigatorActor));
		}
		return false;
	}

	return true;
}

bool UExecutionWindowComponent::TryStartPreparedExecutionPresentation(AActor* InstigatorActor, FString* OutFailureReason)
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!IsExecutionPresentationPreparationReady(InstigatorActor, OutFailureReason))
	{
		return false;
	}

	ACharacter* OwnerCharacter = GetOwningCharacter();
	const FActionExecutionVictimAnimConfig* VictimAnimConfig =
		FindVictimExecutionAnimConfig(ExecutionPresentationPreparationWeaponSubtypeTag);
	if (!OwnerCharacter || !VictimAnimConfig || !VictimAnimConfig->IsValidConfig())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = !OwnerCharacter
				? FString::Printf(TEXT("目标 %s 不是有效 Character，无法播放被处决演出。"), *GetNameSafe(GetOwner()))
				: FString::Printf(
					TEXT("目标 %s 当前 WeaponSubtypeTag=%s 的被处决演出配置无效。"),
					*GetNameSafe(GetOwner()),
					*ExecutionPresentationPreparationWeaponSubtypeTag.ToString());
		}
		ClearExecutionPresentationPreparationRuntime();
		return false;
	}

	ClearExecutionVictimRuntime();

	if (UActionCombatReactComponent* CombatReactComponent = GetOwner()->FindComponentByClass<UActionCombatReactComponent>())
	{
		if (CombatReactComponent->IsCombatReactActive())
		{
			FString CombatReactHandoffFailureReason;
			if (!CombatReactComponent->HandOffActiveCombatReactToExecutionVictim(&CombatReactHandoffFailureReason))
			{
				if (OutFailureReason)
				{
					*OutFailureReason = CombatReactHandoffFailureReason.IsEmpty()
						? TEXT("目标当前受击运行态不能交接给处决 victim 演出。")
						: CombatReactHandoffFailureReason;
				}

				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[ExecutionWindow] execution_victim_combat_react_handoff_failed owner=%s instigator=%s current_react_event=%s current_react_montage=%s combat_react_active=%s movement_restricted=%s waiting_for_landing=%s hold_until_timer_expires=%s execution_window_open=%s victim_locked_by_instigator=%s reason=%s"),
					*GetNameSafe(GetOwner()),
					*GetNameSafe(InstigatorActor),
					*CombatReactComponent->GetCurrentReactEventTag().ToString(),
					*GetNameSafe(CombatReactComponent->GetCurrentReactMontage()),
					CombatReactComponent->IsCombatReactActive() ? TEXT("true") : TEXT("false"),
					CombatReactComponent->IsCombatReactMovementRestricted() ? TEXT("true") : TEXT("false"),
					CombatReactComponent->IsWaitingForLanding() ? TEXT("true") : TEXT("false"),
					CombatReactComponent->IsHoldingReactUntilTimerExpires() ? TEXT("true") : TEXT("false"),
					ExecutionWindowRuntimeState.IsWindowOpen() ? TEXT("true") : TEXT("false"),
					ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor) ? TEXT("true") : TEXT("false"),
					*CombatReactHandoffFailureReason);

				ClearExecutionPresentationPreparationRuntime();
				return false;
			}

			UE_LOG(
				LogTemp,
				Log,
				TEXT("[ExecutionWindow] execution_victim_combat_react_handoff_success owner=%s instigator=%s current_react_event=%s current_react_montage=%s combat_react_active=%s movement_restricted=%s waiting_for_landing=%s hold_until_timer_expires=%s execution_window_open=%s victim_locked_by_instigator=%s handed_off=%s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(InstigatorActor),
				*CombatReactComponent->GetCurrentReactEventTag().ToString(),
				*GetNameSafe(CombatReactComponent->GetCurrentReactMontage()),
				CombatReactComponent->IsCombatReactActive() ? TEXT("true") : TEXT("false"),
				CombatReactComponent->IsCombatReactMovementRestricted() ? TEXT("true") : TEXT("false"),
				CombatReactComponent->IsWaitingForLanding() ? TEXT("true") : TEXT("false"),
				CombatReactComponent->IsHoldingReactUntilTimerExpires() ? TEXT("true") : TEXT("false"),
				ExecutionWindowRuntimeState.IsWindowOpen() ? TEXT("true") : TEXT("false"),
				ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor) ? TEXT("true") : TEXT("false"),
				CombatReactComponent->WasHandedOffToExecutionVictim() ? TEXT("true") : TEXT("false"));
		}
	}

	// 先尝试把目标当前仍在运行的 CombatReact 正式交接给 victim 演出，
	// 再开启 root motion override 和目标侧被处决蒙太奇。
	// 这样可以保证目标不会同时保留两套“正在接管身体表现”的正式运行态。
	BeginExecutionRootMotionOverride(VictimAnimConfig->VictimExecutionMontage, TEXT("ExecutionVictim"));
	const bool bDidPlayMontage =
		OwnerCharacter->PlayAnimMontage(VictimAnimConfig->VictimExecutionMontage) > 0.f;
	if (!bDidPlayMontage)
	{
		EndExecutionRootMotionOverride(VictimAnimConfig->VictimExecutionMontage, TEXT("ExecutionVictimPlayFailed"));
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 的被处决蒙太奇播放失败。Montage=%s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(VictimAnimConfig->VictimExecutionMontage));
		}
	}
	else if (bDidPlayMontage)
	{
		if (USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
			{
				FOnMontageEnded MontageEndedDelegate;
				MontageEndedDelegate.BindUObject(this, &ThisClass::HandleVictimExecutionMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, VictimAnimConfig->VictimExecutionMontage);
			}
		}

		bExecutionVictimRuntimeActive = true;
		bExecutionVictimHardLockActive = true;
		bExecutionRecoveryUnlockFrameReceived = false;
		ExecutionVictimRuntimeInstigator = InstigatorActor;
		ExecutionVictimRuntimeWeaponSubtypeTag = ExecutionPresentationPreparationWeaponSubtypeTag;
		// 只有蒙太奇真正开播成功后，才把 victim runtime 和 hard lock 写成正式激活。
		// 这样 unlock notify、外部干扰拦截和收尾回调读到的状态，都会和“演出是否真的开始”保持一致。
		ApplyExecutionVictimHardLockEffect();

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[ExecutionWindow] ExecutionVictimHardLock begin owner=%s instigator=%s weapon_subtype=%s montage=%s unlock_notify=ExecutionRecoveryUnlock"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(InstigatorActor),
			*ExecutionPresentationPreparationWeaponSubtypeTag.ToString(),
			*GetNameSafe(VictimAnimConfig->VictimExecutionMontage));
	}

	ClearExecutionPresentationPreparationRuntime();
	return bDidPlayMontage;
}

void UExecutionWindowComponent::HandleVictimExecutionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ExecutionRootMotionOverrideRuntime.Montage.Get())
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[ExecutionWindow] VictimExecutionMontageEnded owner=%s montage=%s interrupted=%s hard_lock_active=%s consumed_pending_close=%s weapon_subtype=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Montage),
		bInterrupted ? TEXT("true") : TEXT("false"),
		bExecutionVictimRuntimeActive ? TEXT("true") : TEXT("false"),
		bExecutionConsumedPendingVictimMontageEnd ? TEXT("true") : TEXT("false"),
		*ExecutionVictimRuntimeWeaponSubtypeTag.ToString());

	const bool bWasConsumedPendingVictimMontageEnd = bExecutionConsumedPendingVictimMontageEnd;
	const bool bWasWindowOpen = ExecutionWindowRuntimeState.IsWindowOpen();

	// victim montage end 只负责把“目标侧这段演出已经结束”这个事实交回窗口主链。
	// 若命中帧已把窗口标成 consumed_pending_close，就走正式消费关窗；
	// 否则一律按未消费自然结束处理，不在这里重新推导命中是否应该补算。
	FinalizeExecutionVictimRuntime(bInterrupted);
	EndExecutionRootMotionOverride(Montage, TEXT("ExecutionVictimEnded"));

	if (!bWasWindowOpen)
	{
		bExecutionConsumedPendingVictimMontageEnd = false;
		return;
	}

	if (bWasConsumedPendingVictimMontageEnd)
	{
		CloseExecutionWindow(true);
		return;
	}

	CloseExecutionWindow(false);
}

bool UExecutionWindowComponent::DescribeExecutionPresentationPreparationState(
	AActor* InstigatorActor,
	FString& OutDescription,
	FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	FString ContinueFailureReason;
	const bool bCanContinue = CanContinueExecutionPresentationPreparation(InstigatorActor, &ContinueFailureReason);
	const FString FailureReasonText = ContinueFailureReason.IsEmpty() ? TEXT("无") : ContinueFailureReason;
	OutDescription = FString::Printf(
		TEXT("PreparationActive=%s Ready=%s PreparationInstigator=%s WeaponSubtype=%s Elapsed=%.2f/%.2f CanContinue=%s Failure=%s"),
		bExecutionPresentationPreparationActive ? TEXT("是") : TEXT("否"),
		bExecutionPresentationPreparationReady ? TEXT("是") : TEXT("否"),
		*GetNameSafe(ExecutionPresentationPreparationInstigator.Get()),
		ExecutionPresentationPreparationWeaponSubtypeTag.IsValid()
			? *ExecutionPresentationPreparationWeaponSubtypeTag.ToString()
			: TEXT("None"),
		ExecutionPresentationPreparationElapsedSeconds,
		ExecutionPresentationPreparationDurationSeconds,
		bCanContinue ? TEXT("是") : TEXT("否"),
		*FailureReasonText);

	if (!bCanContinue && OutFailureReason)
	{
		*OutFailureReason = ContinueFailureReason;
	}

	return bCanContinue || bExecutionPresentationPreparationActive || bExecutionPresentationPreparationReady;
}

void UExecutionWindowComponent::NotifyExecutionRecoveryUnlockFrame()
{
	if (!bExecutionVictimRuntimeActive || bExecutionRecoveryUnlockFrameReceived)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ExecutionWindow] ExecutionRecoveryUnlock ignored owner=%s active=%s already_received=%s"),
			*GetNameSafe(GetOwner()),
			bExecutionVictimRuntimeActive ? TEXT("true") : TEXT("false"),
			bExecutionRecoveryUnlockFrameReceived ? TEXT("true") : TEXT("false"));
		return;
	}

	// ExecutionRecoveryUnlock 只解除目标侧 hard lock：
	// 1. 允许外部普通伤害 / 效果重新接管目标；
	// 2. 释放 victim runtime 持有的临时碰撞穿透保护；
	// 3. 但不在这里直接关窗，也不在这里恢复 Poise。
	bExecutionRecoveryUnlockFrameReceived = true;
	bExecutionVictimHardLockActive = false;
	RemoveExecutionVictimHardLockEffect();

	if (ExecutionRootMotionOverrideRuntime.bActive
		&& ExecutionRootMotionOverrideRuntime.Reason == TEXT("ExecutionVictim")
		&& ExecutionRootMotionOverrideRuntime.CapsulePawnPassThroughHandle.IsValid())
	{
		if (AActionCharacterBase* OwnerActionCharacter = Cast<AActionCharacterBase>(GetOwner()))
		{
			if (UActionCollisionRuntimeComponent* CollisionRuntimeComponent =
				OwnerActionCharacter->GetActionCollisionRuntimeComponent())
			{
				CollisionRuntimeComponent->ReleaseCollisionOverride(
					ExecutionRootMotionOverrideRuntime.CapsulePawnPassThroughHandle);
				ExecutionRootMotionOverrideRuntime.CapsulePawnPassThroughHandle.Reset();
			}
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[ExecutionWindow] ExecutionVictimHardLock unlock owner=%s instigator=%s weapon_subtype=%s reason=unlock_frame_received"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ExecutionVictimRuntimeInstigator.Get()),
		*ExecutionVictimRuntimeWeaponSubtypeTag.ToString());
}

void UExecutionWindowComponent::FinalizeExecutionVictimRuntime(bool bWasInterrupted)
{
	(void)bWasInterrupted;

	if (!bExecutionVictimRuntimeActive)
	{
		return;
	}

	// victim runtime 收尾只负责结束目标侧“被处决演出正在接管身体”的这一层运行态。
	// 是否关闭窗口、是否广播消费成功、是否恢复 Poise，仍由更上层窗口主链继续裁决。
	RemoveExecutionVictimHardLockEffect();
	ClearExecutionVictimRuntime();
}

bool UExecutionWindowComponent::ShouldBlockIncomingDamageOrEffect(AActor* InstigatorActor, bool bIsExecutionHit) const
{
	// 这是处决保护对外最核心的查询口：
	// 它不负责区分伤害类型、技能来源或具体效果内容，
	// 只回答“在当前锁定状态下，这次外部命中应不应该被挡掉”。
	// 查询入口也要能自愈失效引用。
	// 否则受击解析器在读取这一层保护时，会被“Tag 还在 / 弱引用已失效”的旧状态长期误导。
	if (ExecutionWindowRuntimeState.bExecutionVictimLocked
		&& !ExecutionWindowRuntimeState.ExecutionVictimLockInstigator.IsValid())
	{
		UExecutionWindowComponent* MutableThis = const_cast<UExecutionWindowComponent*>(this);
		MutableThis->ExecutionWindowRuntimeState.ClearExecutionVictimLock();
		MutableThis->RemoveExecutionVictimLockEffect();
	}

	// 当前执行者自己的正式处决命中必须能穿过目标硬锁。
	// 恢复硬锁只挡外部普通伤害 / 效果，不挡本次处决命中本身。
	if (bIsExecutionHit && ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor))
	{
		return false;
	}

	if (bExecutionVictimRuntimeActive && bExecutionVictimHardLockActive)
	{
		// 目标侧被处决蒙太奇的解除帧到达前，任何普通伤害或效果都不能继续接管目标。
		return true;
	}

	if (!ExecutionWindowRuntimeState.HasExecutionVictimLock())
	{
		// 没有受害者锁定时，说明目标当前不处于处决保护阶段，所有伤害 / 效果默认放行。
		return false;
	}

	// 其余所有外部命中，无论是普通伤害、DOT 还是额外状态效果，都视为对处决演出的干扰并拦截。
	// 也就是说，这里保护的不是某一种具体数值，而是“当前这段处决演出不允许被外部改写”的整体语义。
	return true;
}

void UExecutionWindowComponent::BroadcastExecutionEvent(FGameplayTag EventTag, AActor* InstigatorActor) const
{
	if (!EventTag.IsValid() || !GetOwner())
	{
		return;
	}

	// 处决相关链统一通过 GameplayEvent 广播，
	// 这样 Ability、角色组件与 UI 都可以监听同一事件，而不需要再各自维护一套回调协议。
	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = InstigatorActor;
	EventData.Target = GetOwner();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), EventTag, EventData);
}

const UActionAttributeSetBase* UExecutionWindowComponent::GetOwningAttributeSet() const
{
	// 执行组件不长期缓存属性集。
	// 原因是这里的读取频率不高，且保持按需读取更能避免拥有者重建后的悬空引用风险。
	if (const AActor* OwnerActor = GetOwner())
	{
		if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(OwnerActor))
		{
			if (UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface->GetAbilitySystemComponent())
			{
				// 执行组件默认要求拥有者把属性集注册在 ASC 上。
				// 因此这里只走标准 GAS 查询路径，不另外维护一份旁路属性缓存。
				return AbilitySystemComponent->GetSet<UActionAttributeSetBase>();
			}
		}
	}

	return nullptr;
}

UActionAbilitySystemComponent* UExecutionWindowComponent::GetOwningActionAbilitySystemComponent() const
{
	// 和属性集读取一样，这里也不额外缓存 ASC。
	// 原因是执行组件只在锁定效果施加 / 移除时使用 ASC，调用频率远低于战斗输入链，直接按需取即可。
	if (const AActor* OwnerActor = GetOwner())
	{
		if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(OwnerActor))
		{
			// 这里要求拿到的是项目自定义 ASC。
			// 因为锁定效果的施加、移除和状态展示，都依赖我们自己在 ASC 上扩展的辅助能力。
			return Cast<UActionAbilitySystemComponent>(AbilitySystemInterface->GetAbilitySystemComponent());
		}
	}

	return nullptr;
}

ACharacter* UExecutionWindowComponent::GetOwningCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

void UExecutionWindowComponent::ApplyExecutionVictimLockEffect()
{
	if (ExecutionVictimLockEffectHandle.IsValid())
	{
		// 锁定效果已经存在时直接视为成功。
		// 这样重复预占、重复校验或中间步骤幂等补调用时，不会重复叠加同一层保护效果。
		return;
	}

	UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent();
	if (!ActionAbilitySystemComponent)
	{
		// 没有 ASC 时，逻辑层锁定仍可能已成立；
		// 这里只是无法再把它额外投影成 GAS 可见状态。
		return;
	}

	// 这里把“受害者锁定”同步投影成 ASC 上的一条正式效果。
	// 这样即使未来有更多系统只认识 GAS 状态标签，也能无缝读到目标当前正处于处决受害者保护中。
	ExecutionVictimLockEffectHandle =
		ActionAbilitySystemComponent->ApplyExecutionProtectionEffect(ExecutionVictimLockCombatModifierEffect);
}

void UExecutionWindowComponent::RemoveExecutionVictimLockEffect()
{
	if (!ExecutionVictimLockEffectHandle.IsValid())
	{
		// 没有有效句柄时，说明当前没有可撤销的 GAS 侧锁定效果。
		return;
	}

	if (UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent())
	{
		// 只有在还能取到 ASC 时才真正尝试移除效果。
		// 若角色本身已进入销毁链，句柄依旧会在下面被作废，避免运行时残留“逻辑上还持有旧效果”的错觉。
		ActionAbilitySystemComponent->RemoveActiveGameplayEffect(ExecutionVictimLockEffectHandle);
	}

	// 无论 ASC 当前是否还存在，本地句柄都必须失效。
	// 这样后续重复清理或重复预占时，组件看到的都是“当前没有旧效果句柄残留”的干净状态。
	ExecutionVictimLockEffectHandle.Invalidate();
}

void UExecutionWindowComponent::ApplyExecutionVictimHardLockEffect()
{
	if (ExecutionVictimHardLockEffectHandle.IsValid())
	{
		return;
	}

	if (UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent())
	{
		ExecutionVictimHardLockEffectHandle =
			ActionAbilitySystemComponent->ApplyExecutionProtectionEffect(ExecutionVictimHardLockCombatModifierEffect);
	}
}

void UExecutionWindowComponent::RemoveExecutionVictimHardLockEffect()
{
	if (!ExecutionVictimHardLockEffectHandle.IsValid())
	{
		return;
	}

	if (UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent())
	{
		ActionAbilitySystemComponent->RemoveActiveGameplayEffect(ExecutionVictimHardLockEffectHandle);
	}

	ExecutionVictimHardLockEffectHandle.Invalidate();
}

void UExecutionWindowComponent::UpdateExecutionPresentationPreparation(const float DeltaTime)
{
	if (!bExecutionPresentationPreparationActive || bExecutionPresentationPreparationReady)
	{
		return;
	}

	AActor* InstigatorActor = ExecutionPresentationPreparationInstigator.Get();
	ACharacter* OwnerCharacter = GetOwningCharacter();
	if (!CanContinueExecutionPresentationPreparation(InstigatorActor) || !OwnerCharacter)
	{
		ClearExecutionPresentationPreparationRuntime();
		return;
	}

	FVector ToInstigator = InstigatorActor->GetActorLocation() - OwnerCharacter->GetActorLocation();
	ToInstigator.Z = 0.f;
	if (ToInstigator.IsNearlyZero())
	{
		bExecutionPresentationPreparationReady = true;
		bExecutionPresentationPreparationActive = false;
		return;
	}

	const FRotator DesiredRotation = FRotator(0.f, ToInstigator.GetSafeNormal().Rotation().Yaw, 0.f);
	const float SafeDuration = FMath::Max(ExecutionPresentationPreparationDurationSeconds, 0.f);
	if (SafeDuration <= KINDA_SMALL_NUMBER)
	{
		OwnerCharacter->SetActorRotation(DesiredRotation);
		bExecutionPresentationPreparationReady = true;
		bExecutionPresentationPreparationActive = false;
		return;
	}

	ExecutionPresentationPreparationElapsedSeconds = FMath::Min(
		ExecutionPresentationPreparationElapsedSeconds + FMath::Max(DeltaTime, 0.f),
		SafeDuration);
	const float RemainingTime = FMath::Max(SafeDuration - ExecutionPresentationPreparationElapsedSeconds, 0.f);
	const float CurrentYaw = OwnerCharacter->GetActorRotation().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredRotation.Yaw);

	if (RemainingTime <= KINDA_SMALL_NUMBER || FMath::Abs(DeltaYaw) <= 0.5f)
	{
		OwnerCharacter->SetActorRotation(DesiredRotation);
		bExecutionPresentationPreparationReady = true;
		bExecutionPresentationPreparationActive = false;
		return;
	}

	const float StepYaw = DeltaYaw * FMath::Clamp(FMath::Max(DeltaTime, 0.f) / RemainingTime, 0.f, 1.f);
	OwnerCharacter->SetActorRotation(FRotator(0.f, CurrentYaw + StepYaw, 0.f));
}

bool UExecutionWindowComponent::CanContinueExecutionPresentationPreparation(AActor* InstigatorActor, FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!IsValid(InstigatorActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("目标侧处决前准备的执行者引用已失效。");
		}
		return false;
	}

	if (!ExecutionWindowRuntimeState.IsWindowOpen())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("目标 %s 的处决窗口当前已关闭。"), *GetNameSafe(GetOwner()));
		}
		return false;
	}

	if (!ExecutionWindowRuntimeState.IsReservedBy(InstigatorActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 当前不再由执行者 %s 预占。"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(InstigatorActor));
		}
		return false;
	}

	if (!ExecutionWindowRuntimeState.IsExecutionVictimLockedBy(InstigatorActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 的 victim lock 当前不再归执行者 %s 所有。"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(InstigatorActor));
		}
		return false;
	}

	if (!HasVictimExecutionPresentationForWeaponSubtype(ExecutionPresentationPreparationWeaponSubtypeTag))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 当前 WeaponSubtypeTag=%s 的 victim 动画配置已失效。"),
				*GetNameSafe(GetOwner()),
				ExecutionPresentationPreparationWeaponSubtypeTag.IsValid()
					? *ExecutionPresentationPreparationWeaponSubtypeTag.ToString()
					: TEXT("None"));
		}
		return false;
	}

	return true;
}

void UExecutionWindowComponent::ClearExecutionPresentationPreparationRuntime()
{
	bExecutionPresentationPreparationActive = false;
	bExecutionPresentationPreparationReady = false;
	ExecutionPresentationPreparationInstigator.Reset();
	ExecutionPresentationPreparationWeaponSubtypeTag = FGameplayTag();
	ExecutionPresentationPreparationElapsedSeconds = 0.f;
	ExecutionPresentationPreparationDurationSeconds = 0.f;
}

const FActionExecutionVictimAnimConfig* UExecutionWindowComponent::FindVictimExecutionAnimConfig(
	const FGameplayTag& WeaponSubtypeTag) const
{
	return WeaponSubtypeTag.IsValid() ? VictimExecutionAnimConfigs.Find(WeaponSubtypeTag) : nullptr;
}

bool UExecutionWindowComponent::HasExecutionRecoveryUnlockNotify(const UAnimMontage* RecoveryMontage) const
{
	if (!RecoveryMontage)
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : RecoveryMontage->Notifies)
	{
		if (NotifyEvent.Notify
			&& NotifyEvent.Notify->IsA(UAnimNotify_ExecutionRecoveryUnlock::StaticClass()))
		{
			return true;
		}
	}

	return false;
}

void UExecutionWindowComponent::RestorePoiseToMaxForExecution(const FName Reason, const bool bOnlyIfAlive)
{
	const UActionAttributeSetBase* AttributeSet = GetOwningAttributeSet();
	UActionAbilitySystemComponent* ActionAbilitySystemComponent = GetOwningActionAbilitySystemComponent();
	if (!AttributeSet || !ActionAbilitySystemComponent)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ExecutionWindow] PoiseRestore owner=%s reason=%s alive=false poise_before=0.00 max_poise=0.00 delta=0.00 poise_after=0.00 used_asc_path=false failure=%s"),
			*GetNameSafe(GetOwner()),
			*Reason.ToString(),
			AttributeSet ? TEXT("missing_ability_system") : TEXT("missing_attribute_set"));
		return;
	}

	const bool bOwnerAlive = AttributeSet->IsAlive();
	const float PoiseBefore = AttributeSet->GetPoise();
	const float MaxPoise = AttributeSet->GetMaxPoise();
	if (bOnlyIfAlive && !bOwnerAlive)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[ExecutionWindow] PoiseRestore skipped owner=%s reason=%s alive=false poise_before=%.2f max_poise=%.2f delta=0.00 poise_after=%.2f used_asc_path=false failure=target_dead"),
			*GetNameSafe(GetOwner()),
			*Reason.ToString(),
			PoiseBefore,
			MaxPoise,
			PoiseBefore);
		return;
	}

	const float DeltaPoise = FMath::Max(MaxPoise - PoiseBefore, 0.f);
	if (DeltaPoise > KINDA_SMALL_NUMBER)
	{
		ActionAbilitySystemComponent->ApplyModToAttributeUnsafe(
			UActionAttributeSetBase::GetPoiseAttribute(),
			EGameplayModOp::Additive,
			DeltaPoise);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[ExecutionWindow] PoiseRestore owner=%s reason=%s alive=%s poise_before=%.2f max_poise=%.2f delta=%.2f poise_after=%.2f used_asc_path=true"),
		*GetNameSafe(GetOwner()),
		*Reason.ToString(),
		bOwnerAlive ? TEXT("true") : TEXT("false"),
		PoiseBefore,
		MaxPoise,
		DeltaPoise,
		AttributeSet->GetPoise());
}

bool UExecutionWindowComponent::BeginExecutionRootMotionOverride(UAnimMontage* Montage, const FName Reason)
{
	ACharacter* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter || !Montage)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ExecutionWindow] RootMotionOverride begin failed owner=%s character=%s montage=%s reason=%s failure=invalid_input"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(OwnerCharacter),
			*GetNameSafe(Montage),
			*Reason.ToString());
		return false;
	}

	USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ExecutionWindow] RootMotionOverride begin failed owner=%s character=%s montage=%s reason=%s failure=missing_anim_instance"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(OwnerCharacter),
			*GetNameSafe(Montage),
			*Reason.ToString());
		return false;
	}

	if (ExecutionRootMotionOverrideRuntime.bActive)
	{
		EndExecutionRootMotionOverride(
			ExecutionRootMotionOverrideRuntime.Montage.Get(),
			ExecutionRootMotionOverrideRuntime.Reason);
	}

	ExecutionRootMotionOverrideRuntime.Character = OwnerCharacter;
	ExecutionRootMotionOverrideRuntime.Montage = Montage;
	ExecutionRootMotionOverrideRuntime.PreviousRootMotionMode = AnimInstance->RootMotionMode;
	ExecutionRootMotionOverrideRuntime.Reason = Reason;
	ExecutionRootMotionOverrideRuntime.bActive = true;
	AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);

	if (Reason == TEXT("ExecutionVictim"))
	{
		if (AActionCharacterBase* OwnerActionCharacter = Cast<AActionCharacterBase>(GetOwner()))
		{
			if (UActionCollisionRuntimeComponent* CollisionRuntimeComponent =
				OwnerActionCharacter->GetActionCollisionRuntimeComponent())
			{
				FActionCollisionOverrideRequest CollisionOverrideRequest;
				CollisionOverrideRequest.Slot = EActionCollisionSlot::CharacterCapsule;
				CollisionOverrideRequest.Preset = EActionCollisionPreset::ExecutionVictimPawnPassThrough;
				CollisionOverrideRequest.Priority = 200;
				CollisionOverrideRequest.OwnerReason = TEXT("ExecutionVictimRootMotion");
				CollisionOverrideRequest.TargetRegistrationNames.Add(TEXT("CharacterCapsule"));
				ExecutionRootMotionOverrideRuntime.CapsulePawnPassThroughHandle =
					CollisionRuntimeComponent->AcquireCollisionOverride(CollisionOverrideRequest);
			}
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[ExecutionWindow] RootMotionOverride begin owner=%s character=%s montage=%s reason=%s previous_root_motion_mode=%s new_root_motion_mode=%s montage_has_root_motion=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(OwnerCharacter),
		*GetNameSafe(Montage),
		*Reason.ToString(),
		LexToStringExecutionRootMotionMode(ExecutionRootMotionOverrideRuntime.PreviousRootMotionMode),
		LexToStringExecutionRootMotionMode(AnimInstance->RootMotionMode),
		Montage->HasRootMotion() ? TEXT("true") : TEXT("false"));

	if (!Montage->HasRootMotion())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ExecutionWindow] RootMotionOverride asset warning owner=%s montage=%s reason=%s failure=montage_has_no_root_motion check=EnableRootMotion/RootBone/Retarget"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Montage),
			*Reason.ToString());
	}

	return true;
}

void UExecutionWindowComponent::EndExecutionRootMotionOverride(UAnimMontage* Montage, const FName Reason)
{
	if (!ExecutionRootMotionOverrideRuntime.bActive)
	{
		return;
	}

	UAnimMontage* RuntimeMontage = ExecutionRootMotionOverrideRuntime.Montage.Get();
	if (Montage && RuntimeMontage && Montage != RuntimeMontage)
	{
		return;
	}

	if (ACharacter* RuntimeCharacter = ExecutionRootMotionOverrideRuntime.Character.Get())
	{
		if (ExecutionRootMotionOverrideRuntime.CapsulePawnPassThroughHandle.IsValid())
		{
			if (AActionCharacterBase* OwnerActionCharacter = Cast<AActionCharacterBase>(GetOwner()))
			{
				if (UActionCollisionRuntimeComponent* CollisionRuntimeComponent =
					OwnerActionCharacter->GetActionCollisionRuntimeComponent())
				{
					CollisionRuntimeComponent->ReleaseCollisionOverride(
						ExecutionRootMotionOverrideRuntime.CapsulePawnPassThroughHandle);
				}
			}
		}

		if (USkeletalMeshComponent* MeshComponent = RuntimeCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
			{
				const ERootMotionMode::Type CurrentRootMotionMode = AnimInstance->RootMotionMode;
				AnimInstance->SetRootMotionMode(ExecutionRootMotionOverrideRuntime.PreviousRootMotionMode);
				UE_LOG(
					LogTemp,
					Log,
					TEXT("[ExecutionWindow] RootMotionOverride end owner=%s character=%s montage=%s reason=%s previous_root_motion_mode=%s restored_root_motion_mode=%s"),
					*GetNameSafe(GetOwner()),
					*GetNameSafe(RuntimeCharacter),
					*GetNameSafe(RuntimeMontage),
					*Reason.ToString(),
					LexToStringExecutionRootMotionMode(CurrentRootMotionMode),
					LexToStringExecutionRootMotionMode(AnimInstance->RootMotionMode));
			}
		}
	}

	ExecutionRootMotionOverrideRuntime.Reset();
}

void UExecutionWindowComponent::ClearExecutionVictimRuntime()
{
	bExecutionVictimRuntimeActive = false;
	bExecutionVictimHardLockActive = false;
	bExecutionRecoveryUnlockFrameReceived = false;
	ExecutionVictimRuntimeInstigator.Reset();
	ExecutionVictimRuntimeWeaponSubtypeTag = FGameplayTag();
	ExecutionVictimHardLockEffectHandle.Invalidate();
}

void UExecutionWindowComponent::RestartExecutionWindowTimer()
{
	if (!GetWorld() || !ExecutionWindowRuntimeState.IsWindowOpen())
	{
		return;
	}

	// 每次重启都先清旧计时器，保证同一窗口在任意时刻都只存在一个正式超时来源。
	// 这能避免旧计时器晚一帧触发，把已经被刷新过的窗口再次误关掉。
	GetWorld()->GetTimerManager().ClearTimer(ExecutionWindowRuntimeState.ExecutionWindowTimerHandle);
	// 这里显式使用当前配置的固定持续时长重开一次性计时器，
	// 让“窗口刷新”语义始终等价于“从现在开始重新给满一轮处决输入时间”。
	GetWorld()->GetTimerManager().SetTimer(
		ExecutionWindowRuntimeState.ExecutionWindowTimerHandle,
		this,
		&UExecutionWindowComponent::OnExecutionWindowExpired,
		ExecutionWindowDuration,
		false);
}

void UExecutionWindowComponent::OnExecutionWindowExpired()
{
	// 超时关闭统一按“未消费窗口”处理：
	// 1. 会广播窗口关闭事件；
	// 2. 若配置允许，会恢复目标韧性；
	// 3. 不会触发正式处决事件链。
	CloseExecutionWindow(false);
}
