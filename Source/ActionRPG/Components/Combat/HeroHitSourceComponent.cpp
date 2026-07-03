#include "Components/Combat/HeroHitSourceComponent.h"

#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Combat/ActionHitResolver.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Collision/ActionCollisionRuntimeComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Debug/ActionDebugHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroHitSourceComponent, Log, All);

namespace HeroHitSourceComponentPrivate
{
	static bool UsesRepeatedHitPolicy(const FActionHitWindowRuntimeConfig& InConfig)
	{
		return InConfig.UsesIntervalWhileOverlapping();
	}

	static FString ResolvePolicyDebugText(const EActionHitWindowResolvePolicy InResolvePolicy)
	{
		switch (InResolvePolicy)
		{
		case EActionHitWindowResolvePolicy::SingleHitPerSourceTarget:
			return TEXT("单次结算");

		case EActionHitWindowResolvePolicy::IntervalWhileOverlapping:
			return TEXT("持续接触间隔结算");

		default:
			return TEXT("未知策略");
		}
	}

	static FString JoinNameArrayDebugText(const TArray<FName>& InNames)
	{
		if (InNames.Num() <= 0)
		{
			return TEXT("无");
		}

		TArray<FString> Parts;
		Parts.Reserve(InNames.Num());
		for (const FName& Name : InNames)
		{
			Parts.Add(Name != NAME_None ? Name.ToString() : TEXT("None"));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	static FString JoinActiveSourceSetDebugText(const TSet<FName>& InNames)
	{
		if (InNames.Num() <= 0)
		{
			return TEXT("无");
		}

		TArray<FString> Parts;
		Parts.Reserve(InNames.Num());
		for (const FName& Name : InNames)
		{
			Parts.Add(Name != NAME_None ? Name.ToString() : TEXT("None"));
		}

		Parts.Sort();
		return FString::Join(Parts, TEXT(", "));
	}
}

UHeroHitSourceComponent::UHeroHitSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetComponentTickEnabled(false);
}

void UHeroHitSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	RegisterBuiltInHitSources();
	CacheAndBindOwnedBodyHitComponents();
	UpdateOwnedBodyHitComponentCollisionState();
}

void UHeroHitSourceComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsHitWindowActive()
		|| !HeroHitSourceComponentPrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig))
	{
		RefreshRepeatedHitTickState();
		return;
	}

	const float CurrentWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float RequiredInterval = ActiveHitWindowRuntimeConfig.RepeatResolveInterval;

	for (auto It = ActiveRepeatedBodyHitContacts.CreateIterator(); It; ++It)
	{
		FActionActiveHitContactState& ContactState = It.Value();
		if (!ContactState.bIsOverlapping || !ContactState.TargetActor.IsValid() || !IsHitSourceActive(ContactState.SourceId))
		{
			It.RemoveCurrent();
			continue;
		}

		if (CurrentWorldTime - ContactState.LastResolvedWorldTime < RequiredInterval)
		{
			UE_LOG(
				LogHeroHitSourceComponent,
				VeryVerbose,
				TEXT("重复命中等待间隔：窗口=%s 来源=%s 目标=%s 间隔=%.3f"),
				*ActiveHitWindowName.ToString(),
				*ContactState.SourceId.ToString(),
				*GetNameSafe(ContactState.TargetActor.Get()),
				RequiredInterval);
			continue;
		}

		TryResolveRepeatedBodyHit(ContactState, TEXT("持续接触重复命中"));
	}

	RefreshRepeatedHitTickState();
}

int32 UHeroHitSourceComponent::BeginHitWindow(const FActionHitWindowRuntimeConfig& InWindowRuntimeConfig)
{
	ActiveHitWindowHandle = NextHitWindowHandle++;
	ActiveHitWindowRuntimeConfig = InWindowRuntimeConfig;
	ActiveHitWindowName = InWindowRuntimeConfig.WindowName;
	bUseWeaponCollisionDetection = InWindowRuntimeConfig.bUseWeaponCollisionDetection;
	RefreshActiveHitSources(
		InWindowRuntimeConfig.EnabledHitSourceIds,
		InWindowRuntimeConfig.EnabledHitSourceGroupIds);
	RegisteredHitsInCurrentWindow.Reset();
	ActiveRepeatedBodyHitContacts.Reset();
	UpdateOwnedBodyHitComponentCollisionState();
	RefreshRepeatedHitTickState();
	PrintHitWindowEventDebug(
		FString::Printf(TEXT("[命中窗口] 开启: %s"), *DescribeCurrentHitWindowDebug()),
		FColor::Cyan,
		2.5f);
	return ActiveHitWindowHandle;
}

int32 UHeroHitSourceComponent::BeginHitWindow(const FName InWindowName, const bool bInUseWeaponCollisionDetection)
{
	FActionHitWindowRuntimeConfig WindowRuntimeConfig;
	WindowRuntimeConfig.WindowName = InWindowName;
	WindowRuntimeConfig.bUseWeaponCollisionDetection = bInUseWeaponCollisionDetection;
	return BeginHitWindow(WindowRuntimeConfig);
}

int32 UHeroHitSourceComponent::BeginHitWindow(
	const FName InWindowName,
	const bool bInUseWeaponCollisionDetection,
	const TArray<FName>& InEnabledHitSourceIds)
{
	FActionHitWindowRuntimeConfig WindowRuntimeConfig;
	WindowRuntimeConfig.WindowName = InWindowName;
	WindowRuntimeConfig.bUseWeaponCollisionDetection = bInUseWeaponCollisionDetection;
	WindowRuntimeConfig.EnabledHitSourceIds = InEnabledHitSourceIds;
	return BeginHitWindow(WindowRuntimeConfig);
}

int32 UHeroHitSourceComponent::BeginHitWindow(
	const FName InWindowName,
	const bool bInUseWeaponCollisionDetection,
	const TArray<FName>& InEnabledHitSourceIds,
	const TArray<FName>& InEnabledHitSourceGroupIds)
{
	FActionHitWindowRuntimeConfig WindowRuntimeConfig;
	WindowRuntimeConfig.WindowName = InWindowName;
	WindowRuntimeConfig.bUseWeaponCollisionDetection = bInUseWeaponCollisionDetection;
	WindowRuntimeConfig.EnabledHitSourceIds = InEnabledHitSourceIds;
	WindowRuntimeConfig.EnabledHitSourceGroupIds = InEnabledHitSourceGroupIds;
	return BeginHitWindow(WindowRuntimeConfig);
}

void UHeroHitSourceComponent::EndHitWindow(const int32 InWindowHandle)
{
	if (InWindowHandle == INDEX_NONE || ActiveHitWindowHandle != InWindowHandle)
	{
		return;
	}

	ResetHitWindowRuntime();
}

void UHeroHitSourceComponent::ResetHitWindowRuntime()
{
	const FString ClosingWindowName =
		ActiveHitWindowName != NAME_None ? ActiveHitWindowName.ToString() : TEXT("None");

	for (const TPair<FActionHitSourceRegistrationKey, FActionActiveHitContactState>& Pair : ActiveRepeatedBodyHitContacts)
	{
		UE_LOG(
			LogHeroHitSourceComponent,
			Log,
			TEXT("命中窗口停止：窗口=%s 来源=%s 目标=%s 事件=因窗口关闭或接触结束被停止"),
			*ActiveHitWindowName.ToString(),
			*Pair.Key.SourceId.ToString(),
			*GetNameSafe(Pair.Key.TargetActor.Get()));
	}

	ActiveHitWindowHandle = INDEX_NONE;
	ActiveHitWindowName = NAME_None;
	bUseWeaponCollisionDetection = false;
	ActiveHitWindowRuntimeConfig = FActionHitWindowRuntimeConfig();
	ActiveHitSourcesInCurrentWindow.Reset();
	RegisteredHitsInCurrentWindow.Reset();
	ActiveRepeatedBodyHitContacts.Reset();
	ActiveOwnedBodyCollisionOverrideHandles.Reset();
	UpdateOwnedBodyHitComponentCollisionState();
	RefreshRepeatedHitTickState();
	PrintHitWindowEventDebug(
		FString::Printf(TEXT("[命中窗口] 关闭: 窗口=%s"), *ClosingWindowName),
		FColor::Silver,
		2.0f);
}

FString UHeroHitSourceComponent::DescribeCurrentHitWindowDebug() const
{
	const FString WindowName =
		ActiveHitWindowName != NAME_None ? ActiveHitWindowName.ToString() : TEXT("None");

	return FString::Printf(
		TEXT("窗口=%s 句柄=%d 策略=%s 间隔=%.2f 武器碰撞=%s 直接来源=[%s] 来源组=[%s] 当前激活=[%s]"),
		*WindowName,
		ActiveHitWindowHandle,
		*HeroHitSourceComponentPrivate::ResolvePolicyDebugText(ActiveHitWindowRuntimeConfig.ResolvePolicy),
		ActiveHitWindowRuntimeConfig.RepeatResolveInterval,
		ActiveHitWindowRuntimeConfig.bUseWeaponCollisionDetection ? TEXT("开") : TEXT("关"),
		*HeroHitSourceComponentPrivate::JoinNameArrayDebugText(ActiveHitWindowRuntimeConfig.EnabledHitSourceIds),
		*HeroHitSourceComponentPrivate::JoinNameArrayDebugText(ActiveHitWindowRuntimeConfig.EnabledHitSourceGroupIds),
		*HeroHitSourceComponentPrivate::JoinActiveSourceSetDebugText(ActiveHitSourcesInCurrentWindow));
}

void UHeroHitSourceComponent::PrintCurrentHitWindowDebug() const
{
	PrintHitWindowEventDebug(FString::Printf(TEXT("[命中窗口] %s"), *DescribeCurrentHitWindowDebug()), FColor::Cyan, 4.0f);
}

void UHeroHitSourceComponent::PrintHitWindowEventDebug(
	const FString& InMessage,
	const FColor& InColor,
	const float InDuration) const
{
	if (!bEnableHitWindowScreenDebug || InMessage.IsEmpty())
	{
		return;
	}

	Debug::Print(InMessage, InColor, InDuration);
}

bool UHeroHitSourceComponent::IsHitSourceActive(const FName InSourceId) const
{
	if (!IsHitWindowActive() || InSourceId == NAME_None)
	{
		return false;
	}

	return ActiveHitSourcesInCurrentWindow.Contains(InSourceId);
}

void UHeroHitSourceComponent::GetActiveHitSourceIds(TArray<FName>& OutHitSourceIds) const
{
	OutHitSourceIds.Reset();
	for (const FName& SourceId : ActiveHitSourcesInCurrentWindow)
	{
		OutHitSourceIds.Add(SourceId);
	}
}

void UHeroHitSourceComponent::GetActiveHitSourceIdsByType(
	const EActionHitSourceType InSourceType,
	TArray<FName>& OutHitSourceIds) const
{
	OutHitSourceIds.Reset();

	if (InSourceType == EActionHitSourceType::None)
	{
		return;
	}

	for (const FName& SourceId : ActiveHitSourcesInCurrentWindow)
	{
		FActionHitSourceDefinition Definition;
		if (TryGetRegisteredHitSource(SourceId, Definition) && Definition.SourceType == InSourceType)
		{
			OutHitSourceIds.Add(SourceId);
		}
	}
}

void UHeroHitSourceComponent::GetActiveHitSourceDefinitions(TArray<FActionHitSourceDefinition>& OutHitSourceDefinitions) const
{
	OutHitSourceDefinitions.Reset();

	for (const FName& SourceId : ActiveHitSourcesInCurrentWindow)
	{
		FActionHitSourceDefinition Definition;
		if (TryGetRegisteredHitSource(SourceId, Definition))
		{
			OutHitSourceDefinitions.Add(Definition);
		}
	}
}

bool UHeroHitSourceComponent::TryGetFirstActiveHitSourceByType(
	const EActionHitSourceType InSourceType,
	FActionHitSourceDefinition& OutDefinition) const
{
	OutDefinition = FActionHitSourceDefinition();

	if (InSourceType == EActionHitSourceType::None)
	{
		return false;
	}

	for (const FName& SourceId : ActiveHitSourcesInCurrentWindow)
	{
		FActionHitSourceDefinition Definition;
		if (TryGetRegisteredHitSource(SourceId, Definition) && Definition.SourceType == InSourceType)
		{
			OutDefinition = Definition;
			return true;
		}
	}

	return false;
}

bool UHeroHitSourceComponent::CanRegisterHit(AActor* InTargetActor) const
{
	return CanRegisterHitBySource(ActionHitSourceDefaults::GetWeaponCollisionSourceId(), InTargetActor);
}

bool UHeroHitSourceComponent::CanRegisterHitBySource(const FName InSourceId, AActor* InTargetActor) const
{
	if (!IsHitWindowActive() || InSourceId == NAME_None || !IsValid(InTargetActor))
	{
		return false;
	}

	if (!IsHitSourceActive(InSourceId))
	{
		return false;
	}

	FActionHitSourceRegistrationKey RegistrationKey;
	RegistrationKey.SourceId = InSourceId;
	RegistrationKey.TargetActor = InTargetActor;
	return !RegisteredHitsInCurrentWindow.Contains(RegistrationKey);
}

bool UHeroHitSourceComponent::TryRegisterHit(AActor* InTargetActor)
{
	return TryRegisterSingleHitBySource(ActionHitSourceDefaults::GetWeaponCollisionSourceId(), InTargetActor);
}

bool UHeroHitSourceComponent::TryRegisterHitBySource(const FName InSourceId, AActor* InTargetActor)
{
	return TryRegisterSingleHitBySource(InSourceId, InTargetActor);
}

bool UHeroHitSourceComponent::TryRegisterSingleHitBySource(const FName InSourceId, AActor* InTargetActor)
{
	if (!CanRegisterHitBySource(InSourceId, InTargetActor))
	{
		return false;
	}

	FActionHitSourceRegistrationKey RegistrationKey;
	RegistrationKey.SourceId = InSourceId;
	RegistrationKey.TargetActor = InTargetActor;
	RegisteredHitsInCurrentWindow.Add(RegistrationKey);
	return true;
}

bool UHeroHitSourceComponent::TryBeginRepeatedHitContact(
	const FName InSourceId,
	const FName InSourceComponentName,
	AActor* InTargetActor)
{
	if (!IsHitWindowActive()
		|| !HeroHitSourceComponentPrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig)
		|| InSourceId == NAME_None
		|| !IsValid(InTargetActor)
		|| !IsHitSourceActive(InSourceId))
	{
		return false;
	}

	FActionHitSourceRegistrationKey RegistrationKey;
	RegistrationKey.SourceId = InSourceId;
	RegistrationKey.TargetActor = InTargetActor;

	if (FActionActiveHitContactState* ExistingState = ActiveRepeatedBodyHitContacts.Find(RegistrationKey))
	{
		ExistingState->bIsOverlapping = true;
		ExistingState->SourceComponentName = InSourceComponentName;
		return false;
	}

	FActionActiveHitContactState ContactState;
	ContactState.SourceId = InSourceId;
	ContactState.TargetActor = InTargetActor;
	ContactState.SourceComponentName = InSourceComponentName;
	ContactState.LastResolvedWorldTime = -BIG_NUMBER;
	ContactState.bIsOverlapping = true;
	ActiveRepeatedBodyHitContacts.Add(RegistrationKey, ContactState);
	return true;
}

bool UHeroHitSourceComponent::RegisterOrUpdateHitSource(const FActionHitSourceDefinition& InDefinition)
{
	if (!InDefinition.IsValidDefinition())
	{
		return false;
	}

	RegisteredHitSources.Add(InDefinition.SourceId, InDefinition);
	return true;
}

void UHeroHitSourceComponent::UnregisterHitSource(const FName InSourceId)
{
	if (InSourceId == NAME_None)
	{
		return;
	}

	RegisteredHitSources.Remove(InSourceId);
	ActiveHitSourcesInCurrentWindow.Remove(InSourceId);
	OwnedBodyHitComponentsBySourceId.Remove(InSourceId);

	for (auto It = RegisteredHitsInCurrentWindow.CreateIterator(); It; ++It)
	{
		if (It->SourceId == InSourceId)
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = ActiveRepeatedBodyHitContacts.CreateIterator(); It; ++It)
	{
		if (It->Key.SourceId == InSourceId)
		{
			It.RemoveCurrent();
		}
	}

	RefreshRepeatedHitTickState();
}

bool UHeroHitSourceComponent::TryGetRegisteredHitSource(
	const FName InSourceId,
	FActionHitSourceDefinition& OutDefinition) const
{
	OutDefinition = FActionHitSourceDefinition();

	if (InSourceId == NAME_None)
	{
		return false;
	}

	if (const FActionHitSourceDefinition* FoundDefinition = RegisteredHitSources.Find(InSourceId))
	{
		OutDefinition = *FoundDefinition;
		return OutDefinition.IsValidDefinition();
	}

	return false;
}

bool UHeroHitSourceComponent::TryFillHitSourceInfoFromRegistration(
	const FName InSourceId,
	FActionHitSourceInfo& InOutHitSourceInfo) const
{
	FActionHitSourceDefinition Definition;
	if (!TryGetRegisteredHitSource(InSourceId, Definition))
	{
		return false;
	}

	InOutHitSourceInfo.SourceId = Definition.SourceId;
	InOutHitSourceInfo.SourceType = Definition.SourceType;

	if (Definition.SourceTag.IsValid())
	{
		InOutHitSourceInfo.SourceTag = Definition.SourceTag;
	}

	if (Definition.SourceComponentName != NAME_None)
	{
		InOutHitSourceInfo.SourceComponentName = Definition.SourceComponentName;
	}

	if (Definition.SourceSocketName != NAME_None)
	{
		InOutHitSourceInfo.SourceSocketName = Definition.SourceSocketName;
	}

	return true;
}

bool UHeroHitSourceComponent::TryResolveRegisteredHitSourceIdByComponentName(
	const FName InComponentName,
	const EActionHitSourceType InRequiredSourceType,
	FName& OutSourceId) const
{
	OutSourceId = NAME_None;

	if (InComponentName == NAME_None || InRequiredSourceType == EActionHitSourceType::None)
	{
		return false;
	}

	for (const TPair<FName, FActionHitSourceDefinition>& Pair : RegisteredHitSources)
	{
		const FActionHitSourceDefinition& Definition = Pair.Value;
		if (Definition.SourceType == InRequiredSourceType
			&& Definition.SourceComponentName == InComponentName)
		{
			OutSourceId = Definition.SourceId;
			return true;
		}
	}

	return false;
}

bool UHeroHitSourceComponent::RegisterOrUpdateHitSourceGroup(const FActionHitSourceGroupDefinition& InDefinition)
{
	if (!InDefinition.IsValidDefinition())
	{
		return false;
	}

	RegisteredHitSourceGroups.Add(InDefinition.GroupId, InDefinition);
	return true;
}

bool UHeroHitSourceComponent::TryGetRegisteredHitSourceGroup(
	const FName InGroupId,
	FActionHitSourceGroupDefinition& OutDefinition) const
{
	OutDefinition = FActionHitSourceGroupDefinition();

	if (InGroupId == NAME_None)
	{
		return false;
	}

	if (const FActionHitSourceGroupDefinition* FoundDefinition = RegisteredHitSourceGroups.Find(InGroupId))
	{
		OutDefinition = *FoundDefinition;
		return OutDefinition.IsValidDefinition();
	}

	return false;
}

void UHeroHitSourceComponent::RefreshActiveHitSources(const TArray<FName>& InEnabledHitSourceIds)
{
	RefreshActiveHitSources(InEnabledHitSourceIds, TArray<FName>());
}

void UHeroHitSourceComponent::RefreshActiveHitSources(
	const TArray<FName>& InEnabledHitSourceIds,
	const TArray<FName>& InEnabledHitSourceGroupIds)
{
	ActiveHitSourcesInCurrentWindow.Reset();

	for (const FName& SourceId : InEnabledHitSourceIds)
	{
		if (SourceId != NAME_None && RegisteredHitSources.Contains(SourceId))
		{
			ActiveHitSourcesInCurrentWindow.Add(SourceId);
		}
	}

	for (const FName& GroupId : InEnabledHitSourceGroupIds)
	{
		FActionHitSourceGroupDefinition GroupDefinition;
		if (!TryGetRegisteredHitSourceGroup(GroupId, GroupDefinition))
		{
			continue;
		}

		for (const FName& SourceId : GroupDefinition.SourceIds)
		{
			if (SourceId != NAME_None && RegisteredHitSources.Contains(SourceId))
			{
				ActiveHitSourcesInCurrentWindow.Add(SourceId);
			}
		}
	}

	if (bUseWeaponCollisionDetection)
	{
		ActiveHitSourcesInCurrentWindow.Add(ActionHitSourceDefaults::GetWeaponCollisionSourceId());
	}
}

void UHeroHitSourceComponent::RegisterBuiltInHitSources()
{
	FActionHitSourceDefinition WeaponCollisionSource;
	WeaponCollisionSource.SourceId = ActionHitSourceDefaults::GetWeaponCollisionSourceId();
	WeaponCollisionSource.SourceType = EActionHitSourceType::WeaponCollision;
	WeaponCollisionSource.SourceComponentName = TEXT("WeaponCollisionBox");
	RegisterOrUpdateHitSource(WeaponCollisionSource);

	FActionHitSourceDefinition MainWeaponBladeSource;
	MainWeaponBladeSource.SourceId = ActionHitSourceDefaults::GetMainWeaponBladeSourceId();
	MainWeaponBladeSource.SourceType = EActionHitSourceType::WeaponCollision;
	MainWeaponBladeSource.SourceComponentName = TEXT("MainWeaponBlade");
	RegisterOrUpdateHitSource(MainWeaponBladeSource);

	FActionHitSourceDefinition OffhandWeaponBladeSource;
	OffhandWeaponBladeSource.SourceId = ActionHitSourceDefaults::GetOffhandWeaponBladeSourceId();
	OffhandWeaponBladeSource.SourceType = EActionHitSourceType::WeaponCollision;
	OffhandWeaponBladeSource.SourceComponentName = TEXT("OffhandWeaponBlade");
	RegisterOrUpdateHitSource(OffhandWeaponBladeSource);

	FActionHitSourceDefinition SpearHeadSource;
	SpearHeadSource.SourceId = ActionHitSourceDefaults::GetSpearHeadSourceId();
	SpearHeadSource.SourceType = EActionHitSourceType::WeaponCollision;
	SpearHeadSource.SourceComponentName = TEXT("SpearHead");
	RegisterOrUpdateHitSource(SpearHeadSource);

	FActionHitSourceDefinition SpearShaftSource;
	SpearShaftSource.SourceId = ActionHitSourceDefaults::GetSpearShaftSourceId();
	SpearShaftSource.SourceType = EActionHitSourceType::WeaponCollision;
	SpearShaftSource.SourceComponentName = TEXT("SpearShaft");
	RegisterOrUpdateHitSource(SpearShaftSource);

	FActionHitSourceDefinition GreatswordBladeSource;
	GreatswordBladeSource.SourceId = ActionHitSourceDefaults::GetGreatswordBladeSourceId();
	GreatswordBladeSource.SourceType = EActionHitSourceType::WeaponCollision;
	GreatswordBladeSource.SourceComponentName = TEXT("GreatswordBlade");
	RegisterOrUpdateHitSource(GreatswordBladeSource);

	FActionHitSourceDefinition CharacterBodySource;
	CharacterBodySource.SourceId = ActionHitSourceDefaults::GetCharacterBodySourceId();
	CharacterBodySource.SourceType = EActionHitSourceType::CharacterBody;
	CharacterBodySource.SourceComponentName = TEXT("CharacterBody");
	RegisterOrUpdateHitSource(CharacterBodySource);

	FActionHitSourceDefinition LeftFistSource;
	LeftFistSource.SourceId = ActionHitSourceDefaults::GetLeftFistSourceId();
	LeftFistSource.SourceType = EActionHitSourceType::CharacterBody;
	LeftFistSource.SourceComponentName = TEXT("LeftFist");
	RegisterOrUpdateHitSource(LeftFistSource);

	FActionHitSourceDefinition RightFistSource;
	RightFistSource.SourceId = ActionHitSourceDefaults::GetRightFistSourceId();
	RightFistSource.SourceType = EActionHitSourceType::CharacterBody;
	RightFistSource.SourceComponentName = TEXT("RightFist");
	RegisterOrUpdateHitSource(RightFistSource);

	FActionHitSourceDefinition LeftFootSource;
	LeftFootSource.SourceId = ActionHitSourceDefaults::GetLeftFootSourceId();
	LeftFootSource.SourceType = EActionHitSourceType::CharacterBody;
	LeftFootSource.SourceComponentName = TEXT("LeftFoot");
	RegisterOrUpdateHitSource(LeftFootSource);

	FActionHitSourceDefinition RightFootSource;
	RightFootSource.SourceId = ActionHitSourceDefaults::GetRightFootSourceId();
	RightFootSource.SourceType = EActionHitSourceType::CharacterBody;
	RightFootSource.SourceComponentName = TEXT("RightFoot");
	RegisterOrUpdateHitSource(RightFootSource);

	FActionHitSourceDefinition LeftElbowSource;
	LeftElbowSource.SourceId = ActionHitSourceDefaults::GetLeftElbowSourceId();
	LeftElbowSource.SourceType = EActionHitSourceType::CharacterBody;
	LeftElbowSource.SourceComponentName = TEXT("LeftElbow");
	RegisterOrUpdateHitSource(LeftElbowSource);

	FActionHitSourceDefinition RightElbowSource;
	RightElbowSource.SourceId = ActionHitSourceDefaults::GetRightElbowSourceId();
	RightElbowSource.SourceType = EActionHitSourceType::CharacterBody;
	RightElbowSource.SourceComponentName = TEXT("RightElbow");
	RegisterOrUpdateHitSource(RightElbowSource);

	FActionHitSourceDefinition LeftKneeSource;
	LeftKneeSource.SourceId = ActionHitSourceDefaults::GetLeftKneeSourceId();
	LeftKneeSource.SourceType = EActionHitSourceType::CharacterBody;
	LeftKneeSource.SourceComponentName = TEXT("LeftKnee");
	RegisterOrUpdateHitSource(LeftKneeSource);

	FActionHitSourceDefinition RightKneeSource;
	RightKneeSource.SourceId = ActionHitSourceDefaults::GetRightKneeSourceId();
	RightKneeSource.SourceType = EActionHitSourceType::CharacterBody;
	RightKneeSource.SourceComponentName = TEXT("RightKnee");
	RegisterOrUpdateHitSource(RightKneeSource);

	FActionHitSourceDefinition ProjectileSource;
	ProjectileSource.SourceId = ActionHitSourceDefaults::GetProjectileSourceId();
	ProjectileSource.SourceType = EActionHitSourceType::Projectile;
	ProjectileSource.SourceComponentName = TEXT("Projectile");
	RegisterOrUpdateHitSource(ProjectileSource);

	FActionHitSourceDefinition ExecutionSource;
	ExecutionSource.SourceId = ActionHitSourceDefaults::GetExecutionSourceId();
	ExecutionSource.SourceType = EActionHitSourceType::Execution;
	ExecutionSource.SourceComponentName = TEXT("Execution");
	RegisterOrUpdateHitSource(ExecutionSource);

	FActionHitSourceGroupDefinition UnarmedDefaultGroup;
	UnarmedDefaultGroup.GroupId = ActionHitSourceDefaults::GetUnarmedDefaultGroupId();
	UnarmedDefaultGroup.SourceIds = { ActionHitSourceDefaults::GetCharacterBodySourceId() };
	RegisterOrUpdateHitSourceGroup(UnarmedDefaultGroup);

	FActionHitSourceGroupDefinition UnarmedLightGroup;
	UnarmedLightGroup.GroupId = ActionHitSourceDefaults::GetUnarmedLightGroupId();
	UnarmedLightGroup.SourceIds = {
		ActionHitSourceDefaults::GetLeftFistSourceId(),
		ActionHitSourceDefaults::GetRightFistSourceId()
	};
	RegisterOrUpdateHitSourceGroup(UnarmedLightGroup);

	FActionHitSourceGroupDefinition UnarmedKickGroup;
	UnarmedKickGroup.GroupId = ActionHitSourceDefaults::GetUnarmedKickGroupId();
	UnarmedKickGroup.SourceIds = {
		ActionHitSourceDefaults::GetLeftFootSourceId(),
		ActionHitSourceDefaults::GetRightFootSourceId(),
		ActionHitSourceDefaults::GetLeftKneeSourceId(),
		ActionHitSourceDefaults::GetRightKneeSourceId()
	};
	RegisterOrUpdateHitSourceGroup(UnarmedKickGroup);

	FActionHitSourceGroupDefinition UnarmedElbowGroup;
	UnarmedElbowGroup.GroupId = ActionHitSourceDefaults::GetUnarmedElbowGroupId();
	UnarmedElbowGroup.SourceIds = {
		ActionHitSourceDefaults::GetLeftElbowSourceId(),
		ActionHitSourceDefaults::GetRightElbowSourceId()
	};
	RegisterOrUpdateHitSourceGroup(UnarmedElbowGroup);

	FActionHitSourceGroupDefinition WeaponDefaultGroup;
	WeaponDefaultGroup.GroupId = ActionHitSourceDefaults::GetWeaponDefaultGroupId();
	WeaponDefaultGroup.SourceIds = { ActionHitSourceDefaults::GetWeaponCollisionSourceId() };
	RegisterOrUpdateHitSourceGroup(WeaponDefaultGroup);

	FActionHitSourceGroupDefinition DualBladeDefaultGroup;
	DualBladeDefaultGroup.GroupId = ActionHitSourceDefaults::GetDualBladeDefaultGroupId();
	DualBladeDefaultGroup.SourceIds = {
		ActionHitSourceDefaults::GetMainWeaponBladeSourceId(),
		ActionHitSourceDefaults::GetOffhandWeaponBladeSourceId()
	};
	RegisterOrUpdateHitSourceGroup(DualBladeDefaultGroup);

	FActionHitSourceGroupDefinition SpearDefaultGroup;
	SpearDefaultGroup.GroupId = ActionHitSourceDefaults::GetSpearDefaultGroupId();
	SpearDefaultGroup.SourceIds = {
		ActionHitSourceDefaults::GetSpearHeadSourceId(),
		ActionHitSourceDefaults::GetSpearShaftSourceId()
	};
	RegisterOrUpdateHitSourceGroup(SpearDefaultGroup);
}

void UHeroHitSourceComponent::CacheAndBindOwnedBodyHitComponents()
{
	OwnedBodyHitComponentsBySourceId.Reset();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		FName ResolvedSourceId = NAME_None;
		if (!TryResolveRegisteredHitSourceIdByComponentName(
			PrimitiveComponent->GetFName(),
			EActionHitSourceType::CharacterBody,
			ResolvedSourceId))
		{
			continue;
		}

		OwnedBodyHitComponentsBySourceId.Add(ResolvedSourceId, PrimitiveComponent);
		if (UActionCollisionRuntimeComponent* CollisionRuntimeComponent = GetOwningCollisionRuntimeComponent())
		{
			CollisionRuntimeComponent->RegisterCollisionSlot(
				EActionCollisionSlot::OwnedBodyHit,
				PrimitiveComponent,
				ResolvedSourceId);
		}
		PrimitiveComponent->SetGenerateOverlapEvents(true);
		PrimitiveComponent->OnComponentBeginOverlap.AddUniqueDynamic(
			this,
			&ThisClass::HandleOwnedBodyHitComponentBeginOverlap);
		PrimitiveComponent->OnComponentEndOverlap.AddUniqueDynamic(
			this,
			&ThisClass::HandleOwnedBodyHitComponentEndOverlap);
	}
}

void UHeroHitSourceComponent::UpdateOwnedBodyHitComponentCollisionState()
{
	UActionCollisionRuntimeComponent* CollisionRuntimeComponent = GetOwningCollisionRuntimeComponent();
	if (!CollisionRuntimeComponent)
	{
		return;
	}

	CollisionRuntimeComponent->ReleaseCollisionOverridesByReason(TEXT("HeroOwnedBodyHitWindow"));
	ActiveOwnedBodyCollisionOverrideHandles.Reset();

	for (const TPair<FName, TWeakObjectPtr<UPrimitiveComponent>>& Pair : OwnedBodyHitComponentsBySourceId)
	{
		if (!Pair.Value.IsValid() || !IsHitSourceActive(Pair.Key))
		{
			continue;
		}

		FActionCollisionOverrideRequest CollisionOverrideRequest;
		CollisionOverrideRequest.Slot = EActionCollisionSlot::OwnedBodyHit;
		CollisionOverrideRequest.Preset = EActionCollisionPreset::HitQueryPawnOverlap;
		CollisionOverrideRequest.Priority = 100;
		CollisionOverrideRequest.OwnerReason = TEXT("HeroOwnedBodyHitWindow");
		CollisionOverrideRequest.TargetRegistrationNames.Add(Pair.Key);
		ActiveOwnedBodyCollisionOverrideHandles.Add(
			Pair.Key,
			CollisionRuntimeComponent->AcquireCollisionOverride(CollisionOverrideRequest));
	}
}

void UHeroHitSourceComponent::RefreshRepeatedHitTickState()
{
	const bool bShouldTick =
		IsHitWindowActive()
		&& HeroHitSourceComponentPrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig)
		&& ActiveRepeatedBodyHitContacts.Num() > 0;
	SetComponentTickEnabled(bShouldTick);
}

UActionCollisionRuntimeComponent* UHeroHitSourceComponent::GetOwningCollisionRuntimeComponent() const
{
	const AActionCharacterBase* OwnerCharacterBase = Cast<AActionCharacterBase>(GetOwner());
	if (!OwnerCharacterBase)
	{
		return nullptr;
	}

	return OwnerCharacterBase->GetActionCollisionRuntimeComponent();
}

void UHeroHitSourceComponent::HandleOwnedBodyHitComponentBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!IsValid(OverlappedComponent)
		|| !IsValid(OtherActor)
		|| OtherActor == GetOwner())
	{
		return;
	}

	FName SourceId = NAME_None;
	if (!TryResolveRegisteredHitSourceIdByComponentName(
		OverlappedComponent->GetFName(),
		EActionHitSourceType::CharacterBody,
		SourceId))
	{
		return;
	}

	if (HeroHitSourceComponentPrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig))
	{
		if (TryBeginRepeatedHitContact(SourceId, OverlappedComponent->GetFName(), OtherActor))
		{
			FActionHitSourceRegistrationKey ContactKey;
			ContactKey.SourceId = SourceId;
			ContactKey.TargetActor = OtherActor;
			if (FActionActiveHitContactState* ContactState = ActiveRepeatedBodyHitContacts.Find(ContactKey))
			{
				TryResolveRepeatedBodyHit(*ContactState, TEXT("首次接触命中"));
			}
		}

		RefreshRepeatedHitTickState();
		return;
	}

	if (!TryRegisterSingleHitBySource(SourceId, OtherActor))
	{
		return;
	}

	FActionDamagePayload DamagePayload;
	if (!TryBuildBodyHitDamagePayload(
		OtherActor,
		SourceId,
		OverlappedComponent->GetFName(),
		DamagePayload))
	{
		return;
	}

	const FActionHitResolveResult ResolveResult = UActionHitResolver::ResolveHit(OtherActor, DamagePayload);
	if (!ResolveResult.WasResolved())
	{
		return;
	}

	RewardSpecialWeaponSwitchEnergyOnResolvedHit(DamagePayload, ResolveResult);
	UE_LOG(
		LogHeroHitSourceComponent,
		Log,
		TEXT("命中窗口结算：窗口=%s 策略=单次 来源=%s 目标=%s 事件=首次接触命中"),
		*ActiveHitWindowName.ToString(),
		*SourceId.ToString(),
		*GetNameSafe(OtherActor));
	PrintHitWindowEventDebug(
		FString::Printf(
			TEXT("[命中窗口] 首次命中: 窗口=%s 来源=%s 目标=%s"),
			*ActiveHitWindowName.ToString(),
			*SourceId.ToString(),
			*GetNameSafe(OtherActor)),
		FColor::Green);
}

void UHeroHitSourceComponent::HandleOwnedBodyHitComponentEndOverlap(
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
	if (!TryResolveRegisteredHitSourceIdByComponentName(
		OverlappedComponent->GetFName(),
		EActionHitSourceType::CharacterBody,
		SourceId))
	{
		return;
	}

	RemoveRepeatedBodyHitContact(SourceId, OtherActor);
	RefreshRepeatedHitTickState();
}

bool UHeroHitSourceComponent::TryBuildBodyHitDamagePayload(
	AActor* InTargetActor,
	const FName InSourceId,
	const FName InSourceComponentName,
	FActionDamagePayload& OutDamagePayload) const
{
	OutDamagePayload = FActionDamagePayload();

	const AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	if (!OwnerHeroCharacter)
	{
		return false;
	}

	const UHeroAttackComponent* AttackComponent = OwnerHeroCharacter->GetHeroAttackComponent();
	if (!AttackComponent)
	{
		return false;
	}

	return AttackComponent->TryBuildCurrentAttackDamagePayloadForHitSource(
		InTargetActor,
		InSourceId,
		InSourceComponentName,
		OutDamagePayload);
}

bool UHeroHitSourceComponent::TryResolveRepeatedBodyHit(
	FActionActiveHitContactState& InOutContactState,
	const TCHAR* InDebugResolveReason)
{
	if (!InOutContactState.TargetActor.IsValid()
		|| !IsHitWindowActive()
		|| !HeroHitSourceComponentPrivate::UsesRepeatedHitPolicy(ActiveHitWindowRuntimeConfig)
		|| !IsHitSourceActive(InOutContactState.SourceId))
	{
		return false;
	}

	FActionDamagePayload DamagePayload;
	if (!TryBuildBodyHitDamagePayload(
		InOutContactState.TargetActor.Get(),
		InOutContactState.SourceId,
		InOutContactState.SourceComponentName,
		DamagePayload))
	{
		return false;
	}

	const FActionHitResolveResult ResolveResult =
		UActionHitResolver::ResolveHit(InOutContactState.TargetActor.Get(), DamagePayload);
	InOutContactState.LastResolvedWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	UE_LOG(
		LogHeroHitSourceComponent,
		Log,
		TEXT("命中窗口结算：窗口=%s 策略=持续接触间隔 来源=%s 目标=%s 间隔=%.3f 事件=%s"),
		*ActiveHitWindowName.ToString(),
		*InOutContactState.SourceId.ToString(),
		*GetNameSafe(InOutContactState.TargetActor.Get()),
		ActiveHitWindowRuntimeConfig.RepeatResolveInterval,
		InDebugResolveReason);

	if (!ResolveResult.WasResolved())
	{
		return false;
	}

	RewardSpecialWeaponSwitchEnergyOnResolvedHit(DamagePayload, ResolveResult);
	PrintHitWindowEventDebug(
		FString::Printf(
			TEXT("[命中窗口] %s: 窗口=%s 来源=%s 目标=%s 间隔=%.2f"),
			InDebugResolveReason,
			*ActiveHitWindowName.ToString(),
			*InOutContactState.SourceId.ToString(),
			*GetNameSafe(InOutContactState.TargetActor.Get()),
			ActiveHitWindowRuntimeConfig.RepeatResolveInterval),
		FColor::Orange);
	return true;
}

void UHeroHitSourceComponent::RemoveRepeatedBodyHitContact(const FName InSourceId, AActor* InTargetActor)
{
	if (InSourceId == NAME_None || !IsValid(InTargetActor))
	{
		return;
	}

	FActionHitSourceRegistrationKey RegistrationKey;
	RegistrationKey.SourceId = InSourceId;
	RegistrationKey.TargetActor = InTargetActor;
	if (ActiveRepeatedBodyHitContacts.Remove(RegistrationKey) > 0)
	{
		UE_LOG(
			LogHeroHitSourceComponent,
			Log,
			TEXT("命中窗口停止：窗口=%s 策略=持续接触间隔 来源=%s 目标=%s 事件=因窗口关闭或接触结束被停止"),
			*ActiveHitWindowName.ToString(),
			*InSourceId.ToString(),
			*GetNameSafe(InTargetActor));
		PrintHitWindowEventDebug(
			FString::Printf(
				TEXT("[命中窗口] 停止接触: 窗口=%s 来源=%s 目标=%s"),
				*ActiveHitWindowName.ToString(),
				*InSourceId.ToString(),
				*GetNameSafe(InTargetActor)),
			FColor::Silver,
			1.5f);
	}
}

void UHeroHitSourceComponent::RewardSpecialWeaponSwitchEnergyOnResolvedHit(
	const FActionDamagePayload& InDamagePayload,
	const FActionHitResolveResult& InResolveResult) const
{
	if (InDamagePayload.SpecialWeaponSwitchEnergyRewardOnHit <= 0.f)
	{
		return;
	}

	if (InResolveResult.ResultType != EActionHitResultType::Damaged
		&& InResolveResult.ResultType != EActionHitResultType::Blocked
		&& InResolveResult.ResultType != EActionHitResultType::GuardBroken)
	{
		return;
	}

	const AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	if (!OwnerHeroCharacter)
	{
		return;
	}

	if (UHeroEquipmentComponent* EquipmentComponent = OwnerHeroCharacter->FindComponentByClass<UHeroEquipmentComponent>())
	{
		EquipmentComponent->AddSpecialWeaponSwitchEnergy(InDamagePayload.SpecialWeaponSwitchEnergyRewardOnHit);
	}
}
