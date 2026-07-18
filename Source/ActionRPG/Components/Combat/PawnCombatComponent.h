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
	/** 未声明运行语义。 */
	None,
	/** 普通主动动画，不属于受击或处决链。 */
	NonReact,
	/** 当前动画属于 CombatReact 正式受击链。 */
	CombatReact,
	/** 当前动画属于处决链。 */
	Execution
};

USTRUCT(BlueprintType)
struct FActionRunningAnimationReactGuardContext
{
	GENERATED_BODY()

public:
	/** 当前参与受击接管比较的运行中蒙太奇。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** 这段运行中动画的正式语义。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	EActionRunningAnimationSemantic Semantic = EActionRunningAnimationSemantic::None;

	/** 新来的受击至少达到这个优先级，才允许打断当前动画。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (ClampMin = "0"))
	int32 MinIncomingReactPriorityToInterrupt = 0;

	/** 当前 guard context 是否已经具备正式比较资格。 */
	bool IsValid() const
	{
		return Montage != nullptr && Semantic != EActionRunningAnimationSemantic::None;
	}
};

struct FActionMontageRootMotionOverrideRuntime
{
	/** 当前被临时覆写 Root MotionMode 的角色。 */
	TWeakObjectPtr<ACharacter> Character;
	/** 当前要求覆写 Root MotionMode 的蒙太奇。 */
	TWeakObjectPtr<UAnimMontage> Montage;
	/** 覆写前的 Root MotionMode。 */
	TEnumAsByte<ERootMotionMode::Type> PreviousRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	/** 覆写原因名。只服务日志与成对校验。 */
	FName Reason = NAME_None;
	/** 当前这笔 Root Motion 覆写是否仍有效。 */
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
 * 这里只保留各类战斗组件都会共享的最小正式公共状态与查询接口。
 * 它不是战斗总控，也不负责攻击/受击/防御/切武的完整业务逻辑；
 * 子类只把“跨链都需要共享的最小战斗事实”写回这里。
 */
UCLASS()
class ACTIONRPG_API UPawnCombatComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UPawnCombatComponent();

public:
	/** 处理一次战斗事件虚入口。基类不定义具体事件链，正式规则由子类实现。 */
	virtual bool HandleIncomingCombatEvent(FGameplayTag InCombatEventTag, AActor* InstigatorActor);

	/** 尝试处理一次传入伤害。基类只保留统一入口，具体判定与写回由子类实现。 */
	virtual bool TryHandleIncomingDamage(const FActionDamagePayload& InDamagePayload, FActionHitResolveResult& OutResult);

public:
	/** 写入当前装备武器标签。这是最小公共战斗状态，不替代 Equipment 的正式当前装备快照。 */
	void SetCurrentEquippedWeaponTag(FGameplayTag InWeaponTag);

	UFUNCTION(Blueprintpure, meta = (BlueprintThreadSafe))
	/** 读取当前装备武器标签。它只回答战斗公共状态，不反查槽位配置或武器定义。 */
	FGameplayTag GetCurrentEquippedWeaponTag() const;

	/** 写入当前装备武器类别。 */
	void SetCurrentEquippedWeaponCategory(EHeroWeaponCategory InWeaponCategory);

	/** 读取当前装备武器类别。 */
	EHeroWeaponCategory GetCurrentEquippedWeaponCategory() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	/** 读取当前登记中的运行蒙太奇。它是公共运行引用，不保证对应链路一定仍有效。 */
	virtual UAnimMontage* GetCurrentRunningAnimMontage() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	/** 读取战斗模式切换蒙太奇。基类保留统一查询口，具体资源由子类决定。 */
	virtual UAnimMontage* GetCombatModeTransitionAnimMontage();

	/** 更新当前公共运行蒙太奇引用。 */
	void UpdateRunningAnimMontage(UAnimMontage* InNewMontage);
	/** 直接清空当前公共运行蒙太奇引用。 */
	void ClearRunningAnimMontageReference();
	/** 仅当当前引用仍命中指定蒙太奇时才清空，避免旧回调误收新蒙太奇。 */
	bool ClearRunningAnimMontageReferenceIfMatches(const UAnimMontage* InMontage);
	/** 停止当前登记中的攻击蒙太奇。 */
	void StopCurrentAttackMontage();
	/** 设置当前运行动画的受击接管保护上下文。 */
	void SetRunningAnimationReactGuardContext(
		UAnimMontage* InMontage,
		EActionRunningAnimationSemantic InSemantic,
		int32 InMinIncomingReactPriorityToInterrupt);
	/** 清空当前运行动画的受击接管保护上下文。 */
	void ClearRunningAnimationReactGuardContext();
	/** 仅在上下文仍命中当前动画时清理保护上下文。 */
	bool ClearRunningAnimationReactGuardContextIfMatches(
		const UAnimMontage* InMontage,
		EActionRunningAnimationSemantic InSemantic = EActionRunningAnimationSemantic::None);
	/** 读取当前运行动画的受击接管保护上下文。 */
	const FActionRunningAnimationReactGuardContext& GetRunningAnimationReactGuardContext() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	/** 判断当前运行动画是否仍受受击接管保护。 */
	bool IsCurrentRunningAnimationProtectedFromIncomingReact(int32 IncomingPriority) const;

	/** 临时把某段蒙太奇切到指定 Root Motion 覆写模式。 */
	bool BeginMontageRootMotionOverride(ACharacter* Character, UAnimMontage* Montage, FName Reason);
	/** 收掉当前蒙太奇 Root Motion 覆写。 */
	void EndMontageRootMotionOverride(ACharacter* Character, UAnimMontage* Montage, FName Reason);

public:
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	/** 读取当前连段索引。 */
	int32 GetComboIndex() const;

	/** 按当前分支可用的蒙太奇数量推进一次连段索引。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void AdvanceComboIndex(int32 InComboLength);

	/** 直接写入当前连段上限。它只服务公共连段状态，不负责解析攻击分支。 */
	void UpdateComboMaxIndex(int32 Num);

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void ResetComboIndex();

public:
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	/** 写入当前是否允许攻击。 */
	void SetAttackEnabled(bool bInAttackEnabled);

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	/** 查询当前是否允许攻击。 */
	bool IsAttackEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	/** 写入当前战斗模式。 */
	virtual void SetCombatMode(EHeroCombatMode InCombatMode);

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	/** 读取当前战斗模式。 */
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
