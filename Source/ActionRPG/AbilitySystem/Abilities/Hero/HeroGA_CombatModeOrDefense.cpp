// 文件说明：实现“进入战斗姿态 / 防御”共用 Gameplay Ability 的运行时逻辑。

#include "AbilitySystem/Abilities/Hero/HeroGA_CombatModeOrDefense.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "ActionGameplayTags.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroGA_CombatModeOrDefense, Log, All);

UHeroGA_CombatModeOrDefense::UHeroGA_CombatModeOrDefense()
	: Super()
{
	ActivationPolicy = EActionAbilityActivationPolicy::OnInput;

	// 在原生构造函数里固定声明 AbilityTag，避免蓝图侧遗漏配置。
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_CombatModeOrDefense);
	ActivationOwnedTags.AddTag(ActionGameplayTags::State_Ability_Defense_Active);
	CombatReactAbilityRule.bAllowActivationDuringRecoveryCancelWindow = true;

	// 防御期间的临时战斗状态也统一落到这份持续修正效果里。
	// 这样格挡期标签、受击过滤和状态查询都能共享同一份生命周期来源，
	// 不需要再在 GA 里手动维护一组分散的临时标签。
	DefenseCombatModifierEffect.Duration = 10.f;
	DefenseCombatModifierEffect.StatusEffectTag = ActionGameplayTags::StatusEffect_Combat_Defense;
	DefenseCombatModifierEffect.GrantedTags.AddTag(ActionGameplayTags::State_Combat_Defense_Active);
}

bool UHeroGA_CombatModeOrDefense::ValidateRelationshipActivationPreconditions(FString& OutFailureReason)
{
	// 关系预检阶段只补充“这条主链现在有没有资格被放行”的判断。
	// 这里不真正写姿态或防御运行态，避免预检失败时留下半套战斗副作用。
	if (!ValidateHeroRuntimeObjects(OutFailureReason, false, true))
	{
		return false;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCombatComponent)
	{
		// 没有战斗组件时，这条 GA 连“当前到底处于 Idle、Combo 还是其它姿态”都无从判断。
		OutFailureReason = TEXT("hero combat component is invalid");
		return false;
	}

	UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo();
	if (!HeroDefenseComponent)
	{
		OutFailureReason = TEXT("hero defense component is invalid");
		return false;
	}

	if (!HeroDefenseComponent->CanEnterRelationshipActivationForNonAttackInput(
		ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense,
		&OutFailureReason))
	{
		// 防御 / 姿态链当前只在关系裁决前保留共享硬门禁。
		// 面对活跃主动 GA 时是否允许抢入，统一交给 ASC 关系矩阵处理。
		// 这里不再把旧 non-attack input gate 当成最终关系结论。
		return false;
	}

	const EHeroCombatMode CurrentCombatMode = HeroCombatComponent->GetCombatMode();
	if (CurrentCombatMode == EHeroCombatMode::Idle)
	{
		// Idle 下触发这条 GA 时，不要求防御资源，因为这次输入只会把角色切进普通战斗姿态。
		// 也就是说，这里放行的是“姿态切换资格”，不是“防御资格”。
		return true;
	}

	if (CurrentCombatMode != EHeroCombatMode::Combo)
	{
		// 这条能力只支持两种语义：
		// 1. Idle -> Combo 的姿态切入；
		// 2. Combo -> Defense 的正式防御。
		// 其它姿态一律不解释为合法激活上下文。
		OutFailureReason = TEXT("combat mode or defense cannot activate in current combat mode");
		return false;
	}

	// 只有已经在普通战斗姿态里，才会把这次输入解释成“进入防御”。
	// 因此也只有这条路径需要提前校验防御蒙太奇是否存在。
	if (!HeroDefenseComponent->GetDefenseAnimMontage())
	{
		OutFailureReason = TEXT("defense montage is missing");
		return false;
	}

	return true;
}

void UHeroGA_CombatModeOrDefense::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 这批字段都只服务“本次激活”的局部时序。
	// 正式防御状态仍在组件侧；GA 本地只缓存分流结果、释放来源和收尾是否已执行等短生命周期信息。
	// 每次激活前都先清空防御运行时标记，避免上一次防御中断后的残留状态串到新流程。
	// 同一输入在不同姿态下会走不同语义，因此这里必须保证防御侧标记绝对是“这次输入新鲜算出的结果”。
	bDefenseStateStarted = false;
	bDefenseAbilityFinished = false;
	bDefenseEndedByInputRelease = false;
	bDefenseReleaseWatchActive = false;

	AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo();
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCharacter || !HeroCombatComponent)
	{
		Debug::Print(TEXT("[GA][CombatModeOrDefense] Begin Failed: Hero/CombatComponent 无效"), FColor::Red, 2.0f);
		UE_LOG(LogHeroGA_CombatModeOrDefense, Warning, TEXT("战斗姿态/防御能力激活失败：角色或战斗组件为空。"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const EHeroCombatMode CurrentCombatMode = HeroCombatComponent->GetCombatMode();
	Debug::Print(
		FString::Printf(
			TEXT("[GA][CombatModeOrDefense] Begin CombatMode=%d"),
			static_cast<int32>(CurrentCombatMode)),
		FColor::Cyan,
		2.0f);

	if (CurrentCombatMode == EHeroCombatMode::Idle)
	{
		// Idle 状态下按下该输入，只负责发起“进入普通战斗姿态”正式过渡。
		// 进入战斗蒙太奇、武器从 Holster 切到 WeaponSocket 的精确帧，以及异常收尾，
		// 统一交给 HeroCombatComponent 维护，不再由主 ABP 轮询或这条 GA 局部状态并行表达。
		HeroCombatComponent->TryEnterCombatModeFromIdle();
		// 这条分支本质上是一次“轻量姿态切换”，不是状态型防御 Ability。
		// 所以不需要让 Ability 持续存活，姿态写进去后立即结束即可。
		Debug::Print(TEXT("[GA][CombatModeOrDefense] Enter Combo From Idle"), FColor::Green, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (CurrentCombatMode == EHeroCombatMode::Combo)
	{
		// 已经处于普通战斗姿态时，同一输入才会真正进入防御流程。
		// 也只有这条路径才会进入“按住持续、松手退出”的状态型 Ability 生命周期。
		DefenseLogic();
		return;
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][CombatModeOrDefense] Ignored CombatMode=%d"),
			static_cast<int32>(CurrentCombatMode)),
		FColor::Yellow,
		2.0f);
	K2_EndAbility();
}

void UHeroGA_CombatModeOrDefense::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 这条 GA 既可能被输入释放正常结束，也可能被更高优先级行为或外部系统强制结束。
	// 因此这里必须做兜底收口，不能只依赖释放监听或蒙太奇回调。
	// 防御 GA 可能被外部系统直接结束，不能只依赖输入释放或蒙太奇回调。
	if (!bDefenseAbilityFinished && bDefenseStateStarted)
	{
		if (!bDefenseEndedByInputRelease)
		{
			if (UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo())
			{
				// 被外部强制结束后，要求玩家先松开防御键，再允许下一次重新进入防御。
				HeroDefenseComponent->RequireDefenseReleaseBeforeReactivation();
			}
		}

		FinalizeDefenseAbility();
	}

	StopDefenseReleaseWatch();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHeroGA_CombatModeOrDefense::HandleDefenseInputReleased(float HeldTime)
{
	(void)HeldTime;
	Debug::Print(TEXT("[GA][CombatModeOrDefense] Input Released"), FColor::Yellow, 2.0f);
	bDefenseEndedByInputRelease = true;

	// 真正退出防御的主入口是“输入释放”，而不是防御蒙太奇自然播完。
	// 这样“按住持续、松手退出”的正式语义就固定在输入层，而不是表现层动画长度上。
	FinishDefenseAbility(true);
}

void UHeroGA_CombatModeOrDefense::DefenseLogic()
{
	// 进入这里说明这次输入已经在 Combo 态下被解释成“正式防御”。
	// 后续流程要先写防御运行态，再附加临时战斗修正，再挂上释放监听与兜底轮询。
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCombatComponent || !GetHeroCharacterFromActorInfo())
	{
		Debug::Print(TEXT("[GA][CombatModeOrDefense] Defense Failed: Hero/CombatComponent 无效"), FColor::Red, 2.0f);
		K2_EndAbility();
		return;
	}

	UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo();
	UAnimMontage* DefenseMontage = HeroDefenseComponent ? HeroDefenseComponent->GetDefenseAnimMontage() : nullptr;
	if (!DefenseMontage)
	{
		Debug::Print(TEXT("[GA][CombatModeOrDefense] Defense Failed: DefenseMontage 为空"), FColor::Red, 2.0f);
		const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = HeroCombatComponent->GetCurrentWeaponDefinition();
		UE_LOG(
			LogHeroGA_CombatModeOrDefense,
			Warning,
			TEXT("防御动画为空：请检查当前武器的 DefenseMontage 是否已配置并成功加载。WeaponTag=%s CombatMode=%d"),
			CurrentWeaponDefinition ? *CurrentWeaponDefinition->WeaponTag.ToString() : TEXT("None"),
			static_cast<int32>(HeroCombatComponent->GetCombatMode()));
		K2_EndAbility();
		return;
	}

	bDefenseStateStarted = true;
	// 从这里开始，Ability 已经真正进入“防御态由自己维护”的阶段。
	// 后续无论是输入释放、蒙太奇中断还是外部强制结束，都必须走统一收尾。
	if (HeroDefenseComponent)
	{
		// 防御链公共运行态统一收口到防御组件。
		HeroDefenseComponent->ApplyDefenseAbilityStarted(DefenseMontage);
	}
	// 防御期的临时战斗修饰效果放在进入防御时统一附加，
	// 这样格挡判定、受击过滤或后续需要的防御期标签都能跟防御生命周期自然同步。
	// 这也意味着后续如果要扩展“防御期间减伤、霸体、特殊格挡状态”，优先改数据而不是继续加 GA 分支。
	// 先写正式防御运行态，再附加临时效果，可以保证状态查询看到的是已经完整起手的防御语义。
	ApplyHeroCombatModifierEffect(DefenseCombatModifierEffect);

	Debug::Print(TEXT("[GA][CombatModeOrDefense] Defense Begin"), FColor::Cyan, 2.0f);

	if (!PlayHeroMontage(DefenseMontage, TEXT("PlayDefenseMontageAndWait"), TEXT("Defense")))
	{
		Debug::Print(TEXT("[GA][CombatModeOrDefense] Defense Failed: 蒙太奇播放失败"), FColor::Red, 2.0f);
		UE_LOG(LogHeroGA_CombatModeOrDefense, Warning, TEXT("防御蒙太奇播放失败。Montage=%s"), *GetNameSafe(DefenseMontage));
		FinishDefenseAbility(true);
		return;
	}

	UAbilityTask_WaitInputRelease* InputReleasedEvent = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	if (!InputReleasedEvent)
	{
		Debug::Print(TEXT("[GA][CombatModeOrDefense] Defense Failed: WaitInputRelease 创建失败"), FColor::Red, 2.0f);
		// 防御态已经正式进入后，如果释放监听任务没建起来，必须立刻主动收尾。
		// 否则角色会停留在“已经进入防御，但永远等不到释放回调”的坏状态里。
		FinishDefenseAbility(true);
		return;
	}

	// 防御不是“一段动画播完就结束”的技能，而是“按住持续、松手退出”的状态型能力。
	// 因此输入释放监听比蒙太奇结束更关键。
	// 蒙太奇在这里只负责表现层起手，真正决定何时退出的是输入层。
	// WaitInputRelease 是主释放通道；后面的 StartDefenseReleaseWatch() 只负责兜极端时序漏事件。
	// 这里绑定的是本次激活的 AbilityTask 回调桥，不是新的正式输入状态源；
	// 是否真的处于按住、缓冲或 Held 回放阶段，仍继续回到 HeroCombatInputComponent。
	InputReleasedEvent->OnRelease.AddDynamic(this, &UHeroGA_CombatModeOrDefense::HandleDefenseInputReleased);
	InputReleasedEvent->Activate();
	StartDefenseReleaseWatch();
}

void UHeroGA_CombatModeOrDefense::FinishDefenseAbility(bool bShouldEndAbility)
{
	if (bDefenseAbilityFinished)
	{
		return;
	}

	// 先统一做正式状态回收，再决定是否立刻结束 Ability 对象。
	// 这样外部如果只想退出防御态而暂时不销毁能力，也能复用同一套底层收尾。
	// 先收状态、再决定是否 K2_EndAbility，可以避免蓝图或外部结束顺序把防御留成半套。
	FinalizeDefenseAbility();

	if (!bDefenseEndedByInputRelease)
	{
		if (UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo())
		{
			// 非释放路径退出防御时，阻止 Held 重放立刻把角色重新拉回防御态。
			HeroDefenseComponent->RequireDefenseReleaseBeforeReactivation();
		}
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][CombatModeOrDefense] Defense End ShouldEnd=%d"),
			bShouldEndAbility ? 1 : 0),
		FColor::Green,
		2.0f);

	if (!bShouldEndAbility)
	{
		// 某些路径可能只想先退出防御态，再把 Ability 对象保留到更外层流程统一结束。
		// 因此这里把“状态收尾”和“是否立刻结束能力”显式拆成两层。
		return;
	}

	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		// 防御状态结束后再请求恢复战斗输入，避免和受击重置态或闪避态收尾互相打架。
		if (!HeroCombatComponent->IsCombatReactStateResetInProgress()
			&& !HeroCombatComponent->IsDodgeActive())
		{
			// 这里只是安排“下一帧恢复输入处理”，不是当场重放输入。
			// 这样能保证防御窗口、受击窗口和其它高优先级状态都先退干净。
			// 也就是说 RequestRecoverCombatInputAfterDefense() 只负责恢复时序，不负责重新判定或重放输入。
			if (UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo())
			{
				HeroDefenseComponent->RequestRecoverCombatInputAfterDefense();
			}
		}
	}

	K2_EndAbility();
}

void UHeroGA_CombatModeOrDefense::FinalizeDefenseAbility()
{
	if (bDefenseAbilityFinished || !bDefenseStateStarted)
	{
		// 只有真正进入过防御态，才需要做防御收尾。
		// 单纯 Idle -> Combo 的那条输入分流不应该误落到这里。
		return;
	}

	bDefenseAbilityFinished = true;
	StopDefenseReleaseWatch();
	// 这里才是防御运行态正式收口点。
	// 只有真正进入过防御态的激活，才应该落到这里；Idle -> Combo 的输入分流不会参与这套收尾。

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	const bool bCombatReactResetInProgress =
		HeroCombatComponent && HeroCombatComponent->IsCombatReactStateResetInProgress();

	if (UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo())
	{
		// 防御链公共运行态收尾交回防御组件统一处理。
		// 受击重置若正在接管最终状态，这里就把那份接管语义一起传下去，避免双重收尾互相覆盖。
		HeroDefenseComponent->FinalizeDefenseAbilityRuntime(bCombatReactResetInProgress);
	}
}

void UHeroGA_CombatModeOrDefense::StartDefenseReleaseWatch()
{
	if (bDefenseReleaseWatchActive || !GetWorld())
	{
		return;
	}

	// 防御释放除了依赖 GAS 的 WaitInputRelease 外，再补一层运行时输入状态兜底。
	// 这样即使极端时序下释放事件漏掉，只要输入状态表已经显示按键不再按住，也会主动结束防御。
	// 这层轮询只兜“漏掉释放事件”的坏时序，不负责新建防御资格判断，也不替代正式输入系统。
	bDefenseReleaseWatchActive = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::HandleDeferredDefenseReleaseWatch);
}

void UHeroGA_CombatModeOrDefense::StopDefenseReleaseWatch()
{
	// 停止兜底轮询本身不等于结束防御，只表示后续不再继续检查“是否漏掉释放事件”。
	bDefenseReleaseWatchActive = false;
}

void UHeroGA_CombatModeOrDefense::HandleDeferredDefenseReleaseWatch()
{
	if (!bDefenseReleaseWatchActive || bDefenseAbilityFinished || !bDefenseStateStarted || !GetWorld())
	{
		return;
	}

	if (!IsDefenseInputStillHeld())
	{
		Debug::Print(TEXT("[GA][CombatModeOrDefense] Release Watch Triggered"), FColor::Yellow, 2.0f);
		// 兜底轮询一旦确认已经没按住，就回到和正常释放完全相同的退出主入口。
		// 它只补“事件丢了”的缺口，不单独发明第二套退出语义。
		HandleDefenseInputReleased(0.f);
		return;
	}

	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::HandleDeferredDefenseReleaseWatch);
}

bool UHeroGA_CombatModeOrDefense::IsDefenseInputStillHeld() const
{
	// 这里只读取正式输入组件里的按住态，供释放兜底轮询使用。
	// 它不是一份新的输入状态源，也不会绕过关系系统重新决定是否允许进入防御。
	const AActionHeroCharacter* HeroCharacter =
		CurrentActorInfo ? Cast<AActionHeroCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const UHeroCombatInputComponent* HeroCombatInputComponent =
		HeroCharacter ? HeroCharacter->GetHeroCombatInputComponent() : nullptr;
	if (!HeroCombatInputComponent)
	{
		return false;
	}

	return HeroCombatInputComponent->IsCombatInputPressedOrHeld(
		ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense);
}

void UHeroGA_CombatModeOrDefense::OnHeroMontageCompleted(FName MontageContext)
{
	(void)MontageContext;
	// 防御蒙太奇结束后，玩家仍可能保持按住防御，因此这里不主动结束 Ability。
	// 否则会出现动画播完就自动掉盾，而不是由输入释放控制退出。
	// 防御不像 Attack 有“过渡播完后继续推进执行”的双层语义；这里的动画结束只影响表现，不直接决定退出。
	Debug::Print(TEXT("[GA][CombatModeOrDefense] Montage Completed"), FColor::Yellow, 2.0f);
}

void UHeroGA_CombatModeOrDefense::OnHeroMontageBlendOut(FName MontageContext)
{
	(void)MontageContext;
	// 防御蒙太奇结束后，玩家仍可能保持按住防御，因此这里不主动结束 Ability。
	// 只要输入释放监听还没到，这条防御链就仍然应该视为持续中。
	Debug::Print(TEXT("[GA][CombatModeOrDefense] Montage BlendOut"), FColor::Yellow, 2.0f);
}

void UHeroGA_CombatModeOrDefense::OnHeroMontageInterrupted(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][CombatModeOrDefense] Montage Interrupted"), FColor::Red, 2.0f);
	// 被打断说明这段防御表现已经无法继续维持，必须立刻退出防御态。
	FinishDefenseAbility(true);
}

void UHeroGA_CombatModeOrDefense::OnHeroMontageCancelled(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][CombatModeOrDefense] Montage Cancelled"), FColor::Red, 2.0f);
	// Cancelled 与 Interrupted 一样，都代表防御的持续前提已失效，因此统一强制收尾。
	FinishDefenseAbility(true);
}
