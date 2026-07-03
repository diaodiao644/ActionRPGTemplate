#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimEnums.h"
#include "Components/PawnExtensionComponentBase.h"
#include "GameplayTagContainer.h"
#include "ActionGameplayTags.h"
#include "ActionType/ActionCombatRuntimeTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionAnimationTypes.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "PawnCombatComponent.generated.h"

class ACharacter;

UENUM(BlueprintType)
enum class EActionRunningAnimationSemantic : uint8
{
	None,
	NonReact,
	CombatReact,
	Execution
};

USTRUCT(BlueprintType)
struct FActionRunningAnimationReactGuardContext
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	EActionRunningAnimationSemantic Semantic = EActionRunningAnimationSemantic::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (ClampMin = "0"))
	int32 MinIncomingReactPriorityToInterrupt = 0;

	bool IsValid() const
	{
		return Montage != nullptr && Semantic != EActionRunningAnimationSemantic::None;
	}
};

struct FActionMontageRootMotionOverrideRuntime
{
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UAnimMontage> Montage;
	TEnumAsByte<ERootMotionMode::Type> PreviousRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	FName Reason = NAME_None;
	bool bActive = false;

	void Reset()
	{
		Character.Reset();
		Montage.Reset();
		PreviousRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
		Reason = NAME_None;
		bActive = false;
	}
};

/**
 * 所有战斗组件的基础类。
 * 这里只保留各类战斗组件都会共享的最小状态与查询接口。
 */
UCLASS()
class ACTIONRPG_API UPawnCombatComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UPawnCombatComponent();

public:
	/** 处理一次战斗反应事件。具体逻辑由子类实现。 */
	virtual bool HandleIncomingCombatEvent(FGameplayTag InCombatEventTag, AActor* InstigatorActor);

	/** 尝试处理一次传入伤害。具体规则由子类实现。 */
	virtual bool TryHandleIncomingDamage(const FActionDamagePayload& InDamagePayload, FActionHitResolveResult& OutResult);

public:
	void SetCurrentEquippedWeaponTag(FGameplayTag InWeaponTag);

	UFUNCTION(Blueprintpure, meta = (BlueprintThreadSafe))
	FGameplayTag GetCurrentEquippedWeaponTag() const;

	void SetCurrentEquippedWeaponCategory(EHeroWeaponCategory InWeaponCategory);

	EHeroWeaponCategory GetCurrentEquippedWeaponCategory() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual UAnimMontage* GetCurrentRunningAnimMontage() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual UAnimMontage* GetCombatModeTransitionAnimMontage();

	void UpdateRunningAnimMontage(UAnimMontage* InNewMontage);
	void ClearRunningAnimMontageReference();
	bool ClearRunningAnimMontageReferenceIfMatches(const UAnimMontage* InMontage);
	void StopCurrentAttackMontage();
	void SetRunningAnimationReactGuardContext(
		UAnimMontage* InMontage,
		EActionRunningAnimationSemantic InSemantic,
		int32 InMinIncomingReactPriorityToInterrupt);
	void ClearRunningAnimationReactGuardContext();
	bool ClearRunningAnimationReactGuardContextIfMatches(
		const UAnimMontage* InMontage,
		EActionRunningAnimationSemantic InSemantic = EActionRunningAnimationSemantic::None);
	const FActionRunningAnimationReactGuardContext& GetRunningAnimationReactGuardContext() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	bool IsCurrentRunningAnimationProtectedFromIncomingReact(int32 IncomingPriority) const;

	bool BeginMontageRootMotionOverride(ACharacter* Character, UAnimMontage* Montage, FName Reason);
	void EndMontageRootMotionOverride(ACharacter* Character, UAnimMontage* Montage, FName Reason);

public:
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	int32 GetComboIndex() const;

	/** 按当前分支可用的蒙太奇数量推进一次连段索引。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void AdvanceComboIndex(int32 InComboLength);

	/** 直接写入当前连段上限。 */
	void UpdateComboMaxIndex(int32 Num);

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void ResetComboIndex();

public:
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void SetAttackEnabled(bool bInAttackEnabled);

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	bool IsAttackEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual void SetCombatMode(EHeroCombatMode InCombatMode);

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	EHeroCombatMode GetCombatMode() const;

public:
	/** 当前装备武器标签。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "CombatConfig")
	FGameplayTag CurrentEquippedWeaponTag = ActionGameplayTags::Player_Weapon_Unarmed_Default;

protected:
	/** 当前连段索引。 */
	int32 ComboIndex = 0;

	/** 当前允许的最大连段数量。 */
	int32 ComboMaxIndex = 0;

	/** 当前是否允许主攻击输入与依赖攻击开关的战斗操作。 */
	bool bAttackEnabled = true;

	/** 当前战斗模式。 */
	EHeroCombatMode CombatMode = EHeroCombatMode::Idle;

	/** 当前记录中的运行蒙太奇。 */
	UAnimMontage* CurrentRunningAnimMontage = nullptr;

	/** 当前运行中动画用于受击接管比较的正式上下文。 */
	FActionRunningAnimationReactGuardContext RunningAnimationReactGuardContext;

	/** 当前由战斗链临时接管的蒙太奇 Root Motion 模式。 */
	FActionMontageRootMotionOverrideRuntime MontageRootMotionOverrideRuntime;

	/** 当前装备武器类别。 */
	EHeroWeaponCategory CurrentEquippedWeaponCategory = EHeroWeaponCategory::Unarmed;
};
