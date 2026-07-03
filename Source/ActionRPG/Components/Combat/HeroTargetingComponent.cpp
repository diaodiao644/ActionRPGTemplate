// 英雄 Targeting 组件实现。
#include "Components/Combat/HeroTargetingComponent.h"

#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameBase/ActionPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroTargetingComponent, Log, All);

namespace HeroTargetingTurnAssist
{
	enum class ESimpleTurnAssistDebugResult : uint8
	{
		Disabled,
		InvalidForward,
		NoHit,
		Filtered,
		RayHit,
		LockHit,
		InputFallback
	};

	static const TCHAR* GetTriggerSourceText(const EActionSimpleTurnAssistTriggerSource InTriggerSource)
	{
		switch (InTriggerSource)
		{
		case EActionSimpleTurnAssistTriggerSource::Execution:
			return TEXT("处决");
		case EActionSimpleTurnAssistTriggerSource::SpiritOffensive:
			return TEXT("灵武器主动攻击");
		case EActionSimpleTurnAssistTriggerSource::Attack:
		default:
			break;
		}

		return TEXT("攻击");
	}

	static float ResolveFacingAngleDegrees(const float InFacingDot)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(InFacingDot, -1.f, 1.f)));
	}

	static FVector RotateDirectionByYawDegrees(const FVector& InDirection, const float InYawDegrees)
	{
		return FRotator(0.f, InYawDegrees, 0.f).RotateVector(InDirection);
	}

	static void AddUniqueRayAngleDegrees(TArray<float>& InOutRayAngles, const float InAngleDegrees)
	{
		for (const float ExistingAngleDegrees : InOutRayAngles)
		{
			if (FMath::IsNearlyEqual(ExistingAngleDegrees, InAngleDegrees, KINDA_SMALL_NUMBER))
			{
				return;
			}
		}

		InOutRayAngles.Add(InAngleDegrees);
	}

	static void BuildRayAngleSet(
		const float InMaxSearchHalfAngleDegrees,
		const float InRayAngleStepDegrees,
		TArray<float>& OutRayAngles)
	{
		OutRayAngles.Reset();
		AddUniqueRayAngleDegrees(OutRayAngles, 0.f);

		const float MaxSearchHalfAngleDegrees = FMath::Clamp(InMaxSearchHalfAngleDegrees, 0.f, 180.f);
		const float RayAngleStepDegrees = FMath::Max(InRayAngleStepDegrees, 0.1f);
		for (float AngleDegrees = RayAngleStepDegrees;
			AngleDegrees < MaxSearchHalfAngleDegrees;
			AngleDegrees += RayAngleStepDegrees)
		{
			AddUniqueRayAngleDegrees(OutRayAngles, AngleDegrees);
			AddUniqueRayAngleDegrees(OutRayAngles, -AngleDegrees);
		}

		if (MaxSearchHalfAngleDegrees > KINDA_SMALL_NUMBER)
		{
			AddUniqueRayAngleDegrees(OutRayAngles, MaxSearchHalfAngleDegrees);
			AddUniqueRayAngleDegrees(OutRayAngles, -MaxSearchHalfAngleDegrees);
		}

		OutRayAngles.Sort([](const float LeftAngleDegrees, const float RightAngleDegrees)
		{
			return LeftAngleDegrees < RightAngleDegrees;
		});
	}

	static const TCHAR* GetDebugResultText(const ESimpleTurnAssistDebugResult InResult)
	{
		switch (InResult)
		{
		case ESimpleTurnAssistDebugResult::Disabled:
			return TEXT("Disabled");
		case ESimpleTurnAssistDebugResult::InvalidForward:
			return TEXT("InvalidForward");
		case ESimpleTurnAssistDebugResult::NoHit:
			return TEXT("NoHit");
		case ESimpleTurnAssistDebugResult::Filtered:
			return TEXT("Filtered");
		case ESimpleTurnAssistDebugResult::RayHit:
			return TEXT("RayHit");
		case ESimpleTurnAssistDebugResult::LockHit:
			return TEXT("LockHit");
		case ESimpleTurnAssistDebugResult::InputFallback:
			return TEXT("InputFallback");
		default:
			break;
		}

		return TEXT("Unknown");
	}

	static FColor GetDebugResultColor(const ESimpleTurnAssistDebugResult InResult)
	{
		switch (InResult)
		{
		case ESimpleTurnAssistDebugResult::Disabled:
			return FColor(128, 128, 128);
		case ESimpleTurnAssistDebugResult::InvalidForward:
			return FColor::Red;
		case ESimpleTurnAssistDebugResult::NoHit:
			return FColor::Orange;
		case ESimpleTurnAssistDebugResult::Filtered:
			return FColor(255, 140, 0);
		case ESimpleTurnAssistDebugResult::RayHit:
			return FColor::Green;
		case ESimpleTurnAssistDebugResult::LockHit:
			return FColor::Blue;
		case ESimpleTurnAssistDebugResult::InputFallback:
			return FColor::Magenta;
		default:
			break;
		}

		return FColor::White;
	}

	static ESimpleTurnAssistDebugResult ResolveDebugResultFromText(const TCHAR* InDebugResultText)
	{
		if (FCString::Strcmp(InDebugResultText, TEXT("Disabled")) == 0)
		{
			return ESimpleTurnAssistDebugResult::Disabled;
		}

		if (FCString::Strcmp(InDebugResultText, TEXT("InvalidForward")) == 0)
		{
			return ESimpleTurnAssistDebugResult::InvalidForward;
		}

		if (FCString::Strcmp(InDebugResultText, TEXT("NoHit")) == 0)
		{
			return ESimpleTurnAssistDebugResult::NoHit;
		}

		if (FCString::Strcmp(InDebugResultText, TEXT("Filtered")) == 0)
		{
			return ESimpleTurnAssistDebugResult::Filtered;
		}

		if (FCString::Strcmp(InDebugResultText, TEXT("LockHit")) == 0)
		{
			return ESimpleTurnAssistDebugResult::LockHit;
		}

		if (FCString::Strcmp(InDebugResultText, TEXT("InputFallback")) == 0)
		{
			return ESimpleTurnAssistDebugResult::InputFallback;
		}

		return ESimpleTurnAssistDebugResult::RayHit;
	}
}

namespace HeroTargetingLock
{
	static const TCHAR* GetBreakReasonText(const EActionTargetLockBreakReason InReason)
	{
		switch (InReason)
		{
		case EActionTargetLockBreakReason::ManualRelease:
			return TEXT("手动解除");
		case EActionTargetLockBreakReason::AcquireFailed:
			return TEXT("获取失败");
		case EActionTargetLockBreakReason::TargetInvalid:
			return TEXT("目标失效");
		case EActionTargetLockBreakReason::TargetDead:
			return TEXT("目标死亡");
		case EActionTargetLockBreakReason::OutOfRange:
			return TEXT("超出距离");
		case EActionTargetLockBreakReason::RuntimeReset:
			return TEXT("运行时重置");
		case EActionTargetLockBreakReason::None:
		default:
			break;
		}

		return TEXT("无");
	}
}

UHeroTargetingComponent::UHeroTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UHeroTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshMovementOrientationFromLockState();
}

void UHeroTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTargetLock(EActionTargetLockBreakReason::RuntimeReset);
	Super::EndPlay(EndPlayReason);
}

void UHeroTargetingComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateTargetLockRuntime(DeltaTime);
}

bool UHeroTargetingComponent::TryApplyOffensiveFacing(const EActionSimpleTurnAssistTriggerSource InTriggerSource)
{
	const FActionSimpleTurnAssistConfig* TurnAssistConfig = ResolveTurnAssistConfig(InTriggerSource);

	if (IsTargetLockActive())
	{
		AActor* CurrentLockedTargetActor = GetLockedTargetActor();
		if (IsValidSimpleTurnAssistTarget(CurrentLockedTargetActor))
		{
			const bool bAppliedTurn = TryApplySimpleTurnAssistTowardTarget(CurrentLockedTargetActor, InTriggerSource);
			if (bAppliedTurn)
			{
				if (TurnAssistConfig)
				{
					AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
					if (OwnerHeroCharacter)
					{
						const FVector SearchStart = OwnerHeroCharacter->GetActorLocation() +
							FVector(0.f, 0.f, TurnAssistConfig->SearchHeightOffset);
						const FVector SearchEnd = SearchStart +
							(CurrentLockedTargetActor->GetActorLocation() - SearchStart).GetSafeNormal2D() *
							FMath::Max(TurnAssistConfig->SearchDistance, 0.f);
						DrawSimpleTurnAssistDebug(
							*TurnAssistConfig,
							InTriggerSource,
							SearchStart,
							SearchEnd,
							CurrentLockedTargetActor,
							true,
							HeroTargetingTurnAssist::GetDebugResultText(
								HeroTargetingTurnAssist::ESimpleTurnAssistDebugResult::LockHit));
					}
				}
			}

			return bAppliedTurn;
		}

		ClearTargetLock(EActionTargetLockBreakReason::TargetInvalid);
	}

	if (TurnAssistConfig)
	{
		if (TryApplySimpleTurnAssist(*TurnAssistConfig, InTriggerSource))
		{
			return true;
		}
	}

	return TryApplyInputFacingFallback(InTriggerSource);
}

bool UHeroTargetingComponent::TryResolveSimpleTurnAssistTarget(
	const FActionSimpleTurnAssistConfig& InTurnAssistConfig,
	const EActionSimpleTurnAssistTriggerSource InTriggerSource,
	AActor*& OutTargetActor)
{
	OutTargetActor = nullptr;
	LastSimpleTurnAssistDebugSnapshot.Reset();
	LastSimpleTurnAssistDebugSnapshot.bAttempted = true;
	LastSimpleTurnAssistDebugSnapshot.TriggerSource = InTriggerSource;

	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !GetWorld())
	{
		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[轻量转向] 来源=%s 未执行搜索：HeroValid=%d WorldValid=%d"),
			HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource),
			OwnerHeroCharacter != nullptr ? 1 : 0,
			GetWorld() != nullptr ? 1 : 0);
		return false;
	}

	const FVector ActorLocation = OwnerHeroCharacter->GetActorLocation();
	const FVector SearchStart = ActorLocation + FVector(0.f, 0.f, InTurnAssistConfig.SearchHeightOffset);

	FVector SearchForward = OwnerHeroCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (SearchForward.IsNearlyZero())
	{
		SearchForward = OwnerHeroCharacter->GetActorRotation().Vector().GetSafeNormal2D();
	}

	if (SearchForward.IsNearlyZero())
	{
		SearchForward = FVector::ForwardVector;
	}

	const FVector SearchEnd =
		SearchStart + SearchForward * FMath::Max(InTurnAssistConfig.SearchDistance, 0.f);

	if (!InTurnAssistConfig.IsEnabledConfig())
	{
		DrawSimpleTurnAssistDebug(
			InTurnAssistConfig,
			InTriggerSource,
			SearchStart,
			SearchEnd,
			nullptr,
			false,
			HeroTargetingTurnAssist::GetDebugResultText(
				HeroTargetingTurnAssist::ESimpleTurnAssistDebugResult::Disabled));
		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[轻量转向] 来源=%s 未执行搜索：ConfigEnabled=0"),
			HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource));
		return false;
	}

	const FVector RawForward = OwnerHeroCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (RawForward.IsNearlyZero())
	{
		DrawSimpleTurnAssistDebug(
			InTurnAssistConfig,
			InTriggerSource,
			SearchStart,
			SearchEnd,
			nullptr,
			false,
			HeroTargetingTurnAssist::GetDebugResultText(
				HeroTargetingTurnAssist::ESimpleTurnAssistDebugResult::InvalidForward));
		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[轻量转向] 来源=%s 未执行搜索：当前角色前向无效"),
			HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource));
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SimpleTurnAssistRayTrace), false);
	QueryParams.AddIgnoredActor(OwnerHeroCharacter);

	TArray<float> RayAngleDegrees;
	HeroTargetingTurnAssist::BuildRayAngleSet(
		InTurnAssistConfig.MaxSearchHalfAngleDegrees,
		InTurnAssistConfig.RayAngleStepDegrees,
		RayAngleDegrees);
	TArray<FVector> RayDirections;
	RayDirections.Reserve(RayAngleDegrees.Num());
	TArray<int32> HitRayIndices;

	AActor* BestTargetActor = nullptr;
	float BestDistanceSquared = FLT_MAX;
	float BestFacingDot = -1.f;
	float BestAbsoluteAngleDegrees = TNumericLimits<float>::Max();
	bool bAnyRayHit = false;

	for (int32 RayIndex = 0; RayIndex < RayAngleDegrees.Num(); ++RayIndex)
	{
		const float AngleDegrees = RayAngleDegrees[RayIndex];
		const FVector RayDirection =
			HeroTargetingTurnAssist::RotateDirectionByYawDegrees(SearchForward, AngleDegrees).GetSafeNormal2D();
		RayDirections.Add(RayDirection);

		TArray<FHitResult> HitResults;
		const FVector RayEnd = SearchStart + RayDirection * FMath::Max(InTurnAssistConfig.SearchDistance, 0.f);
		if (!GetWorld()->LineTraceMultiByObjectType(
			HitResults,
			SearchStart,
			RayEnd,
			ObjectQueryParams,
			QueryParams))
		{
			continue;
		}

		bAnyRayHit = true;

		for (const FHitResult& HitResult : HitResults)
		{
			AActor* CandidateTarget = HitResult.GetActor();
			if (!IsValidSimpleTurnAssistTarget(CandidateTarget))
			{
				continue;
			}

			float FacingDot = 0.f;
			float CenterAngleDegrees = 0.f;
			float DistanceSquared = 0.f;
			if (!TryResolveSimpleTurnAssistCenterFacing(
				SearchStart,
				SearchForward,
				CandidateTarget,
				FacingDot,
				CenterAngleDegrees,
				DistanceSquared))
			{
				continue;
			}

			if (CenterAngleDegrees > InTurnAssistConfig.MaxSearchHalfAngleDegrees)
			{
				continue;
			}

			HitRayIndices.AddUnique(RayIndex);

			const float AbsoluteAngleDegrees = CenterAngleDegrees;
			const bool bFoundCloserTarget = DistanceSquared < BestDistanceSquared;
			const bool bSameDistanceButMoreCentered = FMath::IsNearlyEqual(DistanceSquared, BestDistanceSquared, 1.f)
				&& AbsoluteAngleDegrees < BestAbsoluteAngleDegrees;
			if (bFoundCloserTarget || bSameDistanceButMoreCentered)
			{
				BestDistanceSquared = DistanceSquared;
				BestFacingDot = FacingDot;
				BestAbsoluteAngleDegrees = AbsoluteAngleDegrees;
				BestTargetActor = CandidateTarget;
			}

			break;
		}
	}

	if (!bAnyRayHit)
	{
		DrawSimpleTurnAssistDebug(
			InTurnAssistConfig,
			InTriggerSource,
			SearchStart,
			SearchEnd,
			nullptr,
			false,
			HeroTargetingTurnAssist::GetDebugResultText(
				HeroTargetingTurnAssist::ESimpleTurnAssistDebugResult::NoHit),
			&RayDirections,
			&HitRayIndices);
		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[轻量转向] 来源=%s 未找到候选目标：扇形射线范围内无命中"),
			HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource));
		return false;
	}

	if (!IsValid(BestTargetActor))
	{
		DrawSimpleTurnAssistDebug(
			InTurnAssistConfig,
			InTriggerSource,
			SearchStart,
			SearchEnd,
			nullptr,
			false,
			HeroTargetingTurnAssist::GetDebugResultText(
				HeroTargetingTurnAssist::ESimpleTurnAssistDebugResult::Filtered),
			&RayDirections,
			&HitRayIndices);
		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[轻量转向] 来源=%s 未找到候选目标：射线命中里没有合法目标"),
			HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource));
		return false;
	}

	LastSimpleTurnAssistDebugSnapshot.bFoundCandidate = true;
	LastSimpleTurnAssistDebugSnapshot.TargetActor = BestTargetActor;
	LastSimpleTurnAssistDebugSnapshot.Distance = FMath::Sqrt(BestDistanceSquared);
	LastSimpleTurnAssistDebugSnapshot.FacingDot = BestFacingDot;
	LastSimpleTurnAssistDebugSnapshot.TargetAngleDegrees =
		HeroTargetingTurnAssist::ResolveFacingAngleDegrees(BestFacingDot);

	DrawSimpleTurnAssistDebug(
		InTurnAssistConfig,
		InTriggerSource,
		SearchStart,
		SearchEnd,
		BestTargetActor,
		false,
		HeroTargetingTurnAssist::GetDebugResultText(
			HeroTargetingTurnAssist::ESimpleTurnAssistDebugResult::RayHit),
		&RayDirections,
		&HitRayIndices);
	OutTargetActor = BestTargetActor;
	UE_LOG(
		LogHeroTargetingComponent,
		Log,
		TEXT("[轻量转向] 来源=%s 找到目标=%s 距离=%.2f FacingDot=%.3f 角度=%.2f"),
		HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource),
		*GetNameSafe(BestTargetActor),
		LastSimpleTurnAssistDebugSnapshot.Distance,
		LastSimpleTurnAssistDebugSnapshot.FacingDot,
		LastSimpleTurnAssistDebugSnapshot.TargetAngleDegrees);
	return true;
}

bool UHeroTargetingComponent::TryApplySimpleTurnAssist(
	const FActionSimpleTurnAssistConfig& InTurnAssistConfig,
	const EActionSimpleTurnAssistTriggerSource InTriggerSource)
{
	AActor* TargetActor = nullptr;
	if (!TryResolveSimpleTurnAssistTarget(InTurnAssistConfig, InTriggerSource, TargetActor))
	{
		return false;
	}

	return TryApplySimpleTurnAssistTowardTarget(TargetActor, InTriggerSource);
}

void UHeroTargetingComponent::DrawSimpleTurnAssistDebug(
	const FActionSimpleTurnAssistConfig& InTurnAssistConfig,
	const EActionSimpleTurnAssistTriggerSource InTriggerSource,
	const FVector& InSearchStart,
	const FVector& InSearchEnd,
	AActor* InResolvedTargetActor,
	const bool bInResolvedFromTargetLock,
	const TCHAR* InDebugResultText,
	const TArray<FVector>* InRayDirections,
	const TArray<int32>* InHitRayIndices) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!bDrawSimpleTurnAssistDebug || !GetWorld())
	{
		return;
	}

	const float DebugDuration = FMath::Max(SimpleTurnAssistDebugDrawDuration, 0.f);
	const FVector SearchDirection = (InSearchEnd - InSearchStart).GetSafeNormal2D();
	const FVector DisplayDirection = SearchDirection.IsNearlyZero() ? FVector::ForwardVector : SearchDirection;
	const HeroTargetingTurnAssist::ESimpleTurnAssistDebugResult DebugResult =
		HeroTargetingTurnAssist::ResolveDebugResultFromText(InDebugResultText);
	const FColor SearchColor =
		bInResolvedFromTargetLock ? FColor::Blue : HeroTargetingTurnAssist::GetDebugResultColor(DebugResult);
	const FColor AngleColor = FColor::Cyan;
	const FColor TargetColor = SearchColor;
	const FVector LeftBoundaryDirection = HeroTargetingTurnAssist::RotateDirectionByYawDegrees(
		DisplayDirection,
		InTurnAssistConfig.MaxSearchHalfAngleDegrees);
	const FVector RightBoundaryDirection = HeroTargetingTurnAssist::RotateDirectionByYawDegrees(
		DisplayDirection,
		-InTurnAssistConfig.MaxSearchHalfAngleDegrees);
	const FVector HeightBase = InSearchStart - FVector(0.f, 0.f, InTurnAssistConfig.SearchHeightOffset);
	const float SearchDistance = FMath::Max(InTurnAssistConfig.SearchDistance, 0.f);
	const float MarkerRadius = 10.f;

	DrawDebugLine(GetWorld(), HeightBase, InSearchStart, FColor::Silver, false, DebugDuration, 0, 1.0f);
	DrawDebugSphere(GetWorld(), HeightBase, 8.f, 8, FColor::Silver, false, DebugDuration, 0, 1.0f);
	DrawDebugSphere(GetWorld(), InSearchStart, MarkerRadius, 10, SearchColor, false, DebugDuration, 0, 1.5f);
	DrawDebugSphere(GetWorld(), InSearchEnd, MarkerRadius, 10, SearchColor, false, DebugDuration, 0, 1.5f);
	DrawDebugLine(GetWorld(), InSearchStart, InSearchEnd, SearchColor, false, DebugDuration, 0, 1.5f);
	DrawDebugLine(
		GetWorld(),
		InSearchStart,
		InSearchStart + LeftBoundaryDirection * SearchDistance,
		AngleColor,
		false,
		DebugDuration,
		0,
		1.0f);
	DrawDebugLine(
		GetWorld(),
		InSearchStart,
		InSearchStart + RightBoundaryDirection * SearchDistance,
		AngleColor,
		false,
		DebugDuration,
		0,
		1.0f);

	if (InRayDirections && InRayDirections->Num() > 0)
	{
		for (int32 RayIndex = 0; RayIndex < InRayDirections->Num(); ++RayIndex)
		{
			const bool bHitRay = InHitRayIndices && InHitRayIndices->Contains(RayIndex);
			const FColor RayColor = bHitRay ? SearchColor : FColor(80, 80, 80);
			DrawDebugLine(
				GetWorld(),
				InSearchStart,
				InSearchStart + (*InRayDirections)[RayIndex] * SearchDistance,
				RayColor,
				false,
				DebugDuration,
				0,
				bHitRay ? 1.5f : 0.75f);
		}
	}

	if (IsValid(InResolvedTargetActor))
	{
		DrawDebugSphere(
			GetWorld(),
			InResolvedTargetActor->GetActorLocation(),
			28.f,
			12,
			TargetColor,
			false,
			DebugDuration,
			0,
			2.0f);
		DrawDebugLine(
			GetWorld(),
			InSearchStart,
			InResolvedTargetActor->GetActorLocation(),
			TargetColor,
			false,
			DebugDuration,
			0,
			1.5f);
	}

	DrawDebugString(
		GetWorld(),
		InSearchStart + FVector(0.f, 0.f, 30.f),
		FString::Printf(
			TEXT("TurnAssist[%s][%s] Dist=%.0f HalfAngle=%.0f Step=%.0f Height=%.0f"),
			HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource),
			InDebugResultText,
			InTurnAssistConfig.SearchDistance,
			InTurnAssistConfig.MaxSearchHalfAngleDegrees,
			InTurnAssistConfig.RayAngleStepDegrees,
			InTurnAssistConfig.SearchHeightOffset),
		nullptr,
		FColor::White,
		DebugDuration,
		false);
#endif
}

const FActionSimpleTurnAssistConfig* UHeroTargetingComponent::ResolveTurnAssistConfig(
	const EActionSimpleTurnAssistTriggerSource InTriggerSource) const
{
	switch (InTriggerSource)
	{
	case EActionSimpleTurnAssistTriggerSource::SpiritOffensive:
		return &SpiritOffensiveTurnAssistConfig;

	case EActionSimpleTurnAssistTriggerSource::Attack:
		return &AttackTurnAssistConfig;

	case EActionSimpleTurnAssistTriggerSource::Execution:
	default:
		break;
	}

	return nullptr;
}

bool UHeroTargetingComponent::TryApplySimpleTurnAssistTowardTarget(
	AActor* InTargetActor,
	const EActionSimpleTurnAssistTriggerSource InTriggerSource)
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !IsValidSimpleTurnAssistTarget(InTargetActor))
	{
		LastSimpleTurnAssistDebugSnapshot.Reset();
		LastSimpleTurnAssistDebugSnapshot.bAttempted = true;
		LastSimpleTurnAssistDebugSnapshot.TriggerSource = InTriggerSource;
		LastSimpleTurnAssistDebugSnapshot.TargetActor = InTargetActor;

		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[轻量转向] 来源=%s 未执行转向：Hero 或目标无效"),
			HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource));
		return false;
	}

	FVector ToTarget = InTargetActor->GetActorLocation() - OwnerHeroCharacter->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero())
	{
		LastSimpleTurnAssistDebugSnapshot.Reset();
		LastSimpleTurnAssistDebugSnapshot.bAttempted = true;
		LastSimpleTurnAssistDebugSnapshot.bFoundCandidate = true;
		LastSimpleTurnAssistDebugSnapshot.TriggerSource = InTriggerSource;
		LastSimpleTurnAssistDebugSnapshot.TargetActor = InTargetActor;

		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[轻量转向] 来源=%s 未执行转向：目标位置与自身过近"),
			HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource));
		return false;
	}

	const FVector CurrentForward = OwnerHeroCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	const float FacingDot =
		CurrentForward.IsNearlyZero() ? 0.f : FVector::DotProduct(CurrentForward, DirectionToTarget);

	LastSimpleTurnAssistDebugSnapshot.bAttempted = true;
	LastSimpleTurnAssistDebugSnapshot.bFoundCandidate = true;
	LastSimpleTurnAssistDebugSnapshot.bAppliedTurn = true;
	LastSimpleTurnAssistDebugSnapshot.TriggerSource = InTriggerSource;
	LastSimpleTurnAssistDebugSnapshot.TargetActor = InTargetActor;
	LastSimpleTurnAssistDebugSnapshot.Distance = ToTarget.Size();
	LastSimpleTurnAssistDebugSnapshot.FacingDot = FacingDot;
	LastSimpleTurnAssistDebugSnapshot.TargetAngleDegrees =
		HeroTargetingTurnAssist::ResolveFacingAngleDegrees(FacingDot);

	const FRotator TargetRotation = DirectionToTarget.Rotation();
	OwnerHeroCharacter->SetActorRotation(FRotator(0.f, TargetRotation.Yaw, 0.f));

	UE_LOG(
		LogHeroTargetingComponent,
		Log,
		TEXT("[轻量转向] 来源=%s 已转向目标=%s 距离=%.2f FacingDot=%.3f 角度=%.2f"),
		HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource),
		*GetNameSafe(InTargetActor),
		LastSimpleTurnAssistDebugSnapshot.Distance,
		LastSimpleTurnAssistDebugSnapshot.FacingDot,
		LastSimpleTurnAssistDebugSnapshot.TargetAngleDegrees);
	return true;
}

bool UHeroTargetingComponent::TryApplyInputFacingFallback(
	const EActionSimpleTurnAssistTriggerSource InTriggerSource)
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	AActionPlayerController* OwnerController = GetOwningHeroController();
	if (!OwnerHeroCharacter || !OwnerController || !OwnerController->HasMoveInput())
	{
		return false;
	}

	const FRotator InputDirection = OwnerController->GetInputDirection();
	const FRotator InputYawRotation(0.f, InputDirection.Yaw, 0.f);
	const FVector InputForward = InputYawRotation.Vector().GetSafeNormal2D();
	if (InputForward.IsNearlyZero())
	{
		return false;
	}

	LastSimpleTurnAssistDebugSnapshot.Reset();
	LastSimpleTurnAssistDebugSnapshot.bAttempted = true;
	LastSimpleTurnAssistDebugSnapshot.bAppliedTurn = true;
	LastSimpleTurnAssistDebugSnapshot.TriggerSource = InTriggerSource;
	LastSimpleTurnAssistDebugSnapshot.FacingDot =
		FVector::DotProduct(OwnerHeroCharacter->GetActorForwardVector().GetSafeNormal2D(), InputForward);
	LastSimpleTurnAssistDebugSnapshot.TargetAngleDegrees =
		HeroTargetingTurnAssist::ResolveFacingAngleDegrees(LastSimpleTurnAssistDebugSnapshot.FacingDot);

	OwnerHeroCharacter->SetActorRotation(InputYawRotation);

	if (const FActionSimpleTurnAssistConfig* TurnAssistConfig = ResolveTurnAssistConfig(InTriggerSource))
	{
		const FVector SearchStart =
			OwnerHeroCharacter->GetActorLocation() + FVector(0.f, 0.f, TurnAssistConfig->SearchHeightOffset);
		const FVector SearchEnd =
			SearchStart + InputForward * FMath::Max(TurnAssistConfig->SearchDistance, 0.f);
		DrawSimpleTurnAssistDebug(
			*TurnAssistConfig,
			InTriggerSource,
			SearchStart,
			SearchEnd,
			nullptr,
			false,
			HeroTargetingTurnAssist::GetDebugResultText(
				HeroTargetingTurnAssist::ESimpleTurnAssistDebugResult::InputFallback));
	}

	UE_LOG(
		LogHeroTargetingComponent,
		Log,
		TEXT("[主动攻击转向] 来源=%s 未命中有效目标，已按输入方向回退转向。InputYaw=%.2f FacingDot=%.3f 角度=%.2f"),
		HeroTargetingTurnAssist::GetTriggerSourceText(InTriggerSource),
		InputYawRotation.Yaw,
		LastSimpleTurnAssistDebugSnapshot.FacingDot,
		LastSimpleTurnAssistDebugSnapshot.TargetAngleDegrees);
	return true;
}

bool UHeroTargetingComponent::TryResolveSimpleTurnAssistCenterFacing(
	const FVector& InSearchStart,
	const FVector& InSearchForward,
	AActor* InTargetActor,
	float& OutFacingDot,
	float& OutAngleDegrees,
	float& OutDistanceSquared) const
{
	OutFacingDot = 0.f;
	OutAngleDegrees = 0.f;
	OutDistanceSquared = 0.f;

	if (!IsValidSimpleTurnAssistTarget(InTargetActor))
	{
		return false;
	}

	FVector ToTarget = InTargetActor->GetActorLocation() - InSearchStart;
	ToTarget.Z = 0.f;
	OutDistanceSquared = ToTarget.SizeSquared();
	if (OutDistanceSquared <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector SearchForward = InSearchForward.GetSafeNormal2D();
	if (SearchForward.IsNearlyZero())
	{
		return false;
	}

	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	OutFacingDot = FVector::DotProduct(SearchForward, DirectionToTarget);
	OutAngleDegrees = HeroTargetingTurnAssist::ResolveFacingAngleDegrees(OutFacingDot);
	return true;
}

bool UHeroTargetingComponent::TryAcquireTargetLock()
{
	AActor* TargetActor = nullptr;
	if (!TryResolveTargetLockCandidate(TargetActor))
	{
		ClearTargetLock(EActionTargetLockBreakReason::AcquireFailed);
		LastTargetLockDebugSnapshot.bLastAcquireSucceeded = false;
		return false;
	}

	bTargetLockActive = true;
	LockedTargetActor = TargetActor;
	RefreshTargetLockSnapshotFromTarget(TargetActor);
	LastTargetLockDebugSnapshot.bLockActive = true;
	LastTargetLockDebugSnapshot.bLastAcquireSucceeded = true;
	LastTargetLockDebugSnapshot.BreakReason = EActionTargetLockBreakReason::None;
	RefreshMovementOrientationFromLockState();

	UE_LOG(
		LogHeroTargetingComponent,
		Log,
		TEXT("[锁定目标] 获取成功：目标=%s 距离=%.2f FacingDot=%.3f"),
		*GetNameSafe(TargetActor),
		LastTargetLockDebugSnapshot.Distance,
		LastTargetLockDebugSnapshot.FacingDot);
	return true;
}

bool UHeroTargetingComponent::ToggleTargetLock()
{
	if (IsTargetLockActive())
	{
		ClearTargetLock(EActionTargetLockBreakReason::ManualRelease);
		return true;
	}

	return TryAcquireTargetLock();
}

void UHeroTargetingComponent::ClearTargetLock(const EActionTargetLockBreakReason InReason)
{
	const AActor* PreviousTarget = LockedTargetActor.Get();

	bTargetLockActive = false;
	LockedTargetActor.Reset();
	LastTargetLockDebugSnapshot.bLockActive = false;
	LastTargetLockDebugSnapshot.LockedTargetActor = nullptr;
	LastTargetLockDebugSnapshot.BreakReason = InReason;
	RefreshMovementOrientationFromLockState();

	if (InReason != EActionTargetLockBreakReason::None)
	{
		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[锁定目标] 已解除：原因=%s 旧目标=%s"),
			HeroTargetingLock::GetBreakReasonText(InReason),
			*GetNameSafe(PreviousTarget));
	}
}

void UHeroTargetingComponent::ResetRuntimeStateForHeroStartup()
{
	ClearTargetLock(EActionTargetLockBreakReason::RuntimeReset);
	LastSimpleTurnAssistDebugSnapshot.Reset();
}

AActionHeroCharacter* UHeroTargetingComponent::GetOwningHeroCharacter() const
{
	if (CachedHeroCharacter.IsValid())
	{
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedHeroCharacter.Get();
}

AActionPlayerController* UHeroTargetingComponent::GetOwningHeroController() const
{
	if (CachedHeroController.IsValid())
	{
		return CachedHeroController.Get();
	}

	if (const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		CachedHeroController = Cast<AActionPlayerController>(OwnerHeroCharacter->GetController());
	}

	return CachedHeroController.Get();
}

bool UHeroTargetingComponent::IsValidSimpleTurnAssistTarget(AActor* InTargetActor) const
{
	if (!IsValid(InTargetActor) || InTargetActor == GetOwner())
	{
		return false;
	}

	const AActionCharacterBase* TargetCharacter = Cast<AActionCharacterBase>(InTargetActor);
	if (!TargetCharacter)
	{
		return false;
	}

	const UActionAttributeSetBase* TargetAttributeSet = TargetCharacter->GetActionAttributeSet();
	return TargetAttributeSet && TargetAttributeSet->IsAlive();
}

bool UHeroTargetingComponent::IsValidTargetLockTarget(AActor* InTargetActor) const
{
	return IsValidSimpleTurnAssistTarget(InTargetActor);
}

bool UHeroTargetingComponent::TryResolveTargetLockCandidate(AActor*& OutTargetActor)
{
	OutTargetActor = nullptr;
	LastTargetLockDebugSnapshot.Reset();

	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !GetWorld() || !TargetLockConfig.IsEnabledConfig())
	{
		UE_LOG(
			LogHeroTargetingComponent,
			Log,
			TEXT("[锁定目标] 获取失败：HeroValid=%d WorldValid=%d ConfigEnabled=%d"),
			OwnerHeroCharacter != nullptr ? 1 : 0,
			GetWorld() != nullptr ? 1 : 0,
			TargetLockConfig.IsEnabledConfig() ? 1 : 0);
		return false;
	}

	FVector SearchForward = OwnerHeroCharacter->GetActorForwardVector();
	if (const AActionPlayerController* OwnerController = GetOwningHeroController())
	{
		const FRotator ControlYawRotation(0.f, OwnerController->GetControlRotation().Yaw, 0.f);
		SearchForward = ControlYawRotation.Vector();
	}

	SearchForward.Z = 0.f;
	SearchForward = SearchForward.GetSafeNormal();
	if (SearchForward.IsNearlyZero())
	{
		return false;
	}

	const FVector SearchStart =
		OwnerHeroCharacter->GetActorLocation() + FVector(0.f, 0.f, TargetLockConfig.AcquireSearchHeightOffset);
	const FVector SearchEnd =
		SearchStart + SearchForward * FMath::Max(TargetLockConfig.AcquireSearchDistance, 0.f);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TargetLockSweep), false);
	QueryParams.AddIgnoredActor(OwnerHeroCharacter);

	TArray<FHitResult> HitResults;
	const bool bHasBlockingHit = GetWorld()->SweepMultiByObjectType(
		HitResults,
		SearchStart,
		SearchEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(TargetLockConfig.AcquireSearchRadius, 0.f)),
		QueryParams);
	if (!bHasBlockingHit)
	{
		return false;
	}

	const float MinFacingDot =
		FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(TargetLockConfig.MaxAcquireAngleDegrees, 0.f, 180.f)));
	AActor* BestTargetActor = nullptr;
	float BestDistanceSquared = FLT_MAX;
	float BestFacingDot = -1.f;
	TSet<const AActor*> EvaluatedTargets;

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* CandidateTarget = HitResult.GetActor();
		if (!IsValid(CandidateTarget) || EvaluatedTargets.Contains(CandidateTarget))
		{
			continue;
		}

		EvaluatedTargets.Add(CandidateTarget);
		if (!IsValidTargetLockTarget(CandidateTarget))
		{
			continue;
		}

		FVector ToTarget = CandidateTarget->GetActorLocation() - SearchStart;
		ToTarget.Z = 0.f;
		const float DistanceSquared = ToTarget.SizeSquared();
		if (DistanceSquared <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector DirectionToTarget = ToTarget.GetSafeNormal();
		const float FacingDot = FVector::DotProduct(SearchForward, DirectionToTarget);
		if (FacingDot < MinFacingDot)
		{
			continue;
		}

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestFacingDot = FacingDot;
			BestTargetActor = CandidateTarget;
		}
	}

	if (!IsValid(BestTargetActor))
	{
		return false;
	}

	OutTargetActor = BestTargetActor;
	LastTargetLockDebugSnapshot.bLastAcquireSucceeded = true;
	LastTargetLockDebugSnapshot.bLockActive = true;
	LastTargetLockDebugSnapshot.LockedTargetActor = BestTargetActor;
	LastTargetLockDebugSnapshot.Distance = FMath::Sqrt(BestDistanceSquared);
	LastTargetLockDebugSnapshot.FacingDot = BestFacingDot;
	LastTargetLockDebugSnapshot.BreakReason = EActionTargetLockBreakReason::None;
	return true;
}

void UHeroTargetingComponent::UpdateTargetLockRuntime(const float DeltaTime)
{
	if (!bTargetLockActive)
	{
		return;
	}

	AActor* TargetActor = LockedTargetActor.Get();
	if (!IsValid(TargetActor))
	{
		ClearTargetLock(EActionTargetLockBreakReason::TargetInvalid);
		return;
	}

	const AActionCharacterBase* TargetCharacter = Cast<AActionCharacterBase>(TargetActor);
	const UActionAttributeSetBase* TargetAttributeSet =
		TargetCharacter ? TargetCharacter->GetActionAttributeSet() : nullptr;
	if (!TargetAttributeSet)
	{
		ClearTargetLock(EActionTargetLockBreakReason::TargetInvalid);
		return;
	}

	if (!TargetAttributeSet->IsAlive())
	{
		ClearTargetLock(EActionTargetLockBreakReason::TargetDead);
		return;
	}

	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		ClearTargetLock(EActionTargetLockBreakReason::RuntimeReset);
		return;
	}

	FVector ToTarget = TargetActor->GetActorLocation() - OwnerHeroCharacter->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.SizeSquared() > FMath::Square(FMath::Max(TargetLockConfig.BreakDistance, 0.f)))
	{
		ClearTargetLock(EActionTargetLockBreakReason::OutOfRange);
		return;
	}

	RefreshTargetLockSnapshotFromTarget(TargetActor);
	UpdateTargetLockFacing(DeltaTime);
}

void UHeroTargetingComponent::UpdateTargetLockFacing(const float DeltaTime)
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	AActor* TargetActor = LockedTargetActor.Get();
	if (!OwnerHeroCharacter || !IsValid(TargetActor))
	{
		return;
	}

	FVector ToTarget = TargetActor->GetActorLocation() - OwnerHeroCharacter->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentRotation = OwnerHeroCharacter->GetActorRotation();
	const FRotator TargetRotation = ToTarget.GetSafeNormal().Rotation();
	const FRotator NewRotation = FMath::RInterpTo(
		CurrentRotation,
		FRotator(0.f, TargetRotation.Yaw, 0.f),
		DeltaTime,
		FMath::Max(TargetLockConfig.ActorYawInterpSpeed, 0.f));
	OwnerHeroCharacter->SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));
	RefreshMovementOrientationFromLockState();
}

void UHeroTargetingComponent::RefreshTargetLockSnapshotFromTarget(AActor* InTargetActor)
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !IsValid(InTargetActor))
	{
		return;
	}

	FVector ToTarget = InTargetActor->GetActorLocation() - OwnerHeroCharacter->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FVector CurrentForward = OwnerHeroCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	LastTargetLockDebugSnapshot.bLockActive = bTargetLockActive;
	LastTargetLockDebugSnapshot.LockedTargetActor = InTargetActor;
	LastTargetLockDebugSnapshot.Distance = ToTarget.Size();
	LastTargetLockDebugSnapshot.FacingDot =
		CurrentForward.IsNearlyZero() ? 0.f : FVector::DotProduct(CurrentForward, DirectionToTarget);
}

void UHeroTargetingComponent::RefreshMovementOrientationFromLockState() const
{
	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		if (UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter->GetCharacterMovement())
		{
			MovementComponent->bOrientRotationToMovement = !bTargetLockActive;
		}
	}
}
