// 文件说明：实现 WeaponBase 相关逻辑。

#include "Items/Weapons/WeaponBase.h"

#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Combat/ActionHitResolver.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/Collision/ActionCollisionRuntimeComponent.h"
#include "Components/Combat/HeroHitSourceComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/PrimitiveComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponBase, Log, All);

namespace WeaponBasePrivate
{
	static bool UsesRepeatedHitPolicy(const FActionHitWindowRuntimeConfig& InConfig)
	{
		return InConfig.UsesIntervalWhileOverlapping();
	}

	static UHeroHitSourceComponent* ResolveOwnerHitSourceComponent(const AWeaponBase* InWeapon)
	{
		if (const AActionHeroCharacter* OwnerHeroCharacter = InWeapon ? Cast<AActionHeroCharacter>(InWeapon->GetOwner()) : nullptr)
		{
			return OwnerHeroCharacter->GetHeroHitSourceComponent();
		}

		return nullptr;
	}
}

AWeaponBase::AWeaponBase()
	: Super()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);

	WeaponCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollisionBox"));
	WeaponCollisionBox->SetupAttachment(WeaponMesh);
	WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponCollisionBox->SetGenerateOverlapEvents(true);

	ActionCollisionRuntimeComponent =
		CreateDefaultSubobject<UActionCollisionRuntimeComponent>(TEXT("ActionCollisionRuntimeComponent"));
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	CacheWeaponHitComponents();
	ApplyWeaponPresentationState(EActionWeaponPresentationState::Holstered);
	EnsureWeaponCollisionSlotsRegistered();
}

void AWeaponBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAttackDetectionEnabled || !WeaponBasePrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig))
	{
		RefreshRepeatedHitTickState();
		return;
	}

	const float CurrentWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float RequiredInterval = ActiveHitWindowRuntimeConfig.RepeatResolveInterval;

	for (auto It = ActiveRepeatedWeaponHitContacts.CreateIterator(); It; ++It)
	{
		FActionActiveHitContactState& ContactState = It.Value();
		if (!ContactState.bIsOverlapping
			|| !ContactState.TargetActor.IsValid()
			|| !ActiveWeaponHitSourceIds.Contains(ContactState.SourceId))
		{
			It.RemoveCurrent();
			continue;
		}

		if (CurrentWorldTime - ContactState.LastResolvedWorldTime < RequiredInterval)
		{
			UE_LOG(
				LogWeaponBase,
				VeryVerbose,
				TEXT("武器重复命中等待间隔：窗口=%s 来源=%s 目标=%s 间隔=%.3f"),
				*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
				*ContactState.SourceId.ToString(),
				*GetNameSafe(ContactState.TargetActor.Get()),
				RequiredInterval);
			continue;
		}

		TryResolveRepeatedWeaponHit(ContactState, TEXT("持续接触重复命中"));
	}

	RefreshRepeatedHitTickState();
}

void AWeaponBase::ApplyWeaponPresentationState(const EActionWeaponPresentationState InPresentationState)
{
	CurrentWeaponPresentationState = InPresentationState;

	switch (InPresentationState)
	{
	case EActionWeaponPresentationState::Holstered:
	case EActionWeaponPresentationState::EquippedPresentation:
		ApplyEquippedPresentationCollisionState();
		return;

	case EActionWeaponPresentationState::AttackDetection:
		ApplyAttackDetectionCollisionState();
		return;

	default:
		return;
	}
}

void AWeaponBase::ApplyEquippedPresentationCollisionState()
{
	// 武器挂在角色身上时，正式语义只有“显示在对应挂点上”。
	// 这里统一关掉 Actor collision gate 与所有 PrimitiveComponent 的世界碰撞，
	// 避免武器 Mesh 或蓝图追加组件把角色移动链挤坏。
	SetActorEnableCollision(false);
	UpdateOwnerCollisionIgnoreState(true);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	RefreshRepeatedHitTickState();
}

void AWeaponBase::ApplyAttackDetectionCollisionState()
{
	// 命中窗口期间只临时打开 Actor collision gate，
	// 真实可参与命中的是后面按命中源名单重新打开的 QueryOnly 命中体，
	// 这里不保留任何 Mesh / Primitive 的世界阻挡碰撞。
	SetActorEnableCollision(true);
	UpdateOwnerCollisionIgnoreState(true);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AWeaponBase::BeginAttackDetection()
{
	FActionHitWindowRuntimeConfig HitWindowRuntimeConfig;
	BeginAttackDetectionForHitSources(
		HitWindowRuntimeConfig,
		{ ActionHitSourceDefaults::GetWeaponCollisionSourceId() });
}

void AWeaponBase::BeginAttackDetectionForHitSources(const TArray<FName>& InHitSourceIds)
{
	FActionHitWindowRuntimeConfig HitWindowRuntimeConfig;
	BeginAttackDetectionForHitSources(HitWindowRuntimeConfig, InHitSourceIds);
}

void AWeaponBase::BeginAttackDetectionForHitSources(
	const FActionHitWindowRuntimeConfig& InHitWindowRuntimeConfig,
	const TArray<FName>& InHitSourceIds)
{
	CacheWeaponHitComponents();
	EnsureWeaponCollisionSlotsRegistered();
	ClearHitActorsThisSwing();
	ActiveRepeatedWeaponHitContacts.Reset();
	ActiveWeaponCollisionOverrideHandles.Reset();
	ActiveWeaponHitSourceIds.Reset();
	ActiveHitWindowRuntimeConfig = InHitWindowRuntimeConfig;

	for (const FName& SourceId : InHitSourceIds)
	{
		if (SourceId != NAME_None)
		{
			ActiveWeaponHitSourceIds.Add(SourceId);
		}
	}

	if (ActiveWeaponHitSourceIds.Num() == 0)
	{
		ActiveWeaponHitSourceIds.Add(ActionHitSourceDefaults::GetWeaponCollisionSourceId());
	}

	bAttackDetectionEnabled = true;
	ApplyWeaponPresentationState(EActionWeaponPresentationState::AttackDetection);

	if (ActionCollisionRuntimeComponent)
	{
		ActionCollisionRuntimeComponent->ReleaseCollisionOverridesByReason(TEXT("WeaponAttackDetection"));
		for (const TPair<FName, TWeakObjectPtr<UPrimitiveComponent>>& Pair : WeaponHitComponentsBySourceId)
		{
			if (!ActiveWeaponHitSourceIds.Contains(Pair.Key) || !Pair.Value.IsValid())
			{
				continue;
			}

			FActionCollisionOverrideRequest CollisionOverrideRequest;
			CollisionOverrideRequest.Slot = EActionCollisionSlot::WeaponHit;
			CollisionOverrideRequest.Preset = EActionCollisionPreset::HitQueryPawnOverlap;
			CollisionOverrideRequest.Priority = 100;
			CollisionOverrideRequest.OwnerReason = TEXT("WeaponAttackDetection");
			CollisionOverrideRequest.TargetRegistrationNames.Add(Pair.Key);
			ActiveWeaponCollisionOverrideHandles.Add(
				Pair.Key,
				ActionCollisionRuntimeComponent->AcquireCollisionOverride(CollisionOverrideRequest));
		}
	}

	RefreshRepeatedHitTickState();

	if (UHeroHitSourceComponent* HitSourceComponent = WeaponBasePrivate::ResolveOwnerHitSourceComponent(this))
	{
		HitSourceComponent->PrintHitWindowEventDebug(
			FString::Printf(
				TEXT("[武器命中窗口] 开启: 窗口=%s 武器来源=[%s]"),
				*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
				*FString::JoinBy(ActiveWeaponHitSourceIds.Array(), TEXT(", "), [](const FName& SourceId)
				{
					return SourceId.ToString();
				})),
			FColor::Cyan,
			2.0f);
	}
}

void AWeaponBase::EndAttackDetection()
{
	const FString ClosingWindowName = ActiveHitWindowRuntimeConfig.WindowName.ToString();

	bAttackDetectionEnabled = false;
	ActiveWeaponCollisionOverrideHandles.Reset();
	if (ActionCollisionRuntimeComponent)
	{
		ActionCollisionRuntimeComponent->ReleaseCollisionOverridesByReason(TEXT("WeaponAttackDetection"));
	}

	for (const TPair<FWeaponHitRegistrationKey, FActionActiveHitContactState>& Pair : ActiveRepeatedWeaponHitContacts)
	{
		UE_LOG(
			LogWeaponBase,
			Log,
			TEXT("武器命中窗口停止：窗口=%s 来源=%s 目标=%s 事件=因窗口关闭或接触结束被停止"),
			*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
			*Pair.Key.SourceId.ToString(),
			*GetNameSafe(Pair.Key.TargetActor.Get()));
	}

	ActiveWeaponHitSourceIds.Reset();
	ActiveHitWindowRuntimeConfig = FActionHitWindowRuntimeConfig();
	ActiveRepeatedWeaponHitContacts.Reset();
	ClearHitActorsThisSwing();
	ApplyWeaponPresentationState(EActionWeaponPresentationState::EquippedPresentation);
	RefreshRepeatedHitTickState();

	if (UHeroHitSourceComponent* HitSourceComponent = WeaponBasePrivate::ResolveOwnerHitSourceComponent(this))
	{
		HitSourceComponent->PrintHitWindowEventDebug(
			FString::Printf(TEXT("[武器命中窗口] 关闭: 窗口=%s"), *ClosingWindowName),
			FColor::Silver,
			2.0f);
	}
}

FActionDamagePayload AWeaponBase::BuildDamagePayload(AActor* OtherActor) const
{
	return BuildDamagePayloadForHitSource(
		OtherActor,
		ActionHitSourceDefaults::GetWeaponCollisionSourceId(),
		WeaponCollisionBox);
}

FActionDamagePayload AWeaponBase::BuildDamagePayloadForHitSource(
	AActor* OtherActor,
	const FName InSourceId,
	const UPrimitiveComponent* InSourceComponent) const
{
	FActionDamagePayload DamagePayload;
	DamagePayload.InstigatorActor = GetOwner();
	DamagePayload.SourceActor = const_cast<AWeaponBase*>(this);
	DamagePayload.HitSource.SourceId = InSourceId;
	DamagePayload.HitSource.SourceType = EActionHitSourceType::WeaponCollision;
	DamagePayload.HitSource.SourceComponentName = InSourceComponent
		? InSourceComponent->GetFName()
		: NAME_None;

	if (IsValid(OtherActor))
	{
		// 冲击方向由武器命中体指向目标位置差计算，供受击朝向与后续位移反馈复用。
		DamagePayload.ImpactDirection = (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	}

	return DamagePayload;
}

bool AWeaponBase::TryRegisterHitTargetForCurrentAttack(const FName InSourceId, AActor* OtherActor)
{
	return TryRegisterSingleHitTargetForCurrentAttack(InSourceId, OtherActor);
}

bool AWeaponBase::TryRegisterSingleHitTargetForCurrentAttack(const FName InSourceId, AActor* OtherActor)
{
	if (!IsValid(OtherActor) || InSourceId == NAME_None)
	{
		return false;
	}

	if (AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(GetOwner()))
	{
		if (UHeroHitSourceComponent* HitSourceComponent = OwnerHeroCharacter->GetHeroHitSourceComponent())
		{
			if (HitSourceComponent->IsHitWindowActive())
			{
				return HitSourceComponent->TryRegisterSingleHitBySource(InSourceId, OtherActor);
			}
		}
	}

	FWeaponHitRegistrationKey RegistrationKey;
	RegistrationKey.SourceId = InSourceId;
	RegistrationKey.TargetActor = OtherActor;
	if (HitActorsThisSwing.Contains(RegistrationKey))
	{
		return false;
	}

	HitActorsThisSwing.Add(RegistrationKey);
	return true;
}

bool AWeaponBase::TryBeginRepeatedHitContact(
	const FName InSourceId,
	const FName InSourceComponentName,
	AActor* OtherActor)
{
	if (!IsValid(OtherActor)
		|| InSourceId == NAME_None
		|| !bAttackDetectionEnabled
		|| !ActiveWeaponHitSourceIds.Contains(InSourceId)
		|| !WeaponBasePrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig))
	{
		return false;
	}

	FWeaponHitRegistrationKey RegistrationKey;
	RegistrationKey.SourceId = InSourceId;
	RegistrationKey.TargetActor = OtherActor;

	if (FActionActiveHitContactState* ExistingState = ActiveRepeatedWeaponHitContacts.Find(RegistrationKey))
	{
		ExistingState->bIsOverlapping = true;
		ExistingState->SourceComponentName = InSourceComponentName;
		return false;
	}

	FActionActiveHitContactState ContactState;
	ContactState.SourceId = InSourceId;
	ContactState.TargetActor = OtherActor;
	ContactState.SourceComponentName = InSourceComponentName;
	ContactState.LastResolvedWorldTime = -BIG_NUMBER;
	ContactState.bIsOverlapping = true;
	ActiveRepeatedWeaponHitContacts.Add(RegistrationKey, ContactState);
	return true;
}

void AWeaponBase::OnWeaponHitComponentBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bAttackDetectionEnabled
		|| !IsValid(OverlappedComponent)
		|| !IsValid(OtherActor)
		|| OtherActor == this
		|| OtherActor == GetOwner())
	{
		return;
	}

	FName SourceId = NAME_None;
	if (!TryResolveWeaponHitSourceIdByComponent(OverlappedComponent, SourceId))
	{
		return;
	}

	if (!ActiveWeaponHitSourceIds.Contains(SourceId))
	{
		return;
	}

	if (WeaponBasePrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig))
	{
		if (TryBeginRepeatedHitContact(SourceId, OverlappedComponent->GetFName(), OtherActor))
		{
			FWeaponHitRegistrationKey ContactKey;
			ContactKey.SourceId = SourceId;
			ContactKey.TargetActor = OtherActor;
			if (FActionActiveHitContactState* ContactState = ActiveRepeatedWeaponHitContacts.Find(ContactKey))
			{
				TryResolveRepeatedWeaponHit(*ContactState, TEXT("首次接触命中"));
			}
		}

		RefreshRepeatedHitTickState();
		return;
	}

	if (!TryRegisterSingleHitTargetForCurrentAttack(SourceId, OtherActor))
	{
		return;
	}

	const FActionDamagePayload DamagePayload =
		BuildDamagePayloadForHitSource(OtherActor, SourceId, OverlappedComponent);
	const FActionHitResolveResult ResolveResult = UActionHitResolver::ResolveHit(OtherActor, DamagePayload);
	if (!ResolveResult.WasResolved())
	{
		return;
	}

	RewardInstigatorSpecialWeaponSwitchEnergy(DamagePayload, ResolveResult);
	UE_LOG(
		LogWeaponBase,
		Log,
		TEXT("武器命中窗口结算：窗口=%s 策略=单次 来源=%s 目标=%s 事件=首次接触命中"),
		*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
		*SourceId.ToString(),
		*GetNameSafe(OtherActor));

	if (UHeroHitSourceComponent* HitSourceComponent = WeaponBasePrivate::ResolveOwnerHitSourceComponent(this))
	{
		HitSourceComponent->PrintHitWindowEventDebug(
			FString::Printf(
				TEXT("[武器命中窗口] 首次命中: 窗口=%s 来源=%s 目标=%s"),
				*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
				*SourceId.ToString(),
				*GetNameSafe(OtherActor)),
			FColor::Green);
	}
}

void AWeaponBase::OnWeaponHitComponentEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!IsValid(OverlappedComponent) || !IsValid(OtherActor))
	{
		return;
	}

	FName SourceId = NAME_None;
	if (!TryResolveWeaponHitSourceIdByComponent(OverlappedComponent, SourceId))
	{
		return;
	}

	RemoveRepeatedWeaponHitContact(SourceId, OtherActor);
	RefreshRepeatedHitTickState();
}

void AWeaponBase::ClearHitActorsThisSwing()
{
	HitActorsThisSwing.Reset();
}

void AWeaponBase::CacheWeaponHitComponents()
{
	WeaponHitComponentsBySourceId.Reset();

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		FName ResolvedSourceId = NAME_None;
		if (!TryResolveWeaponHitSourceIdByComponent(PrimitiveComponent, ResolvedSourceId))
		{
			continue;
		}

		WeaponHitComponentsBySourceId.Add(ResolvedSourceId, PrimitiveComponent);
		PrimitiveComponent->SetGenerateOverlapEvents(true);
		PrimitiveComponent->OnComponentBeginOverlap.AddUniqueDynamic(
			this,
			&ThisClass::OnWeaponHitComponentBeginOverlap);
		PrimitiveComponent->OnComponentEndOverlap.AddUniqueDynamic(
			this,
			&ThisClass::OnWeaponHitComponentEndOverlap);
	}
}

void AWeaponBase::RefreshRepeatedHitTickState()
{
	const bool bShouldTick =
		bAttackDetectionEnabled
		&& WeaponBasePrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig)
		&& ActiveRepeatedWeaponHitContacts.Num() > 0;
	SetActorTickEnabled(bShouldTick);
}

void AWeaponBase::EnsureWeaponCollisionSlotsRegistered()
{
	if (!ActionCollisionRuntimeComponent)
	{
		return;
	}

	for (const TPair<FName, TWeakObjectPtr<UPrimitiveComponent>>& Pair : WeaponHitComponentsBySourceId)
	{
		ActionCollisionRuntimeComponent->RegisterCollisionSlot(
			EActionCollisionSlot::WeaponHit,
			Pair.Value.Get(),
			Pair.Key);
	}
}

void AWeaponBase::UpdateOwnerCollisionIgnoreState(const bool bShouldIgnoreOwner)
{
	AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->IgnoreActorWhenMoving(OwnerCharacter, bShouldIgnoreOwner);
	}

	if (UCapsuleComponent* OwnerCapsule = OwnerCharacter->GetCapsuleComponent())
	{
		OwnerCapsule->IgnoreActorWhenMoving(this, bShouldIgnoreOwner);
	}
}

bool AWeaponBase::TryResolveWeaponHitSourceIdByComponent(
	const UPrimitiveComponent* InHitComponent,
	FName& OutSourceId) const
{
	OutSourceId = NAME_None;

	if (!IsValid(InHitComponent))
	{
		return false;
	}

	if (const AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(GetOwner()))
	{
		if (const UHeroHitSourceComponent* HitSourceComponent = OwnerHeroCharacter->GetHeroHitSourceComponent())
		{
			if (HitSourceComponent->TryResolveRegisteredHitSourceIdByComponentName(
				InHitComponent->GetFName(),
				EActionHitSourceType::WeaponCollision,
				OutSourceId))
			{
				return true;
			}
		}
	}

	if (InHitComponent == WeaponCollisionBox)
	{
		OutSourceId = ActionHitSourceDefaults::GetWeaponCollisionSourceId();
		return true;
	}

	return false;
}

bool AWeaponBase::TryResolveRepeatedWeaponHit(
	FActionActiveHitContactState& InOutContactState,
	const TCHAR* InDebugResolveReason)
{
	if (!InOutContactState.TargetActor.IsValid()
		|| !bAttackDetectionEnabled
		|| !WeaponBasePrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig)
		|| !ActiveWeaponHitSourceIds.Contains(InOutContactState.SourceId))
	{
		return false;
	}

	UPrimitiveComponent* SourceComponent = nullptr;
	if (const TWeakObjectPtr<UPrimitiveComponent>* FoundComponent = WeaponHitComponentsBySourceId.Find(InOutContactState.SourceId))
	{
		SourceComponent = FoundComponent->Get();
	}

	const FActionDamagePayload DamagePayload =
		BuildDamagePayloadForHitSource(InOutContactState.TargetActor.Get(), InOutContactState.SourceId, SourceComponent);
	const FActionHitResolveResult ResolveResult =
		UActionHitResolver::ResolveHit(InOutContactState.TargetActor.Get(), DamagePayload);
	InOutContactState.LastResolvedWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	UE_LOG(
		LogWeaponBase,
		Log,
		TEXT("武器命中窗口结算：窗口=%s 策略=持续接触间隔 来源=%s 目标=%s 间隔=%.3f 事件=%s"),
		*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
		*InOutContactState.SourceId.ToString(),
		*GetNameSafe(InOutContactState.TargetActor.Get()),
		ActiveHitWindowRuntimeConfig.RepeatResolveInterval,
		InDebugResolveReason);

	if (!ResolveResult.WasResolved())
	{
		return false;
	}

	RewardInstigatorSpecialWeaponSwitchEnergy(DamagePayload, ResolveResult);

	if (UHeroHitSourceComponent* HitSourceComponent = WeaponBasePrivate::ResolveOwnerHitSourceComponent(this))
	{
		HitSourceComponent->PrintHitWindowEventDebug(
			FString::Printf(
				TEXT("[武器命中窗口] %s: 窗口=%s 来源=%s 目标=%s 间隔=%.2f"),
				InDebugResolveReason,
				*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
				*InOutContactState.SourceId.ToString(),
				*GetNameSafe(InOutContactState.TargetActor.Get()),
				ActiveHitWindowRuntimeConfig.RepeatResolveInterval),
			FColor::Orange);
	}

	return true;
}

void AWeaponBase::RemoveRepeatedWeaponHitContact(const FName InSourceId, AActor* InTargetActor)
{
	if (InSourceId == NAME_None || !IsValid(InTargetActor))
	{
		return;
	}

	FWeaponHitRegistrationKey RegistrationKey;
	RegistrationKey.SourceId = InSourceId;
	RegistrationKey.TargetActor = InTargetActor;
	if (ActiveRepeatedWeaponHitContacts.Remove(RegistrationKey) > 0)
	{
		UE_LOG(
			LogWeaponBase,
			Log,
			TEXT("武器命中窗口停止：窗口=%s 来源=%s 目标=%s 事件=因窗口关闭或接触结束被停止"),
			*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
			*InSourceId.ToString(),
			*GetNameSafe(InTargetActor));

		if (UHeroHitSourceComponent* HitSourceComponent = WeaponBasePrivate::ResolveOwnerHitSourceComponent(this))
		{
			HitSourceComponent->PrintHitWindowEventDebug(
				FString::Printf(
					TEXT("[武器命中窗口] 停止接触: 窗口=%s 来源=%s 目标=%s"),
					*ActiveHitWindowRuntimeConfig.WindowName.ToString(),
					*InSourceId.ToString(),
					*GetNameSafe(InTargetActor)),
				FColor::Silver,
				1.5f);
		}
	}
}

void AWeaponBase::RewardInstigatorSpecialWeaponSwitchEnergy(
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

	AActionHeroCharacter* InstigatorHero = Cast<AActionHeroCharacter>(GetOwner());
	if (!InstigatorHero)
	{
		return;
	}

	if (UHeroEquipmentComponent* EquipmentComponent = InstigatorHero->FindComponentByClass<UHeroEquipmentComponent>())
	{
		// 命中、压防和崩防都会积累特殊切武所需能量，未命中与被完反则不奖励。
		EquipmentComponent->AddSpecialWeaponSwitchEnergy(DamagePayload.SpecialWeaponSwitchEnergyRewardOnHit);
	}
}
