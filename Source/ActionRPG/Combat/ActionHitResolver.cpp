// 文件说明：实现单人版受击解析器相关逻辑。

#include "Combat/ActionHitResolver.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "AbilitySystem/Effects/ActionGE_DamageOverTime.h"
#include "AbilitySystem/Effects/ActionGE_HitDamage.h"
#include "ActionGameplayTags.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Debug/ActionDebugHelper.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatFeedbackComponent.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Execution/ExecutionWindowComponent.h"
#include "DataAssets/Effects/DataAsset_ActionHitEffectDefinition.h"
#include "Interfaces/Combat/ActionCombatReactInterface.h"
#include "Items/Projectiles/ActionProjectileBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionHitResolver, Log, All);

namespace
{
	int32 ResolveInstigatorAbilityLevel(const FActionDamagePayload& DamagePayload)
	{
		const AActionHeroCharacter* InstigatorHero =
			Cast<AActionHeroCharacter>(DamagePayload.InstigatorActor);
		const UHeroCombatComponent* HeroCombatComponent =
			InstigatorHero ? InstigatorHero->GetHeroCombatComponent() : nullptr;
		if (!HeroCombatComponent)
		{
			return 1;
		}

		FActionDamageContextRuntimeState DamageContext;
		return HeroCombatComponent->TryGetActiveDamageContext(DamageContext)
			? FMath::Max(DamageContext.AbilityLevel, 1)
			: 1;
	}

	UHeroCombatFeedbackComponent* ResolveCombatFeedbackComponent(const FActionDamagePayload& DamagePayload)
	{
		const AActionHeroCharacter* InstigatorHero =
			Cast<AActionHeroCharacter>(DamagePayload.InstigatorActor);
		return InstigatorHero ? InstigatorHero->GetHeroCombatFeedbackComponent() : nullptr;
	}

	FVector ResolveCombatFeedbackImpactLocation(
		AActor* TargetActor,
		const FActionDamagePayload& DamagePayload)
	{
		if (IsValid(TargetActor))
		{
			return TargetActor->GetActorLocation();
		}

		if (IsValid(DamagePayload.SourceActor))
		{
			return DamagePayload.SourceActor->GetActorLocation();
		}

		if (IsValid(DamagePayload.InstigatorActor))
		{
			return DamagePayload.InstigatorActor->GetActorLocation();
		}

		return FVector::ZeroVector;
	}

	FVector ResolveCombatFeedbackImpactNormal(
		AActor* TargetActor,
		const FActionDamagePayload& DamagePayload)
	{
		if (!DamagePayload.ImpactDirection.IsNearlyZero())
		{
			return (-DamagePayload.ImpactDirection).GetSafeNormal();
		}

		if (IsValid(TargetActor) && IsValid(DamagePayload.SourceActor))
		{
			return (DamagePayload.SourceActor->GetActorLocation() - TargetActor->GetActorLocation())
				.GetSafeNormal();
		}

		return FVector::ZeroVector;
	}

	void ConfigureCombatFeedbackPresentationFlags(
		const FActionHitPresentationConfig& HitPresentationConfig,
		const FActionHitResolveResult& ResolveResult,
		FActionCombatFeedbackEvent& OutCombatFeedbackEvent)
	{
		switch (ResolveResult.ResultType)
		{
		case EActionHitResultType::Damaged:
			OutCombatFeedbackEvent.bShouldShowDamageNumber =
				HitPresentationConfig.bShowDamageNumber && ResolveResult.AppliedDamage > 0.f;
			OutCombatFeedbackEvent.bShouldPlayImpactEffect = HitPresentationConfig.bPlayImpactEffect;
			OutCombatFeedbackEvent.bShouldPlayImpactSound = HitPresentationConfig.bPlayImpactSound;
			break;

		case EActionHitResultType::Blocked:
		case EActionHitResultType::GuardBroken:
			OutCombatFeedbackEvent.bShouldShowDamageNumber = false;
			OutCombatFeedbackEvent.bShouldPlayImpactEffect = HitPresentationConfig.bPlayImpactEffect;
			OutCombatFeedbackEvent.bShouldPlayImpactSound = HitPresentationConfig.bPlayImpactSound;
			break;

		case EActionHitResultType::Parried:
		case EActionHitResultType::PerfectDodged:
		case EActionHitResultType::Ignored:
		case EActionHitResultType::None:
		default:
			OutCombatFeedbackEvent.bShouldShowDamageNumber = false;
			OutCombatFeedbackEvent.bShouldPlayImpactEffect = false;
			OutCombatFeedbackEvent.bShouldPlayImpactSound = false;
			break;
		}
	}

	void FillProjectileFeedbackSnapshot(
		const FActionDamagePayload& DamagePayload,
		FActionCombatFeedbackEvent& OutCombatFeedbackEvent)
	{
		if (DamagePayload.HitSource.SourceType != EActionHitSourceType::Projectile)
		{
			return;
		}

		OutCombatFeedbackEvent.ProjectileTag = DamagePayload.HitSource.SourceTag;
		if (const AActionProjectileBase* ProjectileActor =
			Cast<AActionProjectileBase>(DamagePayload.SourceActor))
		{
			const FActionProjectilePresentationEvent ProjectileSnapshot =
				ProjectileActor->GetLastPresentationEvent();
			OutCombatFeedbackEvent.ProjectileTag = ProjectileSnapshot.ProjectileTag;
			OutCombatFeedbackEvent.ResolvedConfigSource = ProjectileSnapshot.ResolvedConfigSource;
			OutCombatFeedbackEvent.SelectedProjectileConfigTag =
				ProjectileSnapshot.SelectedProjectileConfigTag;
			OutCombatFeedbackEvent.SpawnSocketName = ProjectileSnapshot.SpawnSocketName;
		}
	}

	void BroadcastCombatFeedbackEvent(
		AActor* TargetActor,
		const FActionDamagePayload& DamagePayload,
		const FActionHitResolveResult& ResolveResult)
	{
		UHeroCombatFeedbackComponent* CombatFeedbackComponent =
			ResolveCombatFeedbackComponent(DamagePayload);
		if (!CombatFeedbackComponent)
		{
			return;
		}

		FActionCombatFeedbackEvent CombatFeedbackEvent;
		CombatFeedbackEvent.EventType =
			ResolveResult.ResultType == EActionHitResultType::Ignored
				? EActionCombatFeedbackEventType::HitIgnored
				: EActionCombatFeedbackEventType::HitResolved;
		CombatFeedbackEvent.HitResultType = ResolveResult.ResultType;
		CombatFeedbackEvent.InstigatorActor = DamagePayload.InstigatorActor;
		CombatFeedbackEvent.SourceActor = DamagePayload.SourceActor;
		CombatFeedbackEvent.TargetActor = TargetActor;
		CombatFeedbackEvent.ImpactLocation =
			ResolveCombatFeedbackImpactLocation(TargetActor, DamagePayload);
		CombatFeedbackEvent.ImpactNormal =
			ResolveCombatFeedbackImpactNormal(TargetActor, DamagePayload);
		CombatFeedbackEvent.BaseDamage = ResolveResult.AppliedDamage;
		CombatFeedbackEvent.GuardStaminaCost = ResolveResult.AppliedGuardStaminaCost;
		CombatFeedbackEvent.PoiseDamage = ResolveResult.AppliedPoiseDamage;
		CombatFeedbackEvent.DamageType = DamagePayload.DamageType;
		CombatFeedbackEvent.DamageElementTypeTag = DamagePayload.DamageElementTypeTag;
		CombatFeedbackEvent.HitTag = DamagePayload.HitTag;
		CombatFeedbackEvent.HitSource = DamagePayload.HitSource;
		CombatFeedbackEvent.DamageNumberStyleTag =
			DamagePayload.HitPresentationConfig.DamageNumberStyleTag;
		CombatFeedbackEvent.Effect = DamagePayload.HitPresentationConfig.ImpactEffect;
		CombatFeedbackEvent.Sound = DamagePayload.HitPresentationConfig.ImpactSound;
		FillProjectileFeedbackSnapshot(DamagePayload, CombatFeedbackEvent);
		ConfigureCombatFeedbackPresentationFlags(
			DamagePayload.HitPresentationConfig,
			ResolveResult,
			CombatFeedbackEvent);

		CombatFeedbackComponent->HandleCombatFeedbackEvent(CombatFeedbackEvent);
	}

	/** 统一判断目标当前是否处于“处决期间无敌”状态。*/
	bool HasExecutionInvulnerability(const UAbilitySystemComponent* TargetAbilitySystemComponent)
	{
		return TargetAbilitySystemComponent
			&& TargetAbilitySystemComponent->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionInvulnerable);
	}

	/** 统一判断目标当前是否已经被锁定为“处决受害者”。*/
	bool HasExecutionVictimLock(const UAbilitySystemComponent* TargetAbilitySystemComponent)
	{
		return TargetAbilitySystemComponent
			&& TargetAbilitySystemComponent->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionVictimLocked);
	}

	/** 统一读取目标当前是否处于“超级护甲可挡受击表现”的状态。*/
	bool HasSuperArmor(const UAbilitySystemComponent* TargetAbilitySystemComponent)
	{
		return TargetAbilitySystemComponent
			&& TargetAbilitySystemComponent->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_SuperArmor_Active);
	}

	/** 从目标 Actor 上解析处决窗口组件。*/
	UExecutionWindowComponent* GetExecutionWindowComponentFromActor(const AActor* TargetActor)
	{
		return TargetActor ? TargetActor->FindComponentByClass<UExecutionWindowComponent>() : nullptr;
	}

	/**
	 * 统一判断当前这次伤害/效果是否应被“处决受害者锁定”拦截。
	 * 规则：
	 * 1. 若目标未被锁定，则直接放行；
	 * 2. 若目标已被锁定，则普通外部伤害/效果一律拦截；
	 * 3. 只有锁定当前目标的执行者打出的“正式处决命中”允许穿过这层保护。
	 */
	bool IsBlockedByExecutionVictimLock(
		AActor* TargetActor,
		const UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FActionDamagePayload& DamagePayload,
		const bool bLogDiagnostics = true)
	{
		const bool bHasVictimLockTag = HasExecutionVictimLock(TargetAbilitySystemComponent);
		if (const UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromActor(TargetActor))
		{
			const bool bLockedByPayloadInstigator =
				ExecutionWindowComponent->IsExecutionVictimLockedBy(DamagePayload.InstigatorActor);
			if (DamagePayload.bTreatAsExecutionHit && bLockedByPayloadInstigator)
			{
				if (bLogDiagnostics)
				{
					UE_LOG(
						LogActionHitResolver,
						Warning,
						TEXT("[GA][Execution][VictimLock] reason=execution_victim_lock_allows_current_execution_hit target=%s instigator=%s source=%s treat_as_execution=true has_component=true has_victim_lock_tag=%s locked_by_instigator=true lock_owner=%s"),
						*GetNameSafe(TargetActor),
						*GetNameSafe(DamagePayload.InstigatorActor),
						*GetNameSafe(DamagePayload.SourceActor),
						bHasVictimLockTag ? TEXT("true") : TEXT("false"),
						*GetNameSafe(ExecutionWindowComponent->GetExecutionVictimLockInstigator()));
				}
				return false;
			}

			const bool bBlockedByComponent = ExecutionWindowComponent->ShouldBlockIncomingDamageOrEffect(
				DamagePayload.InstigatorActor,
				DamagePayload.bTreatAsExecutionHit);

			if (bLogDiagnostics && (DamagePayload.bTreatAsExecutionHit || bBlockedByComponent))
			{
				UE_LOG(
					LogActionHitResolver,
					Warning,
					TEXT("[GA][Execution][VictimLock] reason=%s target=%s instigator=%s source=%s treat_as_execution=%s has_component=true has_victim_lock_tag=%s locked_by_instigator=%s lock_owner=%s"),
					bBlockedByComponent
						? (DamagePayload.bTreatAsExecutionHit ? TEXT("blocked_by_other_instigator") : TEXT("blocked_by_component_runtime"))
						: TEXT("execution_victim_lock_component_allows"),
					*GetNameSafe(TargetActor),
					*GetNameSafe(DamagePayload.InstigatorActor),
					*GetNameSafe(DamagePayload.SourceActor),
					DamagePayload.bTreatAsExecutionHit ? TEXT("true") : TEXT("false"),
					bHasVictimLockTag ? TEXT("true") : TEXT("false"),
					bLockedByPayloadInstigator ? TEXT("true") : TEXT("false"),
					*GetNameSafe(ExecutionWindowComponent->GetExecutionVictimLockInstigator()));
			}

			// 目标存在正式窗口组件时，以组件运行态为准。
			// 旧逻辑在组件放行后继续落到 Tag-only fallback，导致正式处决命中被自身 VictimLock Tag 错拦。
			return bBlockedByComponent;
		}

		if (!bHasVictimLockTag)
		{
			// 没有受害者锁时，说明目标当前不在处决保护阶段，这条拦截规则直接不生效。
			return false;
		}

		// 若极端情况下目标身上残留了锁定 Tag，但组件已经失效，
		// 则宁可保守拦截，也不要放过外部干扰。
		if (bLogDiagnostics)
		{
			UE_LOG(
				LogActionHitResolver,
				Warning,
				TEXT("[GA][Execution][VictimLock] reason=blocked_by_tag_only_fallback target=%s instigator=%s source=%s treat_as_execution=%s has_component=false has_victim_lock_tag=true"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(DamagePayload.InstigatorActor),
				*GetNameSafe(DamagePayload.SourceActor),
				DamagePayload.bTreatAsExecutionHit ? TEXT("true") : TEXT("false"));
		}
		return true;
	}

	/** 统一广播一次战斗反应事件，并在目标实现接口时继续走角色级入口。*/
	void BroadcastCombatReactEvent(
		AActor* TargetActor,
		const FGameplayTag EventTag,
		const FActionDamagePayload& DamagePayload)
	{
		if (!IsValid(TargetActor) || !EventTag.IsValid())
		{
			return;
		}

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = DamagePayload.InstigatorActor;
		EventData.Target = TargetActor;
		EventData.OptionalObject = DamagePayload.SourceActor;
		EventData.InstigatorTags.AddTag(DamagePayload.HitTag);
		if (DamagePayload.HitSource.SourceTag.IsValid())
		{
			EventData.InstigatorTags.AddTag(DamagePayload.HitSource.SourceTag);
		}
		if (DamagePayload.HitSource.ParentSourceTag.IsValid())
		{
			EventData.InstigatorTags.AddTag(DamagePayload.HitSource.ParentSourceTag);
		}
		if (DamagePayload.HitSource.WeaponTag.IsValid())
		{
			EventData.InstigatorTags.AddTag(DamagePayload.HitSource.WeaponTag);
		}

		// 这里先发一条标准 GameplayEvent，
		// 让 GA、UI、状态机等“只认事件流”的系统先拿到统一语义。
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, EventTag, EventData);

		if (TargetActor->GetClass()->ImplementsInterface(UActionCombatReactInterface::StaticClass()))
		{
			// 若目标还实现了角色级受击接口，则继续把同一事件下沉到角色自己的受击表现层。
			// 这样 GameplayEvent 与角色内建战斗反应不会彼此割裂，而是共用同一次结算结果。
			IActionCombatReactInterface::Execute_HandleCombatReactEvent(TargetActor, EventTag, DamagePayload);
		}
	}

	/** 只向目标广播一次 GameplayEvent，不再重复进入整条受击表现链。*/
	void BroadcastGameplayEventOnly(
		AActor* TargetActor,
		const FGameplayTag EventTag,
		const FActionDamagePayload& DamagePayload)
	{
		if (!IsValid(TargetActor) || !EventTag.IsValid())
		{
			return;
		}

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = DamagePayload.InstigatorActor;
		EventData.Target = TargetActor;
		EventData.OptionalObject = DamagePayload.SourceActor;
		EventData.InstigatorTags.AddTag(DamagePayload.HitTag);
		if (DamagePayload.HitSource.SourceTag.IsValid())
		{
			EventData.InstigatorTags.AddTag(DamagePayload.HitSource.SourceTag);
		}
		if (DamagePayload.HitSource.ParentSourceTag.IsValid())
		{
			EventData.InstigatorTags.AddTag(DamagePayload.HitSource.ParentSourceTag);
		}
		if (DamagePayload.HitSource.WeaponTag.IsValid())
		{
			EventData.InstigatorTags.AddTag(DamagePayload.HitSource.WeaponTag);
		}

		// 这条路径刻意只发事件，不进入角色级受击接口。
		// 它适用于“需要额外通知系统，但不该重复触发整条受击表现链”的场景。
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, EventTag, EventData);
	}

	/** 破韧后若最终受击表现不是 PoiseBreak，也要单独补处决窗口事件。*/
	void NotifyPoiseBreakExecutionWindow(
		AActor* TargetActor,
		const FGameplayTag FinalReactEventTag,
		const FActionDamagePayload& DamagePayload)
	{
		if (!IsValid(TargetActor))
		{
			return;
		}

		if (FinalReactEventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
		{
			// 如果这次最终广播的正式受击事件本来就是 PoiseBreak，
			// 那么处决窗口组件稍后会在同一条事件链里自然收到它，这里就不需要重复补一次。
			return;
		}

		// 有些情况下最终表现层事件并不是 PoiseBreak，
		// 但从数值上看这次命中确实把韧性削空了。
		// 这时需要单独补一条 PoiseBreak 语义，专门让处决窗口系统接住这次“可处决”时机。
		// 这样“最终受击表现是什么”和“处决窗口是否应该打开”就不会被强行绑死成同一件事。
		BroadcastGameplayEventOnly(TargetActor, ActionGameplayTags::Combat_Event_PoiseBreak, DamagePayload);

		if (UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromActor(TargetActor))
		{
			ExecutionWindowComponent->HandleCombatReactEvent(
				ActionGameplayTags::Combat_Event_PoiseBreak,
				DamagePayload.InstigatorActor);
		}
	}

	bool TryResolveInstigatorCurrentLoadoutContext(
		const FActionDamagePayload& DamagePayload,
		const UHeroLoadoutContextComponent*& OutLoadoutContextComponent,
		EHeroWeaponLoadoutSlot& OutLoadoutSlot)
	{
		OutLoadoutContextComponent = nullptr;
		OutLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

		const AActionHeroCharacter* InstigatorHeroCharacter =
			Cast<AActionHeroCharacter>(DamagePayload.InstigatorActor);
		const UHeroEquipmentComponent* EquipmentComponent = InstigatorHeroCharacter
			? InstigatorHeroCharacter->FindComponentByClass<UHeroEquipmentComponent>()
			: nullptr;
		OutLoadoutContextComponent = InstigatorHeroCharacter
			? InstigatorHeroCharacter->FindComponentByClass<UHeroLoadoutContextComponent>()
			: nullptr;
		if (!EquipmentComponent || !OutLoadoutContextComponent)
		{
			return false;
		}

		OutLoadoutSlot = EquipmentComponent->GetCurrentEquippedLoadoutSlot();
		return true;
	}

	/** 解析本次命中的“最终处决倍率”。非处决命中默认返回 1。*/
	float ResolveExecutionDamageMultiplier(const FActionDamagePayload& DamagePayload)
	{
		if (!DamagePayload.bTreatAsExecutionHit)
		{
			// 普通命中不参与处决伤害倍率，默认按 1 倍处理。
			return 1.f;
		}

		if (DamagePayload.ExecutionDamageMultiplierOverride > 0.f)
		{
			return DamagePayload.ExecutionDamageMultiplierOverride;
		}

		const AActionCharacterBase* InstigatorCharacter = Cast<AActionCharacterBase>(DamagePayload.InstigatorActor);
		const UActionAttributeSetBase* InstigatorAttributeSet =
			InstigatorCharacter ? InstigatorCharacter->GetActionAttributeSet() : nullptr;
		if (!InstigatorAttributeSet)
		{
			return 1.f;
		}

		// 没有显式覆盖时，优先读取攻击者基础处决倍率，
		// 再叠加当前装备槽缓存中的处决倍率加成。
		float ResolvedExecutionDamageMultiplier = FMath::Max(InstigatorAttributeSet->GetExecutionDamageMultiplier(), 0.f);
		const UHeroLoadoutContextComponent* LoadoutContextComponent = nullptr;
		EHeroWeaponLoadoutSlot CurrentLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
		if (TryResolveInstigatorCurrentLoadoutContext(
			DamagePayload,
			LoadoutContextComponent,
			CurrentLoadoutSlot))
		{
			FActionWeaponAttributeCacheData WeaponAttributeCache;
			if (LoadoutContextComponent->GetWeaponAttributeCacheByLoadoutSlot(
				CurrentLoadoutSlot,
				WeaponAttributeCache))
			{
				ResolvedExecutionDamageMultiplier += WeaponAttributeCache.ExecutionDamageMultiplierBonus;
			}
		}

		return FMath::Max(ResolvedExecutionDamageMultiplier, 0.f);
	}

	/** 构建一条紧凑的命中来源调试文本。*/
	FString DescribeHitSource(const FActionDamagePayload& DamagePayload)
	{
		return FString::Printf(
			TEXT("HitSourceType=%d SourceTag=%s ParentSourceTag=%s WeaponTag=%s LoadoutSlot=%d SourceComponent=%s"),
			static_cast<int32>(DamagePayload.HitSource.SourceType),
			*DamagePayload.HitSource.SourceTag.ToString(),
			*DamagePayload.HitSource.ParentSourceTag.ToString(),
			*DamagePayload.HitSource.WeaponTag.ToString(),
			static_cast<int32>(DamagePayload.HitSource.LoadoutSlot),
			*DamagePayload.HitSource.SourceComponentName.ToString());
	}

	FString DescribeHitResultType(const EActionHitResultType ResultType)
	{
		switch (ResultType)
		{
		case EActionHitResultType::None:
			return TEXT("None");
		case EActionHitResultType::Damaged:
			return TEXT("Damaged");
		case EActionHitResultType::Blocked:
			return TEXT("Blocked");
		case EActionHitResultType::GuardBroken:
			return TEXT("GuardBroken");
		case EActionHitResultType::Parried:
			return TEXT("Parried");
		case EActionHitResultType::PerfectDodged:
			return TEXT("PerfectDodged");
		case EActionHitResultType::Ignored:
			return TEXT("Ignored");
		default:
			return TEXT("Unknown");
		}
	}

	FString DescribeHitReactType(const EActionHitReactType HitReactType)
	{
		switch (HitReactType)
		{
		case EActionHitReactType::None:
			return TEXT("None");
		case EActionHitReactType::HitStun:
			return TEXT("HitStun");
		case EActionHitReactType::HeavyHitReact:
			return TEXT("HeavyHitReact");
		case EActionHitReactType::Launch:
			return TEXT("Launch");
		case EActionHitReactType::Knockdown:
			return TEXT("Knockdown");
		default:
			return TEXT("Unknown");
		}
	}

	FString DescribeExecutionTargetTags(const UAbilitySystemComponent* TargetAbilitySystemComponent)
	{
		if (!TargetAbilitySystemComponent)
		{
			return TEXT("target_tags=<missing_asc>");
		}

		FGameplayTagContainer OwnedTags;
		TargetAbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
		return FString::Printf(
			TEXT("target_tags=[%s] has_victim_lock=%s has_execution_invulnerable=%s has_recovery_hard_lock=%s"),
			*OwnedTags.ToStringSimple(),
			TargetAbilitySystemComponent->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionVictimLocked) ? TEXT("true") : TEXT("false"),
			TargetAbilitySystemComponent->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionInvulnerable) ? TEXT("true") : TEXT("false"),
			TargetAbilitySystemComponent->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionVictim_HardLock) ? TEXT("true") : TEXT("false"));
	}

	/**
	 * 根据“伤害载荷意图 + 这次真实结算结果”推导最终应该广播的受击事件。
	 * 这里之所以不只看 `DamagePayload.HitReactType`，是因为：
	 * 1. 破韧是否真的发生，要等数值结算后才能确认；
	 * 2. 超级护甲是否拦截受击表现，也要结合当前目标状态一起判断。
	 */
	FGameplayTag ResolveDamageReactEventTag(
		const FActionDamagePayload& DamagePayload,
		const FActionHitResolveResult& ResolveResult,
		const bool bHasSuperArmor)
	{
		// 这里做的是“最终受击表现优先级裁决”，而不是单纯把输入枚举翻译成 Tag。
		// 顺序上：
		// 1. 先看超级护甲是否整体挡掉表现；
		// 2. 再看输入里显式声明的高优先级表现，如击倒、击飞；
		// 3. 最后才根据真实结算结果决定是否升级成破韧或普通受击。
		if (bHasSuperArmor)
		{
			return FGameplayTag();
		}

		if (DamagePayload.HitReactType == EActionHitReactType::Knockdown)
		{
			return ActionGameplayTags::Combat_Event_Knockdown;
		}

		if (DamagePayload.HitReactType == EActionHitReactType::Launch)
		{
			return ActionGameplayTags::Combat_Event_Launch;
		}

		if (ResolveResult.bPoiseBroken)
		{
			// 真实破韧一旦成立，就在这里把最终事件升级为 PoiseBreak。
			// 这一步依赖真实结算结果，因此必须放在数值结算之后而不是只看输入载荷。
			return ActionGameplayTags::Combat_Event_PoiseBreak;
		}

		if (DamagePayload.HitReactType == EActionHitReactType::HeavyHitReact)
		{
			return ActionGameplayTags::Combat_Event_HeavyHitReact;
		}

		if (DamagePayload.HitReactType == EActionHitReactType::HitStun)
		{
			return ActionGameplayTags::Combat_Event_HitReact;
		}

		return FGameplayTag();
	}

	/** 把最终广播的战斗事件重新映射回结果结构里的“正式受击类型”。*/
	EActionHitReactType ResolveAppliedHitReactType(const FGameplayTag& EventTag)
	{
		if (EventTag == ActionGameplayTags::Combat_Event_HitReact)
		{
			// 普通受击事件最终仍以 HitStun 语义回写给 ResolveResult，
			// 这样上层在读取“这次命中最终触发了什么受击类型”时，
			// 不需要再反查一遍 GameplayTag 才能知道该走普通硬直分支。
			return EActionHitReactType::HitStun;
		}

		if (EventTag == ActionGameplayTags::Combat_Event_HeavyHitReact)
		{
			return EActionHitReactType::HeavyHitReact;
		}

		if (EventTag == ActionGameplayTags::Combat_Event_Launch)
		{
			// 击飞与击倒这类高阶表现，统一在这里从最终事件标签回落到结果枚举。
			// 这样 ResolveResult 同时保留“系统内部的枚举语义”和“对外广播的事件语义”两层结果。
			return EActionHitReactType::Launch;
		}

		if (EventTag == ActionGameplayTags::Combat_Event_Knockdown)
		{
			return EActionHitReactType::Knockdown;
		}

		if (EventTag == ActionGameplayTags::Combat_Event_PoiseBreak)
		{
			// 失衡本身仍然属于“受击打断类表现”，因此结果类型继续回写成 HitStun。
			// 处决窗口是否额外开启，则由上层单独通过 PoiseBreak 事件去处理。
			return EActionHitReactType::HitStun;
		}

		return EActionHitReactType::None;
	}

	/** 构建本次 GE 结算所需的通用上下文。*/
	FGameplayEffectContextHandle BuildEffectContext(
		UAbilitySystemComponent* SpecOwnerAbilitySystemComponent,
		const FActionDamagePayload& DamagePayload)
	{
		// 命中解析器当前只往上下文里写最核心的来源关系：
		// Instigator 代表真正的攻击发起者，SourceObject 代表这次命中的来源载体。
		// 后续若要扩展伤害数字、命中特效或更复杂来源语义，可以在这层继续追加上下文字段。
		FGameplayEffectContextHandle EffectContext = SpecOwnerAbilitySystemComponent->MakeEffectContext();
		EffectContext.AddInstigator(DamagePayload.InstigatorActor, DamagePayload.SourceActor);
		EffectContext.AddSourceObject(DamagePayload.SourceActor);
		// 当前上下文里还没有塞入自定义 GEContext 或额外命中参数，
		// 因此这里保持最小来源关系即可。
		// 后续如果要接伤害数字样式、命中特效路由或更复杂来源判定，
		// 这条上下文构建函数就是继续加字段的统一入口。
		return EffectContext;
	}

	/** 给运行时 GE Spec 写入状态效果标签，方便 UI 与 Gameplay 共用同一份语义。*/
	void AddStatusEffectTagsToSpec(FGameplayEffectSpec& EffectSpec, const FGameplayTag StatusEffectTag)
	{
		if (!StatusEffectTag.IsValid())
		{
			return;
		}

		// 同一份状态语义同时写入 DynamicAssetTag 和 DynamicGrantedTag：
		// 1. AssetTag 方便外部从效果定义语义角度识别“这是一份什么状态”；
		// 2. GrantedTag 方便运行时标签查询直接看到目标当前正挂着什么状态。
		EffectSpec.AddDynamicAssetTag(ActionGameplayTags::StatusEffect);
		EffectSpec.AddDynamicAssetTag(StatusEffectTag);

		EffectSpec.DynamicGrantedTags.AddTag(ActionGameplayTags::StatusEffect);
		EffectSpec.DynamicGrantedTags.AddTag(StatusEffectTag);
	}

	/** 通过 GE 把一次命中的原始伤害写入目标 ASC，后续由 AttributeSet 完成统一公式结算。*/
	bool ApplyHitDamageGameplayEffect(
		AActor* TargetActor,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FActionDamagePayload& DamagePayload,
		const float HealthDamage,
		const float GuardStaminaCost,
		const float PoiseDamage,
		const float ExecutionDamageMultiplier,
		FString* OutFailureReason = nullptr)
	{
		if (!IsValid(TargetActor) || !TargetAbilitySystemComponent)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("execution_target_or_asc_invalid");
			}
			return false;
		}

		// 这里是“即时伤害 / 体力 / 韧性”正式进入 GAS 结算的唯一落点。
		// 在这一步之前，所有逻辑都还只是资格检查和链路分流；
		// 走到这里后，才算真正把一份命中意图写进 ASC / AttributeSet。

		// 执行者处于处决无敌时，所有通过受击解析器进入的即时伤害与削韧都应被直接拦截。
		if (HasExecutionInvulnerability(TargetAbilitySystemComponent))
		{
			// 处决执行者无敌时，这次即时命中直接不进入后续 GE 结算链。
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("execution_blocked_by_invulnerability");
			}
			return false;
		}

		// 处决受害者锁定期间，只有真正的处决命中允许穿过。
		if (IsBlockedByExecutionVictimLock(TargetActor, TargetAbilitySystemComponent, DamagePayload, false))
		{
			// 被处决受害者锁拦下时，这次命中连正式 GE 都不该创建。
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("execution_blocked_by_victim_lock");
			}
			return false;
		}

		// 三种正式数值全为 0 时没有必要创建 Spec，避免无意义 GE 开销。
		if (HealthDamage <= 0.f
			&& GuardStaminaCost <= 0.f
			&& PoiseDamage <= 0.f)
		{
			// 没有任何数值需要写入时，直接返回失败，让上层决定这次命中是否等价于 Ignored。
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("execution_no_numeric_result");
			}
			return false;
		}

		// 优先使用攻击方 ASC 作为 Spec 构建方，为后续来源属性/来源标签参与计算预留扩展点。
		UAbilitySystemComponent* SourceAbilitySystemComponent =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DamagePayload.InstigatorActor);
		UAbilitySystemComponent* SpecOwnerAbilitySystemComponent =
			SourceAbilitySystemComponent ? SourceAbilitySystemComponent : TargetAbilitySystemComponent;
		if (!SpecOwnerAbilitySystemComponent)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("execution_spec_build_failed");
			}
			return false;
		}

		FGameplayEffectSpecHandle EffectSpecHandle =
			SpecOwnerAbilitySystemComponent->MakeOutgoingSpec(
				UActionGE_HitDamage::StaticClass(),
				1.f,
				BuildEffectContext(SpecOwnerAbilitySystemComponent, DamagePayload));
		if (!EffectSpecHandle.IsValid() || !EffectSpecHandle.Data.IsValid())
		{
			// 连伤害 Spec 都没构出来时，这次即时伤害视为未真正进入 GAS 结算。
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("execution_spec_build_failed");
			}
			return false;
		}

		// 解析器只负责把“原始伤害意图”送进 GE，
		// 真正的伤害公式依旧完全收口在 AttributeSet 里。
		// 这也是为什么这里写入的是 SetByCaller 原始值，而不是已经减完抗性/防御后的最终结果：
		// 受击解析器负责“提交本次命中打算造成什么”，
		// AttributeSet 负责“目标在当前属性状态下真正吃到多少”。
		EffectSpecHandle.Data->SetSetByCallerMagnitude(ActionGameplayTags::SetByCaller_Damage_Health, ActionRoundPositiveBaseValueToInteger(HealthDamage));
		EffectSpecHandle.Data->SetSetByCallerMagnitude(ActionGameplayTags::SetByCaller_Cost_GuardStamina, ActionRoundPositiveBaseValueToInteger(GuardStaminaCost));
		EffectSpecHandle.Data->SetSetByCallerMagnitude(ActionGameplayTags::SetByCaller_Damage_Poise, ActionRoundPositiveBaseValueToInteger(PoiseDamage));
		EffectSpecHandle.Data->SetSetByCallerMagnitude(
			ActionGameplayTags::SetByCaller_Damage_ExecutionMultiplier,
			FMath::Max(ActionRoundRatioValueToFourPlaces(ExecutionDamageMultiplier), 0.f));
		EffectSpecHandle.Data->SetSetByCallerMagnitude(
			ActionGameplayTags::SetByCaller_Damage_IgnoreFinalDamageReductionPercent,
			ActionRoundRatioValueToFourPlaces(DamagePayload.DamageType == EActionDamageType::Elemental ? 50.f : 0.f));

		if (SourceAbilitySystemComponent)
		{
			// 优先走“来源 ASC 对目标 ASC”这条应用路径，
			// 这样后续若有来源属性、来源标签或来源侧修正，也更符合 GAS 的常规伤害关系。
			SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetAbilitySystemComponent);
		}
		else
		{
			// 没有来源 ASC 时，退化为目标自己对自己应用。
			// 这能保证环境伤害、脚本伤害等没有完整攻击者的命中也能继续复用同一条 GE 结算链。
			TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
		}

		// Instant GE 应用后通常不会留下有效 ActiveGameplayEffectHandle。
		// 因此这里的 true 只表示 Spec 已提交给 GAS；真实伤害是否落地必须看属性差值。
		return true;
	}

	bool TryResolveHitEffectApplicationTarget(
		AActor* PrimaryTargetActor,
		const FActionDamagePayload& DamagePayload,
		const EActionHitEffectTargetSide TargetSide,
		AActor*& OutResolvedActor,
		UAbilitySystemComponent*& OutResolvedAbilitySystemComponent)
	{
		OutResolvedActor = nullptr;
		OutResolvedAbilitySystemComponent = nullptr;

		AActor* ResolvedActor =
			TargetSide == EActionHitEffectTargetSide::Target
				? PrimaryTargetActor
				: DamagePayload.InstigatorActor.Get();
		if (!IsValid(ResolvedActor))
		{
			return false;
		}

		UAbilitySystemComponent* ResolvedAbilitySystemComponent =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ResolvedActor);
		if (!ResolvedAbilitySystemComponent)
		{
			return false;
		}

		OutResolvedActor = ResolvedActor;
		OutResolvedAbilitySystemComponent = ResolvedAbilitySystemComponent;
		return true;
	}

	bool CanApplyHitEffectToActor(
		AActor* TargetActor,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FActionDamagePayload& DamagePayload)
	{
		if (!IsValid(TargetActor) || !TargetAbilitySystemComponent)
		{
			return false;
		}

		if (HasExecutionInvulnerability(TargetAbilitySystemComponent))
		{
			return false;
		}

		return !IsBlockedByExecutionVictimLock(TargetActor, TargetAbilitySystemComponent, DamagePayload);
	}

	bool TryBuildInvalidHitEffectReason(
		const FActionHitEffectEntry& EffectEntry,
		FString& OutFailureReason)
	{
		OutFailureReason.Reset();

		if (!EffectEntry.EffectDefinition)
		{
			OutFailureReason = TEXT("EffectDefinition 为空。");
			return true;
		}

		if (!EffectEntry.EffectDefinition->IsValidDefinition())
		{
			OutFailureReason = TEXT("EffectDefinition 未通过自身合法性校验。");
			return true;
		}

		if (EffectEntry.EffectDefinition->GetEffectKind() != EffectEntry.EffectKind)
		{
			OutFailureReason = FString::Printf(
				TEXT("条目小类与资产小类不匹配。EntryKind=%d AssetKind=%d。"),
				static_cast<int32>(EffectEntry.EffectKind),
				static_cast<int32>(EffectEntry.EffectDefinition->GetEffectKind()));
			return true;
		}

		switch (EffectEntry.EffectKind)
		{
		case EActionHitEffectKind::Dot:
			if (!Cast<UDataAsset_ActionHitDotEffectDefinition>(EffectEntry.EffectDefinition))
			{
				OutFailureReason = TEXT("Dot 条目没有引用 UDataAsset_ActionHitDotEffectDefinition。");
				return true;
			}
			break;

		case EActionHitEffectKind::Buff:
			if (!Cast<UDataAsset_ActionHitBuffEffectDefinition>(EffectEntry.EffectDefinition))
			{
				OutFailureReason = TEXT("Buff 条目没有引用 UDataAsset_ActionHitBuffEffectDefinition。");
				return true;
			}
			break;

		case EActionHitEffectKind::Debuff:
			if (!Cast<UDataAsset_ActionHitDebuffEffectDefinition>(EffectEntry.EffectDefinition))
			{
				OutFailureReason = TEXT("Debuff 条目没有引用 UDataAsset_ActionHitDebuffEffectDefinition。");
				return true;
			}
			break;

		default:
			break;
		}

		return false;
	}

	bool ApplyDamageOverTimeDefinition(
		AActor* TargetActor,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FActionDamagePayload& DamagePayload,
		const UDataAsset_ActionHitDotEffectDefinition& DotEffectDefinition)
	{
		if (!CanApplyHitEffectToActor(TargetActor, TargetAbilitySystemComponent, DamagePayload))
		{
			return false;
		}

		UAbilitySystemComponent* SourceAbilitySystemComponent =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DamagePayload.InstigatorActor);
		UAbilitySystemComponent* SpecOwnerAbilitySystemComponent =
			SourceAbilitySystemComponent ? SourceAbilitySystemComponent : TargetAbilitySystemComponent;
		if (!SpecOwnerAbilitySystemComponent)
		{
			return false;
		}

		const int32 AbilityLevel = ResolveInstigatorAbilityLevel(DamagePayload);
		const float ResolvedDurationSeconds =
			DotEffectDefinition.ResolveDurationSecondsForLevel(AbilityLevel);
		const float ResolvedDamagePerTick =
			DotEffectDefinition.ResolveDamagePerTickForLevel(AbilityLevel);
		const float ResolvedPeriodSeconds =
			DotEffectDefinition.ResolvePeriodSecondsForLevel(AbilityLevel);
		if (ResolvedDurationSeconds <= 0.f
			|| ResolvedDamagePerTick <= 0.f
			|| ResolvedPeriodSeconds <= 0.f)
		{
			return false;
		}

		FGameplayEffectSpecHandle EffectSpecHandle =
			SpecOwnerAbilitySystemComponent->MakeOutgoingSpec(
				UActionGE_DamageOverTime::StaticClass(),
				1.f,
				BuildEffectContext(SpecOwnerAbilitySystemComponent, DamagePayload));
		if (!EffectSpecHandle.IsValid() || !EffectSpecHandle.Data.IsValid())
		{
			return false;
		}

		FGameplayEffectSpec& GameplayEffectSpec = *EffectSpecHandle.Data.Get();
		GameplayEffectSpec.SetSetByCallerMagnitude(
			ActionGameplayTags::SetByCaller_Effect_Duration,
			ResolvedDurationSeconds);
		GameplayEffectSpec.SetSetByCallerMagnitude(
			ActionGameplayTags::SetByCaller_Damage_Health,
			-ActionRoundPositiveBaseValueToInteger(ResolvedDamagePerTick));
		GameplayEffectSpec.Period = FMath::Max(ResolvedPeriodSeconds, KINDA_SMALL_NUMBER);
		AddStatusEffectTagsToSpec(GameplayEffectSpec, DotEffectDefinition.GetStatusEffectTag());
		GameplayEffectSpec.DynamicGrantedTags.AppendTags(DotEffectDefinition.GrantedTags);

		if (SourceAbilitySystemComponent)
		{
			const FActiveGameplayEffectHandle ActiveEffectHandle =
				SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
					GameplayEffectSpec,
					TargetAbilitySystemComponent);
			return ActiveEffectHandle.IsValid();
		}

		const FActiveGameplayEffectHandle ActiveEffectHandle =
			TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(GameplayEffectSpec);
		return ActiveEffectHandle.IsValid();
	}

	bool ApplyBuffOrDebuffDefinition(
		AActor* TargetActor,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FActionDamagePayload& DamagePayload,
		const UDataAsset_ActionHitEffectDefinition& EffectDefinition)
	{
		if (!CanApplyHitEffectToActor(TargetActor, TargetAbilitySystemComponent, DamagePayload))
		{
			return false;
		}

		UActionAbilitySystemComponent* ActionAbilitySystemComponent =
			Cast<UActionAbilitySystemComponent>(TargetAbilitySystemComponent);
		if (!ActionAbilitySystemComponent)
		{
			return false;
		}

		FActionCombatModifierEffectSpec EffectSpec;
		EffectSpec.Duration = FMath::Max(EffectDefinition.DurationSeconds, 0.f);
		EffectSpec.StatusEffectTag = EffectDefinition.GetStatusEffectTag();
		EffectSpec.GrantedTags = EffectDefinition.GrantedTags;
		if (!EffectSpec.IsValidSpec())
		{
			return false;
		}

		return ActionAbilitySystemComponent->ApplyCombatModifierEffect(EffectSpec).IsValid();
	}

	bool ApplyHitEffectEntry(
		AActor* PrimaryTargetActor,
		const FActionDamagePayload& DamagePayload,
		const FActionHitEffectEntry& EffectEntry)
	{
		FString InvalidReason;
		if (TryBuildInvalidHitEffectReason(EffectEntry, InvalidReason))
		{
			UE_LOG(
				LogActionHitResolver,
				Warning,
				TEXT("命中效果配置无效，已忽略。Reason=%s Source=%s Instigator=%s %s"),
				*InvalidReason,
				*GetNameSafe(DamagePayload.SourceActor),
				*GetNameSafe(DamagePayload.InstigatorActor),
				*DescribeHitSource(DamagePayload));
			return false;
		}

		AActor* ResolvedTargetActor = nullptr;
		UAbilitySystemComponent* ResolvedTargetAbilitySystemComponent = nullptr;
		if (!TryResolveHitEffectApplicationTarget(
			PrimaryTargetActor,
			DamagePayload,
			EffectEntry.TargetSide,
			ResolvedTargetActor,
			ResolvedTargetAbilitySystemComponent))
		{
			return false;
		}

		switch (EffectEntry.EffectKind)
		{
		case EActionHitEffectKind::Dot:
			return ApplyDamageOverTimeDefinition(
				ResolvedTargetActor,
				ResolvedTargetAbilitySystemComponent,
				DamagePayload,
				*CastChecked<UDataAsset_ActionHitDotEffectDefinition>(EffectEntry.EffectDefinition));

		case EActionHitEffectKind::Buff:
		case EActionHitEffectKind::Debuff:
			return ApplyBuffOrDebuffDefinition(
				ResolvedTargetActor,
				ResolvedTargetAbilitySystemComponent,
				DamagePayload,
				*EffectEntry.EffectDefinition);

		default:
			break;
		}

		return false;
	}

	bool ApplyHitEffectEntries(
		AActor* PrimaryTargetActor,
		const FActionDamagePayload& DamagePayload,
		const TArray<FActionHitEffectEntry>& EffectEntries,
		const bool bDotOnly = false)
	{
		bool bAppliedAnyEffect = false;

		for (const FActionHitEffectEntry& EffectEntry : EffectEntries)
		{
			if (bDotOnly && EffectEntry.EffectKind != EActionHitEffectKind::Dot)
			{
				continue;
			}

			bAppliedAnyEffect |= ApplyHitEffectEntry(
				PrimaryTargetActor,
				DamagePayload,
				EffectEntry);
		}

		return bAppliedAnyEffect;
	}
}

bool UActionHitResolver::ApplyDamageOverTime(AActor* TargetActor, const FActionDamagePayload& DamagePayload)
{
	if (!IsValid(TargetActor) || !DamagePayload.HasAnyHitEffectSpecs())
	{
		return false;
	}

	bool bAppliedAnyDotEffect = false;
	bAppliedAnyDotEffect |= ApplyHitEffectEntries(TargetActor, DamagePayload, DamagePayload.DefaultEffects, true);
	bAppliedAnyDotEffect |= ApplyHitEffectEntries(TargetActor, DamagePayload, DamagePayload.AdditionalEffects, true);
	if (DamagePayload.bAllowAdditionalHitEffects)
	{
		bAppliedAnyDotEffect |= ApplyHitEffectEntries(
			TargetActor,
			DamagePayload,
			DamagePayload.ExternalAdditionalEffects,
			true);
	}

	return bAppliedAnyDotEffect;
}

FActionHitResolveResult UActionHitResolver::ResolveHit(AActor* TargetActor, const FActionDamagePayload& DamagePayload)
{
	FActionHitResolveResult ResolveResult;
	const bool bShouldLogExecutionDiagnostics = DamagePayload.bTreatAsExecutionHit;

	// 这条主入口本质上做三件事：
	// 1. 先决定“这次命中是否应该继续往下走”；
	// 2. 再决定“若继续往下走，优先走战斗组件接管还是标准 GE 结算”；
	// 3. 最后把真正生效的数值结果重新翻译成受击表现和处决窗口语义。
	// 也就是说，它处理的是“完整命中结算”这一层，
	// 而不是只处理某个单独子问题，例如纯掉血、纯 DOT 或纯事件广播。
	// 这也是为什么这个入口内部会同时出现：
	// 1. 目标有效性检查；
	// 2. 处决保护拦截；
	// 3. 战斗组件抢占；
	// 4. GAS 数值落地；
	// 5. 最终受击事件广播。
	// 它本身就是当前单人战斗命中的总装配点。

	// 先挡掉最外层的非法输入。
	// 这一步失败时直接返回默认结果，避免后面进入任何组件查询或 GE 构建流程。
	if (!IsValid(TargetActor) || !DamagePayload.IsValidPayload())
	{
		if (bShouldLogExecutionDiagnostics)
		{
			const FString FailureMessage = FString::Printf(
				TEXT("[GA][Execution][ResolveHit] target=%s source=%s result_type=%s applied_damage=%.2f applied_poise_damage=%.2f applied_guard_stamina_cost=%.2f reason=%s failure_detail=%s"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(DamagePayload.SourceActor),
				*DescribeHitResultType(ResolveResult.ResultType),
				ResolveResult.AppliedDamage,
				ResolveResult.AppliedPoiseDamage,
				ResolveResult.AppliedGuardStaminaCost,
				TEXT("execution_invalid_target_or_payload"),
				TEXT("Target is invalid or damage payload is not valid."));
			Debug::Print(FailureMessage, FColor::Red, 2.0f);
			UE_LOG(LogActionHitResolver, Warning, TEXT("%s"), *FailureMessage);
		}
		return ResolveResult;
	}

	// 单人项目当前不处理自伤；若后续需要自爆或反噬，再单独放开。
	if (TargetActor == DamagePayload.SourceActor || TargetActor == DamagePayload.InstigatorActor)
	{
		// 这里直接回退为默认结果，而不是硬塞一个 Ignored，
		// 目的是让上层把这种情况理解成“本次命中本来就不进入当前受击域”。
		if (bShouldLogExecutionDiagnostics)
		{
			const FString FailureMessage = FString::Printf(
				TEXT("[GA][Execution][ResolveHit] target=%s source=%s result_type=%s applied_damage=%.2f applied_poise_damage=%.2f applied_guard_stamina_cost=%.2f reason=%s failure_detail=%s"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(DamagePayload.SourceActor),
				*DescribeHitResultType(ResolveResult.ResultType),
				ResolveResult.AppliedDamage,
				ResolveResult.AppliedPoiseDamage,
				ResolveResult.AppliedGuardStaminaCost,
				TEXT("execution_self_hit_filtered"),
				TEXT("Target actor matches source or instigator and was filtered out."));
			Debug::Print(FailureMessage, FColor::Red, 2.0f);
			UE_LOG(LogActionHitResolver, Warning, TEXT("%s"), *FailureMessage);
		}
		return ResolveResult;
	}

	AActionCharacterBase* TargetCharacter = Cast<AActionCharacterBase>(TargetActor);
	if (!TargetCharacter)
	{
		if (bShouldLogExecutionDiagnostics)
		{
			const FString FailureMessage = FString::Printf(
				TEXT("[GA][Execution][ResolveHit] target=%s source=%s result_type=%s applied_damage=%.2f applied_poise_damage=%.2f applied_guard_stamina_cost=%.2f reason=%s failure_detail=%s"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(DamagePayload.SourceActor),
				*DescribeHitResultType(ResolveResult.ResultType),
				ResolveResult.AppliedDamage,
				ResolveResult.AppliedPoiseDamage,
				ResolveResult.AppliedGuardStaminaCost,
				TEXT("execution_missing_target_character"),
				TEXT("Target actor is not an ActionCharacterBase."));
			Debug::Print(FailureMessage, FColor::Red, 2.0f);
			UE_LOG(LogActionHitResolver, Warning, TEXT("%s"), *FailureMessage);
		}
		return ResolveResult;
	}

	UActionAttributeSetBase* AttributeSet = TargetCharacter->GetActionAttributeSet();
	if (!AttributeSet)
	{
		if (bShouldLogExecutionDiagnostics)
		{
			const FString FailureMessage = FString::Printf(
				TEXT("[GA][Execution][ResolveHit] target=%s source=%s result_type=%s applied_damage=%.2f applied_poise_damage=%.2f applied_guard_stamina_cost=%.2f reason=%s failure_detail=%s"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(DamagePayload.SourceActor),
				*DescribeHitResultType(ResolveResult.ResultType),
				ResolveResult.AppliedDamage,
				ResolveResult.AppliedPoiseDamage,
				ResolveResult.AppliedGuardStaminaCost,
				TEXT("execution_missing_attribute_set"),
				TEXT("Target character does not have an ActionAttributeSetBase."));
			Debug::Print(FailureMessage, FColor::Red, 2.0f);
			UE_LOG(LogActionHitResolver, Warning, TEXT("%s"), *FailureMessage);
		}
		return ResolveResult;
	}

	UAbilitySystemComponent* TargetAbilitySystemComponent = TargetCharacter->GetAbilitySystemComponent();
	if (!TargetAbilitySystemComponent)
	{
		if (bShouldLogExecutionDiagnostics)
		{
			const FString FailureMessage = FString::Printf(
				TEXT("[GA][Execution][ResolveHit] target=%s source=%s result_type=%s applied_damage=%.2f applied_poise_damage=%.2f applied_guard_stamina_cost=%.2f reason=%s failure_detail=%s"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(DamagePayload.SourceActor),
				*DescribeHitResultType(ResolveResult.ResultType),
				ResolveResult.AppliedDamage,
				ResolveResult.AppliedPoiseDamage,
				ResolveResult.AppliedGuardStaminaCost,
				TEXT("execution_missing_ability_system_component"),
				TEXT("Target character does not have an ability system component."));
			Debug::Print(FailureMessage, FColor::Red, 2.0f);
			UE_LOG(LogActionHitResolver, Warning, TEXT("%s"), *FailureMessage);
		}
		return ResolveResult;
	}

	const auto FinalizeAndReturnResolveResult = [&]() -> FActionHitResolveResult
	{
		BroadcastCombatFeedbackEvent(TargetActor, DamagePayload, ResolveResult);
		return ResolveResult;
	};

	const auto LogExecutionDiagnostic = [&](const TCHAR* Reason, const FString& FailureDetail)
	{
		if (!bShouldLogExecutionDiagnostics)
		{
			return;
		}

		const FString Message = FString::Printf(
			TEXT("[GA][Execution][ResolveHit] target=%s source=%s result_type=%s applied_damage=%.2f applied_poise_damage=%.2f applied_guard_stamina_cost=%.2f %s reason=%s failure_detail=%s"),
			*GetNameSafe(TargetActor),
			*GetNameSafe(DamagePayload.SourceActor),
			*DescribeHitResultType(ResolveResult.ResultType),
			ResolveResult.AppliedDamage,
			ResolveResult.AppliedPoiseDamage,
			ResolveResult.AppliedGuardStaminaCost,
			*DescribeExecutionTargetTags(TargetAbilitySystemComponent),
			Reason,
			*FailureDetail);
		UE_LOG(LogActionHitResolver, Warning, TEXT("%s"), *Message);
	};

	// 从这里开始，目标已经满足“是标准可受击角色，并且具备完整 GAS 结算条件”。
	// 后面所有分支都建立在这个前提之上，因此可以安全读取属性、标签和目标战斗组件。

	// 超级护甲只影响“受击表现是否被压住”，
	// 不直接阻止数值伤害本身进入正式结算。
	const bool bTargetHasSuperArmor = HasSuperArmor(TargetAbilitySystemComponent);
	// 这也是为什么超级护甲判断被放在前面缓存，而不是等到结算末尾再临时读取：
	// 最终受击事件推导、破韧是否补开处决窗口，都要共享同一帧的护甲语义。

	// 处决期间无敌是当前框架最高优先级的受击拦截条件。
	if (HasExecutionInvulnerability(TargetAbilitySystemComponent))
	{
		LogExecutionDiagnostic(
			TEXT("execution_blocked_by_invulnerability"),
			TEXT("Target ASC already has execution invulnerability."));
		ResolveResult.ResultType = EActionHitResultType::Ignored;
		return FinalizeAndReturnResolveResult();
	}

	// 处决受害者锁定是“双边处决保护”的目标侧入口。
	// 目标一旦进入处决演出，就不会再被其它来源的伤害或效果打断。
	if (IsBlockedByExecutionVictimLock(TargetActor, TargetAbilitySystemComponent, DamagePayload))
	{
		LogExecutionDiagnostic(
			TEXT("execution_blocked_by_victim_lock"),
			TEXT("Target is protected by execution victim lock."));
		ResolveResult.ResultType = EActionHitResultType::Ignored;
		return FinalizeAndReturnResolveResult();
	}

	// 先让目标自己的战斗组件抢占这次命中。
	// 这样格挡、弹反、受击接管等“高于普通掉血”的链条能优先生效，
	// 解析器只在目标没有拦截这次命中时，才继续走标准数值结算。
	if (UPawnCombatComponent* CombatComponent = TargetActor->FindComponentByClass<UPawnCombatComponent>())
	{
		if (CombatComponent->TryHandleIncomingDamage(DamagePayload, ResolveResult))
		{
			if (bShouldLogExecutionDiagnostics && ResolveResult.ResultType == EActionHitResultType::None)
			{
				const FString FailureDetail = FString::Printf(
					TEXT("Target combat component handled the hit with result_type=%s."),
					*DescribeHitResultType(ResolveResult.ResultType));
				LogExecutionDiagnostic(TEXT("execution_component_handled_hit"), FailureDetail);
			}

			// 这一步体现的是“目标战斗状态机优先于解析器普通掉血链”的原则。
			// 只要目标已经明确接管了这次命中，解析器就不再擅自补任何常规生命/韧性结算，
			// 避免格挡、弹反、特殊受击链与普通伤害链发生双重结算。
			// 只要战斗组件接管成功，这次命中的“高优先级战斗语义”就已经有了最终裁决。
			// 后面的标准伤害链不应再重复执行，否则会出现
			// “已经被格挡 / 弹反 / 特殊接管了，但解析器又补了一次普通掉血”的双重结算。
			if (ResolveResult.ResultType == EActionHitResultType::Blocked)
			{
				// 普通格挡不扣生命，只额外结算一次格挡体力损耗。
				const float PreviousStamina = AttributeSet->GetStamina();
				ApplyHitDamageGameplayEffect(
					TargetActor,
					TargetAbilitySystemComponent,
					DamagePayload,
					0.f,
					DamagePayload.GuardStaminaCost,
					0.f,
					1.f);
				ResolveResult.AppliedGuardStaminaCost = FMath::Max(PreviousStamina - AttributeSet->GetStamina(), 0.f);

				// 若格挡把体力打空，则将结果提升为“崩防”，方便后续系统单独监听。
				// 这里依旧以“结算前后真实体力差”和“当前是否已归零”为准，
				// 而不是只看载荷里声明了多大格挡削体力。
				ResolveResult.bGuardBroken = PreviousStamina > 0.f && AttributeSet->GetStamina() <= 0.f;
				if (ResolveResult.bGuardBroken)
				{
					ResolveResult.ResultType = EActionHitResultType::GuardBroken;
					ResolveResult.AppliedHitReactType = EActionHitReactType::HitStun;
					// 崩防属于“由格挡链升级出来的特殊战斗结果”，
					// 因此这里直接广播 GuardBreak，而不再回到普通 ResolveDamageReactEventTag 的推导路径。
					BroadcastCombatReactEvent(TargetActor, ActionGameplayTags::Combat_Event_GuardBreak, DamagePayload);
				}

				return FinalizeAndReturnResolveResult();
			}

			// 其它所有已被战斗组件接管的结果，也都在这里直接收口返回。
			// 这保证了解析器不会越权改写已经由目标战斗状态机做出的判定。
			return FinalizeAndReturnResolveResult();
		}
	}

	const float PreviousHealth = AttributeSet->GetHealth();
	const float PreviousShield = AttributeSet->GetShield();
	const float PreviousStamina = AttributeSet->GetStamina();
	const float PreviousPoise = AttributeSet->GetPoise();
	const float ExecutionDamageMultiplier = ResolveExecutionDamageMultiplier(DamagePayload);
	FString ExecutionDamageApplyFailureReason;

	// 普通受击正式进入 GE 结算阶段。
	// 这里先缓存旧值，再用结算后的属性差值回填 ResolveResult，
	// 可以保证最终结果反映的是“真实生效数值”，而不是载荷里声明的理想数值。
	ResolveResult.ResultType = EActionHitResultType::Damaged;
	const bool bAppliedExecutionDamage = ApplyHitDamageGameplayEffect(
		TargetActor,
		TargetAbilitySystemComponent,
		DamagePayload,
		DamagePayload.BaseDamage,
		0.f,
		DamagePayload.PoiseDamage,
		ExecutionDamageMultiplier,
		&ExecutionDamageApplyFailureReason);
	// ResolveResult 的回填一律以“结算前后真实属性差值”为准，
	// 而不是直接复用载荷里声明的理想伤害值。
	// 这样后续即便继续接入抗性、减伤、易伤、来源修正等公式，
	// 结果结构仍然能反映本次命中真正生效了多少。
	// 这也是解析器对外最重要的契约之一：ResolveResult 永远描述“真实落地结果”，
	// 而不是“攻击者原本想打出多少”。
	ResolveResult.AppliedDamage = FMath::Max(PreviousHealth - AttributeSet->GetHealth(), 0.f);
	const float AppliedShieldDamage = FMath::Max(PreviousShield - AttributeSet->GetShield(), 0.f);
	ResolveResult.AppliedGuardStaminaCost = FMath::Max(PreviousStamina - AttributeSet->GetStamina(), 0.f);
	ResolveResult.AppliedPoiseDamage = FMath::Max(PreviousPoise - AttributeSet->GetPoise(), 0.f);
	ResolveResult.bTargetDied = !AttributeSet->IsAlive();
	ResolveResult.bPoiseBroken = PreviousPoise > 0.f && AttributeSet->GetPoise() <= 0.f;

	// 注意这里并不直接相信 DamagePayload 里声明的伤害值已经全部生效。
	// 真正写回结果时一律以 AttributeSet 结算前后的差值为准，
	// 这样后续接了抗性、减伤、易伤等公式后，ResolveResult 依旧能保持真实可信。
	// 同理，破韧也不是“载荷里声明了削韧就算破韧”，
	// 而是必须满足“结算前有韧性、结算后韧性已归零”这两个真实条件。

	// 直接生命伤害型攻击仍要求首段先打出生命伤害，才允许继续挂 DOT；
	// 但如果这次载荷本来就是“无直伤的纯状态命中”，则允许它直接进入 DOT 结算。
	if (!ResolveResult.bTargetDied)
	{
		ResolveResult.bAppliedHitEffect |= ApplyHitEffectEntries(
			TargetActor,
			DamagePayload,
			DamagePayload.DefaultEffects);
		ResolveResult.bAppliedHitEffect |= ApplyHitEffectEntries(
			TargetActor,
			DamagePayload,
			DamagePayload.AdditionalEffects);

		if (DamagePayload.bAllowAdditionalHitEffects)
		{
			ResolveResult.bAppliedHitEffect |= ApplyHitEffectEntries(
				TargetActor,
				DamagePayload,
				DamagePayload.ExternalAdditionalEffects);
		}
	}

	const bool bHasNumericHitResult =
		ResolveResult.AppliedDamage > 0.f
		|| ResolveResult.AppliedGuardStaminaCost > 0.f
		|| ResolveResult.AppliedPoiseDamage > 0.f;
	const bool bHasReactIntent =
		DamagePayload.HitReactType != EActionHitReactType::None
		|| ResolveResult.bPoiseBroken;

	// 到这里仍然没有数值结果、没有 DOT、也没有任何受击意图时，
	// 说明这次命中从战斗层面等价于“什么都没发生”，应统一回落为 Ignored。
	if (!bHasNumericHitResult
		&& !ResolveResult.bAppliedHitEffect
		&& !bHasReactIntent)
	{
		// 这里把“没有任何真实数值变化，也没有状态附着，也没有表现意图”的命中统一折叠成 Ignored，
		// 方便外部系统把这类结果当成一次真正的“无事发生”处理。
		// 这样上层调用方就不需要自己再写一套“伤害为 0 但又没挂上任何效果时算什么”的二次判定。
		const FString IgnoredFailureDetail = AppliedShieldDamage > 0.f
			? FString::Printf(
				TEXT("GE applied but damage was absorbed by shield only. health_before=%.2f health_after=%.2f shield_before=%.2f shield_after=%.2f shield_absorbed=%.2f poise_before=%.2f poise_after=%.2f"),
				PreviousHealth,
				AttributeSet->GetHealth(),
				PreviousShield,
				AttributeSet->GetShield(),
				AppliedShieldDamage,
				PreviousPoise,
				AttributeSet->GetPoise())
			: FString::Printf(
				TEXT("No numeric hit result, hit effects, or react intent were produced. health_before=%.2f health_after=%.2f shield_before=%.2f shield_after=%.2f poise_before=%.2f poise_after=%.2f ge_applied=%s"),
				PreviousHealth,
				AttributeSet->GetHealth(),
				PreviousShield,
				AttributeSet->GetShield(),
				PreviousPoise,
				AttributeSet->GetPoise(),
				bAppliedExecutionDamage ? TEXT("true") : TEXT("false"));
		const TCHAR* IgnoredReason = AppliedShieldDamage > 0.f
			? TEXT("execution_damage_absorbed_by_shield")
			: (bAppliedExecutionDamage
				? TEXT("execution_no_attribute_delta_after_spec_submit")
				: (ExecutionDamageApplyFailureReason.IsEmpty()
					? TEXT("execution_resolve_returned_ignored")
					: *ExecutionDamageApplyFailureReason));
		LogExecutionDiagnostic(
			IgnoredReason,
			IgnoredFailureDetail);
		ResolveResult.ResultType = EActionHitResultType::Ignored;
		return FinalizeAndReturnResolveResult();
	}

	// 处决命中本身已经处于独立演出链中：
	// 1. 目标此时通常已被锁成“处决受害者”，不应再额外进入普通受击表现；
	// 2. 若这里继续广播 PoiseBreak / HitReact，处决窗口组件会把它当成一次新的处决窗口触发；
	// 3. 这会导致当前处决命中后又把窗口重新打开，污染后续提交与收尾状态。
	// 因此处决命中只做数值结算，不再触发普通受击反应与新的处决窗口事件。
	if (DamagePayload.bTreatAsExecutionHit)
	{
		// 处决命中在这里直接截断，原因有两个：
		// 1. 它的表现和收尾已经由处决演出链自己接管；
		// 2. 若这里再继续推普通受击 / 破韧事件，会把目标重新送回常规受击状态机，污染处决收尾。
		return FinalizeAndReturnResolveResult();
	}

	if (!ResolveResult.bTargetDied)
	{
		// 数值已经结算完成后，再决定表现层最终应收到哪条事件。
		// 这样可以保证“击飞 / 击倒 / 破韧 / 普通受击”的广播依据的是最终结果，而不是输入意图。
		const FGameplayTag FinalReactEventTag =
			ResolveDamageReactEventTag(DamagePayload, ResolveResult, bTargetHasSuperArmor);

		if (FinalReactEventTag.IsValid())
		{
			ResolveResult.AppliedHitReactType = ResolveAppliedHitReactType(FinalReactEventTag);
			// 到这里才真正把“数值结果”翻译成“表现层事件”。
			// 这样无论是普通受击、击飞、击倒还是破韧，监听者拿到的都已经是最终裁决后的统一结论。
			// 这一步故意被放在数值结算之后，避免表现层先响应该事件、而数值层最终却没有真正生效。
			BroadcastCombatReactEvent(TargetActor, FinalReactEventTag, DamagePayload);
		}
		else if (bTargetHasSuperArmor && bHasReactIntent)
		{
			// 没有最终事件并不一定代表“这次命中什么表现都没有”。
			// 对超级护甲目标来说，可能是受击表现被成功压住了，因此要把这层信息写回结果供外部感知。
			ResolveResult.bReactBlockedBySuperArmor = true;
		}

		// 恢复保护期间即便数值上把韧性削空，只要本次命中没有绕过超级护甲，
		// 就不应继续把它升级成“可处决窗口”。
		// 否则会出现“受击表现被保护挡住了，但处决窗口却悄悄打开”的状态穿透。
		if (ResolveResult.bPoiseBroken)
		{
			UE_LOG(
				ActionRPG,
				Log,
				TEXT("poise_break_resolve_result owner=%s instigator=%s source=%s final_event=%s applied_hit_react=%d previous_poise=%.2f current_poise=%.2f target_super_armor=%s"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(DamagePayload.InstigatorActor.Get()),
				*GetNameSafe(DamagePayload.SourceActor.Get()),
				*FinalReactEventTag.ToString(),
				static_cast<int32>(ResolveResult.AppliedHitReactType),
				PreviousPoise,
				AttributeSet->GetPoise(),
				bTargetHasSuperArmor ? TEXT("true") : TEXT("false"));
		}

		if (ResolveResult.bPoiseBroken && !bTargetHasSuperArmor)
		{
			// 这里补开处决窗口的前提非常严格：
			// 必须是“这次真的打出了破韧”，并且这次破韧没有被超级护甲的表现保护挡掉。
			// 只有这样，数值层的破韧和表现层的“目标已经进入可处决状态”才是一致的。
			// 同时这里特意复用 NotifyPoiseBreakExecutionWindow，而不是直接假设最终表现一定是 PoiseBreak：
			// “最终广播哪条受击事件”与“是否应该让处决窗口系统接住这次破韧”是两条并行语义。
			NotifyPoiseBreakExecutionWindow(TargetActor, FinalReactEventTag, DamagePayload);
		}
	}

	return FinalizeAndReturnResolveResult();
}
