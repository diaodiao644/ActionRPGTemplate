// 文件说明：实现基础发射物的初始化、飞行命中与受击解析逻辑。

#include "Items/Projectiles/ActionProjectileBase.h"

#include "Characters/ActionHeroCharacter.h"
#include "Combat/ActionHitResolver.h"
#include "Components/Collision/ActionCollisionRuntimeComponent.h"
#include "Components/Combat/HeroCombatFeedbackComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Debug/ActionDebugHelper.h"
#include "GameFramework/ProjectileMovementComponent.h"

namespace ActionProjectileRuntimeDebug
{
	static const TCHAR* GetSuccessfulHitPolicyText(const EActionProjectileSuccessfulHitPolicy InPolicy)
	{
		switch (InPolicy)
		{
		case EActionProjectileSuccessfulHitPolicy::ContinueFlight:
			return TEXT("命中后继续飞行");

		case EActionProjectileSuccessfulHitPolicy::DestroyImmediately:
		default:
			break;
		}

		return TEXT("命中后立即销毁");
	}

	static const TCHAR* GetWorldBlockPolicyText(const EActionProjectileWorldBlockPolicy InPolicy)
	{
		switch (InPolicy)
		{
		case EActionProjectileWorldBlockPolicy::DestroyOnBlock:
			return TEXT("撞到阻挡立刻销毁");

		case EActionProjectileWorldBlockPolicy::IgnoreWorld:
		default:
			break;
		}

		return TEXT("忽略世界阻挡");
	}

	static const TCHAR* GetPresentationEventTypeText(const EActionProjectilePresentationEventType InEventType)
	{
		switch (InEventType)
		{
		case EActionProjectilePresentationEventType::HitResolved:
			return TEXT("命中成功");

		case EActionProjectilePresentationEventType::HitResolveIgnored:
			return TEXT("命中未结算");

		case EActionProjectilePresentationEventType::WorldBlocked:
			return TEXT("撞到世界阻挡");

		case EActionProjectilePresentationEventType::Destroyed:
			return TEXT("生命周期销毁");

		case EActionProjectilePresentationEventType::Spawned:
		default:
			break;
		}

		return TEXT("生成成功");
	}

	static const TCHAR* GetDestroyReasonText(const EActionProjectileDestroyReason InDestroyReason)
	{
		switch (InDestroyReason)
		{
		case EActionProjectileDestroyReason::SuccessfulHitPolicy:
			return TEXT("命中策略要求销毁");

		case EActionProjectileDestroyReason::SuccessfulHitLimitReached:
			return TEXT("达到最大成功命中数");

		case EActionProjectileDestroyReason::WorldBlocked:
			return TEXT("撞到世界阻挡");

		case EActionProjectileDestroyReason::LifeSpanExpired:
			return TEXT("寿命到期");

		case EActionProjectileDestroyReason::ExternalDestroyed:
			return TEXT("外部主动销毁");

		case EActionProjectileDestroyReason::None:
		default:
			break;
		}

		return TEXT("未销毁");
	}

	static const TCHAR* GetHitResultTypeText(const EActionHitResultType InResultType)
	{
		switch (InResultType)
		{
		case EActionHitResultType::Damaged:
			return TEXT("已造成伤害");

		case EActionHitResultType::Blocked:
			return TEXT("被格挡");

		case EActionHitResultType::GuardBroken:
			return TEXT("破防");

		case EActionHitResultType::Parried:
			return TEXT("被精准格挡");

		case EActionHitResultType::PerfectDodged:
			return TEXT("被完美闪避");

		case EActionHitResultType::Ignored:
			return TEXT("被忽略");

		case EActionHitResultType::None:
		default:
			break;
		}

		return TEXT("无结果");
	}

	static FColor GetPresentationEventDebugColor(const EActionProjectilePresentationEventType InEventType)
	{
		switch (InEventType)
		{
		case EActionProjectilePresentationEventType::Spawned:
		case EActionProjectilePresentationEventType::HitResolved:
			return FColor::Green;

		case EActionProjectilePresentationEventType::HitResolveIgnored:
			return FColor::Yellow;

		case EActionProjectilePresentationEventType::WorldBlocked:
			return FColor::Orange;

		case EActionProjectilePresentationEventType::Destroyed:
		default:
			break;
		}

		return FColor::Cyan;
	}

	static void PrintPresentationEvent(
		const AActionProjectileBase* Projectile,
		const FActionProjectilePresentationEvent& PresentationEvent)
	{
		Debug::Print(FString::Printf(
			TEXT("[发射物表现] 事件=%s，Projectile=%s，Target=%s，发射物标签=%s，来源=%d，选中标签=%s，Socket=%s，命中结果=%s，命中策略=%s，阻挡策略=%s，成功命中数=%d/%d，是否即将销毁=%s，销毁原因=%s"),
			GetPresentationEventTypeText(PresentationEvent.EventType),
			*GetNameSafe(Projectile),
			*GetNameSafe(PresentationEvent.TargetActor),
			PresentationEvent.ProjectileTag.IsValid() ? *PresentationEvent.ProjectileTag.ToString() : TEXT("未配置"),
			static_cast<int32>(PresentationEvent.ResolvedConfigSource),
			PresentationEvent.SelectedProjectileConfigTag.IsValid()
				? *PresentationEvent.SelectedProjectileConfigTag.ToString()
				: TEXT("默认发射物"),
			PresentationEvent.SpawnSocketName != NAME_None
				? *PresentationEvent.SpawnSocketName.ToString()
				: TEXT("无"),
			GetHitResultTypeText(PresentationEvent.HitResultType),
			GetSuccessfulHitPolicyText(PresentationEvent.SuccessfulHitPolicy),
			GetWorldBlockPolicyText(PresentationEvent.WorldBlockPolicy),
			PresentationEvent.SuccessfulResolvedTargetCount,
			PresentationEvent.MaxSuccessfulTargetHitCount > 0
				? PresentationEvent.MaxSuccessfulTargetHitCount
				: -1,
			PresentationEvent.bWillDestroyAfterEvent ? TEXT("是") : TEXT("否"),
			GetDestroyReasonText(PresentationEvent.DestroyReason)),
			GetPresentationEventDebugColor(PresentationEvent.EventType),
			1.8f);
	}

	static UHeroCombatFeedbackComponent* ResolveCombatFeedbackComponent(
		const AActionProjectileBase* Projectile)
	{
		if (!Projectile)
		{
			return nullptr;
		}

		const AActionHeroCharacter* InstigatorHero =
			Cast<AActionHeroCharacter>(Projectile->GetInstigator());
		if (!InstigatorHero)
		{
			InstigatorHero = Cast<AActionHeroCharacter>(Projectile->GetOwner());
		}

		return InstigatorHero ? InstigatorHero->GetHeroCombatFeedbackComponent() : nullptr;
	}

	static const FActionProjectileLifecyclePresentationConfig& GetEmptyLifecyclePresentationConfig()
	{
		static const FActionProjectileLifecyclePresentationConfig EmptyConfig;
		return EmptyConfig;
	}
}

AActionProjectileBase::AActionProjectileBase()
	: Super()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(12.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnProjectileCollisionBeginOverlap);

	ActionCollisionRuntimeComponent =
		CreateDefaultSubobject<UActionCollisionRuntimeComponent>(TEXT("ActionCollisionRuntimeComponent"));

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->InitialSpeed = 1800.f;
	ProjectileMovementComponent->MaxSpeed = 1800.f;
	ProjectileMovementComponent->ProjectileGravityScale = 0.f;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->OnProjectileStop.AddDynamic(this, &ThisClass::HandleProjectileStop);
}

void AActionProjectileBase::InitializeProjectile(
	const FActionDamagePayload& InDamagePayloadTemplate,
	const FActionProjectileInitializationContext& InInitializationContext)
{
	DamagePayloadTemplate = InDamagePayloadTemplate;
	InitializationContext = InInitializationContext;
	DamagePayloadTemplate.SourceActor = this;
	DamagePayloadTemplate.HitSource.SourceId = ActionHitSourceDefaults::GetProjectileSourceId();
	DamagePayloadTemplate.HitSource.SourceType = EActionHitSourceType::Projectile;
	DamagePayloadTemplate.HitSource.SourceComponentName = CollisionComponent
		? CollisionComponent->GetFName()
		: NAME_None;
	bProjectileInitialized = DamagePayloadTemplate.IsValidPayload();
	SuccessfulResolvedTargetCount = 0;
	DestroyReason = EActionProjectileDestroyReason::None;
	bHasBroadcastDestroyedEvent = false;
	LastPresentationEvent = FActionProjectilePresentationEvent();
	ClearHitActors();

	if (!bProjectileInitialized)
	{
		Debug::Print(FString::Printf(
			TEXT("[发射物] 初始化失败：Projectile=%s，载荷无效"),
			*GetNameSafe(this)),
			FColor::Red,
			2.0f);
		return;
	}

	BroadcastPresentationEvent(BuildPresentationEvent(
		EActionProjectilePresentationEventType::Spawned));
}

void AActionProjectileBase::ApplyProjectileConfig(const FActionProjectileConfig& InProjectileConfig)
{
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->InitialSpeed = FMath::Max(InProjectileConfig.InitialSpeed, 0.f);
		ProjectileMovementComponent->MaxSpeed = FMath::Max(InProjectileConfig.MaxSpeed, 0.f);
		ProjectileMovementComponent->ProjectileGravityScale = InProjectileConfig.GravityScale;
	}

	SuccessfulHitPolicy = InProjectileConfig.ResolveSuccessfulHitPolicy();
	WorldBlockPolicy = InProjectileConfig.ResolveWorldBlockPolicy();
	MaxSuccessfulTargetHitCount = InProjectileConfig.ResolveMaxSuccessfulTargetHitCount();
	SpawnPresentationConfig = InProjectileConfig.SpawnPresentationConfig;
	WorldBlockedPresentationConfig = InProjectileConfig.WorldBlockedPresentationConfig;
	DestroyedPresentationConfig = InProjectileConfig.DestroyedPresentationConfig;
	ApplyProjectileCollisionRuntimePreset();

	if (InProjectileConfig.LifeSeconds > 0.f)
	{
		SetLifeSpan(InProjectileConfig.LifeSeconds);
	}
	else
	{
		SetLifeSpan(0.f);
	}
}

void AActionProjectileBase::BeginPlay()
{
	Super::BeginPlay();
}

void AActionProjectileBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	EnsureProjectileCollisionSlotRegistered();
	ApplyProjectileCollisionRuntimePreset();
}

void AActionProjectileBase::LifeSpanExpired()
{
	DestroyProjectileWithReason(EActionProjectileDestroyReason::LifeSpanExpired);
}

void AActionProjectileBase::Destroyed()
{
	if (!bHasBroadcastDestroyedEvent)
	{
		DestroyReason = DestroyReason != EActionProjectileDestroyReason::None
			? DestroyReason
			: EActionProjectileDestroyReason::ExternalDestroyed;
		BroadcastPresentationEvent(BuildPresentationEvent(
			EActionProjectilePresentationEventType::Destroyed,
			nullptr,
			GetActorLocation(),
			FVector::ZeroVector,
			EActionHitResultType::None,
			false,
			DestroyReason));
		bHasBroadcastDestroyedEvent = true;
	}

	Super::Destroyed();
}

void AActionProjectileBase::OnProjectileCollisionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bProjectileInitialized || !IsValid(OtherActor) || OtherActor == this || OtherActor == GetOwner())
	{
		return;
	}

	if (HitActors.Contains(TWeakObjectPtr<AActor>(OtherActor)))
	{
		return;
	}

	FActionDamagePayload DamagePayload;
	if (!BuildImpactDamagePayload(OtherActor, DamagePayload))
	{
		Debug::Print(TEXT("[发射物] 命中时未能构建有效伤害载荷"), FColor::Red, 2.0f);
		return;
	}

	const FVector ImpactLocation = SweepResult.ImpactPoint.IsNearlyZero()
		? OtherActor->GetActorLocation()
		: FVector(SweepResult.ImpactPoint);
	const FVector ImpactNormal = SweepResult.ImpactNormal.IsNearlyZero()
		? (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal()
		: FVector(SweepResult.ImpactNormal);

	const FActionHitResolveResult ResolveResult = UActionHitResolver::ResolveHit(OtherActor, DamagePayload);
	if (!ResolveResult.WasResolved())
	{
		BroadcastPresentationEvent(BuildPresentationEvent(
			EActionProjectilePresentationEventType::HitResolveIgnored,
			OtherActor,
			ImpactLocation,
			ImpactNormal,
			ResolveResult.ResultType));
		return;
	}

	RewardInstigatorSpecialWeaponSwitchEnergy(DamagePayload, ResolveResult);
	HitActors.Add(TWeakObjectPtr<AActor>(OtherActor));
	HandleSuccessfulResolvedTargetHit(OtherActor, ResolveResult, ImpactLocation, ImpactNormal);
}

void AActionProjectileBase::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (!bProjectileInitialized || WorldBlockPolicy != EActionProjectileWorldBlockPolicy::DestroyOnBlock)
	{
		return;
	}

	AActor* ImpactActor = ImpactResult.GetActor();
	const FActionProjectilePresentationEvent WorldBlockedEvent = BuildPresentationEvent(
		EActionProjectilePresentationEventType::WorldBlocked,
		ImpactActor,
		ImpactResult.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(ImpactResult.ImpactPoint),
		FVector(ImpactResult.ImpactNormal),
		EActionHitResultType::None,
		true,
		EActionProjectileDestroyReason::WorldBlocked);
	BroadcastPresentationEvent(WorldBlockedEvent);
	DestroyProjectileWithReason(EActionProjectileDestroyReason::WorldBlocked, &WorldBlockedEvent);
}

bool AActionProjectileBase::BuildImpactDamagePayload(AActor* OtherActor, FActionDamagePayload& OutDamagePayload) const
{
	OutDamagePayload = DamagePayloadTemplate;
	if (!OutDamagePayload.IsValidPayload() || !IsValid(OtherActor))
	{
		return false;
	}

	OutDamagePayload.SourceActor = const_cast<AActionProjectileBase*>(this);
	OutDamagePayload.HitSource.SourceId = ActionHitSourceDefaults::GetProjectileSourceId();
	OutDamagePayload.HitSource.SourceType = EActionHitSourceType::Projectile;
	OutDamagePayload.HitSource.SourceComponentName = CollisionComponent
		? CollisionComponent->GetFName()
		: NAME_None;
	OutDamagePayload.ImpactDirection = (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	return OutDamagePayload.IsValidPayload();
}

void AActionProjectileBase::RewardInstigatorSpecialWeaponSwitchEnergy(
	const FActionDamagePayload& DamagePayload,
	const FActionHitResolveResult& ResolveResult) const
{
	if (DamagePayload.SpecialWeaponSwitchEnergyRewardOnHit <= 0.f)
	{
		return;
	}

	if (ResolveResult.ResultType != EActionHitResultType::Damaged
		&& ResolveResult.ResultType != EActionHitResultType::Blocked
		&& ResolveResult.ResultType != EActionHitResultType::GuardBroken)
	{
		return;
	}

	AActionHeroCharacter* InstigatorHero = Cast<AActionHeroCharacter>(DamagePayload.InstigatorActor);
	if (!InstigatorHero)
	{
		return;
	}

	if (UHeroEquipmentComponent* EquipmentComponent = InstigatorHero->FindComponentByClass<UHeroEquipmentComponent>())
	{
		EquipmentComponent->AddSpecialWeaponSwitchEnergy(DamagePayload.SpecialWeaponSwitchEnergyRewardOnHit);
	}
}

void AActionProjectileBase::HandleSuccessfulResolvedTargetHit(
	AActor* OtherActor,
	const FActionHitResolveResult& ResolveResult,
	const FVector& ImpactLocation,
	const FVector& ImpactNormal)
{
	++SuccessfulResolvedTargetCount;

	const bool bDestroyBySuccessfulHitPolicy =
		SuccessfulHitPolicy == EActionProjectileSuccessfulHitPolicy::DestroyImmediately;
	const bool bDestroyBySuccessfulHitLimit = HasReachedSuccessfulHitLimit();
	const bool bWillDestroyAfterEvent =
		bDestroyBySuccessfulHitPolicy || bDestroyBySuccessfulHitLimit;
	const EActionProjectileDestroyReason PendingDestroyReason =
		bDestroyBySuccessfulHitPolicy
			? EActionProjectileDestroyReason::SuccessfulHitPolicy
			: (bDestroyBySuccessfulHitLimit
				? EActionProjectileDestroyReason::SuccessfulHitLimitReached
				: EActionProjectileDestroyReason::None);

	const FActionProjectilePresentationEvent HitResolvedEvent = BuildPresentationEvent(
		EActionProjectilePresentationEventType::HitResolved,
		OtherActor,
		ImpactLocation,
		ImpactNormal,
		ResolveResult.ResultType,
		bWillDestroyAfterEvent,
		PendingDestroyReason);
	BroadcastPresentationEvent(HitResolvedEvent);

	if (bWillDestroyAfterEvent)
	{
		DestroyProjectileWithReason(PendingDestroyReason, &HitResolvedEvent);
	}
}

bool AActionProjectileBase::HasReachedSuccessfulHitLimit() const
{
	return MaxSuccessfulTargetHitCount > 0
		&& SuccessfulResolvedTargetCount >= MaxSuccessfulTargetHitCount;
}

const FActionProjectileLifecyclePresentationConfig& AActionProjectileBase::ResolveLifecyclePresentationConfig(
	EActionProjectilePresentationEventType InEventType) const
{
	switch (InEventType)
	{
	case EActionProjectilePresentationEventType::Spawned:
		return SpawnPresentationConfig;

	case EActionProjectilePresentationEventType::WorldBlocked:
		return WorldBlockedPresentationConfig;

	case EActionProjectilePresentationEventType::Destroyed:
		return DestroyedPresentationConfig;

	default:
		break;
	}

	return ActionProjectileRuntimeDebug::GetEmptyLifecyclePresentationConfig();
}

void AActionProjectileBase::ClearHitActors()
{
	HitActors.Reset();
}

void AActionProjectileBase::EnsureProjectileCollisionSlotRegistered()
{
	if (ActionCollisionRuntimeComponent)
	{
		ActionCollisionRuntimeComponent->RegisterCollisionSlot(
			EActionCollisionSlot::ProjectileCollision,
			CollisionComponent,
			TEXT("ProjectileCollision"));
	}
}

void AActionProjectileBase::ApplyProjectileCollisionRuntimePreset()
{
	EnsureProjectileCollisionSlotRegistered();
	if (!ActionCollisionRuntimeComponent)
	{
		return;
	}

	if (ProjectileCollisionOverrideHandle.IsValid())
	{
		ActionCollisionRuntimeComponent->ReleaseCollisionOverride(ProjectileCollisionOverrideHandle);
		ProjectileCollisionOverrideHandle.Reset();
	}

	FActionCollisionOverrideRequest CollisionOverrideRequest;
	CollisionOverrideRequest.Slot = EActionCollisionSlot::ProjectileCollision;
	CollisionOverrideRequest.Preset = EActionCollisionPreset::ProjectilePawnOverlap;
	CollisionOverrideRequest.Priority = 100;
	CollisionOverrideRequest.OwnerReason = TEXT("ProjectileConfig");
	CollisionOverrideRequest.TargetRegistrationNames.Add(TEXT("ProjectileCollision"));

	FActionCollisionChannelOverride WorldStaticOverride;
	WorldStaticOverride.Channel = ECC_WorldStatic;
	WorldStaticOverride.Response =
		WorldBlockPolicy == EActionProjectileWorldBlockPolicy::DestroyOnBlock ? ECR_Block : ECR_Ignore;
	CollisionOverrideRequest.ChannelOverrides.Add(WorldStaticOverride);

	FActionCollisionChannelOverride WorldDynamicOverride;
	WorldDynamicOverride.Channel = ECC_WorldDynamic;
	WorldDynamicOverride.Response =
		WorldBlockPolicy == EActionProjectileWorldBlockPolicy::DestroyOnBlock ? ECR_Block : ECR_Ignore;
	CollisionOverrideRequest.ChannelOverrides.Add(WorldDynamicOverride);

	ProjectileCollisionOverrideHandle =
		ActionCollisionRuntimeComponent->AcquireCollisionOverride(CollisionOverrideRequest);
}

FActionProjectilePresentationEvent AActionProjectileBase::BuildPresentationEvent(
	EActionProjectilePresentationEventType InEventType,
	AActor* InTargetActor,
	const FVector& InImpactLocation,
	const FVector& InImpactNormal,
	EActionHitResultType InHitResultType,
	bool bInWillDestroyAfterEvent,
	EActionProjectileDestroyReason InDestroyReason) const
{
	FActionProjectilePresentationEvent PresentationEvent;
	PresentationEvent.EventType = InEventType;
	PresentationEvent.DestroyReason = InDestroyReason;
	PresentationEvent.ProjectileTag = DamagePayloadTemplate.HitSource.SourceTag;
	PresentationEvent.DamageType = DamagePayloadTemplate.DamageType;
	PresentationEvent.DamageElementTypeTag = DamagePayloadTemplate.DamageElementTypeTag;
	PresentationEvent.SuccessfulHitPolicy = SuccessfulHitPolicy;
	PresentationEvent.WorldBlockPolicy = WorldBlockPolicy;
	PresentationEvent.SuccessfulResolvedTargetCount = SuccessfulResolvedTargetCount;
	PresentationEvent.MaxSuccessfulTargetHitCount = MaxSuccessfulTargetHitCount;
	PresentationEvent.InstigatorActor = DamagePayloadTemplate.InstigatorActor;
	PresentationEvent.TargetActor = InTargetActor;
	PresentationEvent.ImpactLocation = InImpactLocation;
	PresentationEvent.ImpactNormal = InImpactNormal;
	PresentationEvent.HitResultType = InHitResultType;
	PresentationEvent.bWillDestroyAfterEvent = bInWillDestroyAfterEvent;
	PresentationEvent.ResolvedConfigSource = InitializationContext.ResolvedConfigSource;
	PresentationEvent.SelectedProjectileConfigTag = InitializationContext.SelectedProjectileConfigTag;
	PresentationEvent.SpawnSocketName = InitializationContext.SpawnSocketName;
	return PresentationEvent;
}

void AActionProjectileBase::BroadcastPresentationEvent(
	const FActionProjectilePresentationEvent& InPresentationEvent)
{
	LastPresentationEvent = InPresentationEvent;
	ActionProjectileRuntimeDebug::PrintPresentationEvent(this, InPresentationEvent);

	if (UHeroCombatFeedbackComponent* CombatFeedbackComponent =
		ActionProjectileRuntimeDebug::ResolveCombatFeedbackComponent(this))
	{
		CombatFeedbackComponent->HandleProjectilePresentationEvent(
			this,
			InPresentationEvent,
			ResolveLifecyclePresentationConfig(InPresentationEvent.EventType));
	}

	OnProjectilePresentationEvent.Broadcast(InPresentationEvent);
	K2_OnProjectilePresentationEvent(InPresentationEvent);
}

void AActionProjectileBase::DestroyProjectileWithReason(
	EActionProjectileDestroyReason InDestroyReason,
	const FActionProjectilePresentationEvent* InBasePresentationEvent)
{
	DestroyReason = InDestroyReason != EActionProjectileDestroyReason::None
		? InDestroyReason
		: EActionProjectileDestroyReason::ExternalDestroyed;

	if (!bHasBroadcastDestroyedEvent)
	{
		FActionProjectilePresentationEvent DestroyedEvent = InBasePresentationEvent
			? *InBasePresentationEvent
			: BuildPresentationEvent(
				EActionProjectilePresentationEventType::Destroyed,
				nullptr,
				GetActorLocation(),
				FVector::ZeroVector,
				EActionHitResultType::None,
				false,
				DestroyReason);
		DestroyedEvent.EventType = EActionProjectilePresentationEventType::Destroyed;
		DestroyedEvent.DestroyReason = DestroyReason;
		DestroyedEvent.bWillDestroyAfterEvent = false;
		BroadcastPresentationEvent(DestroyedEvent);
		bHasBroadcastDestroyedEvent = true;
	}

	if (!IsActorBeingDestroyed())
	{
		Destroy();
	}
}
