// 文件说明：实现英雄侧通用战斗反馈组件逻辑。

#include "Components/Combat/HeroCombatFeedbackComponent.h"

#include "Items/Projectiles/ActionProjectileBase.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

UHeroCombatFeedbackComponent::UHeroCombatFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHeroCombatFeedbackComponent::HandleCombatFeedbackEvent(
	const FActionCombatFeedbackEvent& InCombatFeedbackEvent)
{
	TryPlayWorldEffect(InCombatFeedbackEvent);
	TryPlayWorldSound(InCombatFeedbackEvent);

	OnCombatFeedbackEvent.Broadcast(InCombatFeedbackEvent);

	if (ShouldForwardEventToHUD(InCombatFeedbackEvent))
	{
		OnCombatFeedbackHUDDisplayEvent.Broadcast(InCombatFeedbackEvent);
	}
}

void UHeroCombatFeedbackComponent::HandleProjectilePresentationEvent(
	AActionProjectileBase* InProjectileActor,
	const FActionProjectilePresentationEvent& InPresentationEvent,
	const FActionProjectileLifecyclePresentationConfig& InLifecyclePresentationConfig)
{
	FActionCombatFeedbackEvent CombatFeedbackEvent;
	switch (InPresentationEvent.EventType)
	{
	case EActionProjectilePresentationEventType::Spawned:
		CombatFeedbackEvent.EventType = EActionCombatFeedbackEventType::ProjectileSpawned;
		break;

	case EActionProjectilePresentationEventType::WorldBlocked:
		CombatFeedbackEvent.EventType = EActionCombatFeedbackEventType::ProjectileWorldBlocked;
		break;

	case EActionProjectilePresentationEventType::Destroyed:
		CombatFeedbackEvent.EventType = EActionCombatFeedbackEventType::ProjectileDestroyed;
		break;

	default:
		return;
	}

	CombatFeedbackEvent.HitResultType = InPresentationEvent.HitResultType;
	CombatFeedbackEvent.InstigatorActor = InPresentationEvent.InstigatorActor;
	CombatFeedbackEvent.SourceActor = InProjectileActor;
	CombatFeedbackEvent.TargetActor = InPresentationEvent.TargetActor;
	CombatFeedbackEvent.ImpactLocation = InPresentationEvent.ImpactLocation;
	CombatFeedbackEvent.ImpactNormal = InPresentationEvent.ImpactNormal;
	CombatFeedbackEvent.DamageType = InPresentationEvent.DamageType;
	CombatFeedbackEvent.DamageElementTypeTag = InPresentationEvent.DamageElementTypeTag;
	CombatFeedbackEvent.ProjectileTag = InPresentationEvent.ProjectileTag;
	CombatFeedbackEvent.ResolvedConfigSource = InPresentationEvent.ResolvedConfigSource;
	CombatFeedbackEvent.SelectedProjectileConfigTag = InPresentationEvent.SelectedProjectileConfigTag;
	CombatFeedbackEvent.SpawnSocketName = InPresentationEvent.SpawnSocketName;
	CombatFeedbackEvent.DestroyReason = InPresentationEvent.DestroyReason;
	CombatFeedbackEvent.Effect = InLifecyclePresentationConfig.Effect;
	CombatFeedbackEvent.Sound = InLifecyclePresentationConfig.Sound;
	CombatFeedbackEvent.bShouldShowDamageNumber = false;
	CombatFeedbackEvent.bShouldPlayImpactEffect = InLifecyclePresentationConfig.bPlayEffect;
	CombatFeedbackEvent.bShouldPlayImpactSound = InLifecyclePresentationConfig.bPlaySound;

	HandleCombatFeedbackEvent(CombatFeedbackEvent);
}

bool UHeroCombatFeedbackComponent::ShouldForwardEventToHUD(
	const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const
{
	return InCombatFeedbackEvent.bShouldShowDamageNumber;
}

bool UHeroCombatFeedbackComponent::ShouldPlayWorldEffect(
	const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const
{
	return InCombatFeedbackEvent.bShouldPlayImpactEffect
		&& !InCombatFeedbackEvent.Effect.IsNull();
}

bool UHeroCombatFeedbackComponent::ShouldPlayWorldSound(
	const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const
{
	return InCombatFeedbackEvent.bShouldPlayImpactSound
		&& !InCombatFeedbackEvent.Sound.IsNull();
}

void UHeroCombatFeedbackComponent::TryPlayWorldEffect(
	const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const
{
	if (!ShouldPlayWorldEffect(InCombatFeedbackEvent))
	{
		return;
	}

	UNiagaraSystem* Effect = InCombatFeedbackEvent.Effect.Get();
	if (!Effect || !GetWorld())
	{
		return;
	}

	const FVector SpawnLocation = ResolveFeedbackWorldLocation(InCombatFeedbackEvent);
	const FRotator SpawnRotation = InCombatFeedbackEvent.ImpactNormal.IsNearlyZero()
		? FRotator::ZeroRotator
		: InCombatFeedbackEvent.ImpactNormal.Rotation();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		Effect,
		SpawnLocation,
		SpawnRotation);
}

void UHeroCombatFeedbackComponent::TryPlayWorldSound(
	const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const
{
	if (!ShouldPlayWorldSound(InCombatFeedbackEvent))
	{
		return;
	}

	USoundBase* Sound = InCombatFeedbackEvent.Sound.Get();
	if (!Sound || !GetWorld())
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		GetWorld(),
		Sound,
		ResolveFeedbackWorldLocation(InCombatFeedbackEvent));
}

FVector UHeroCombatFeedbackComponent::ResolveFeedbackWorldLocation(
	const FActionCombatFeedbackEvent& InCombatFeedbackEvent) const
{
	if (!InCombatFeedbackEvent.ImpactLocation.IsNearlyZero())
	{
		return InCombatFeedbackEvent.ImpactLocation;
	}

	if (IsValid(InCombatFeedbackEvent.TargetActor))
	{
		return InCombatFeedbackEvent.TargetActor->GetActorLocation();
	}

	if (IsValid(InCombatFeedbackEvent.SourceActor))
	{
		return InCombatFeedbackEvent.SourceActor->GetActorLocation();
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetActorLocation();
	}

	return FVector::ZeroVector;
}
