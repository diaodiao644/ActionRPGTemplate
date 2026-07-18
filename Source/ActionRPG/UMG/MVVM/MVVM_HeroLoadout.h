#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "MVVM_HeroLoadout.generated.h"

class AActionHeroCharacter;
class UHeroAttributeComponent;
class UHeroCombatComponent;
class UHeroLoadoutStateComponent;
class UMVVM_HeroLoadoutSlot;
struct FGameplayAttribute;

/**
 * 英雄装备域 HUD 只读 ViewModel。
 * 它只负责聚合 Hero 当前的装备 UI 快照、startup 状态和少量战斗 UI 状态，
 * 不反向推进装备事务或战斗正式状态。
 */
UCLASS()
class ACTIONRPG_API UMVVM_HeroLoadout : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UMVVM_HeroLoadout();

	/** 绑定 Hero，并开始消费装备域 UI 快照、startup 状态和少量战斗 UI 状态。它只建立 HUD 聚合消费链，不反向推进业务状态。 */
	void InitMVVMWithHeroCharacter(AActionHeroCharacter* InHeroCharacter);
	/** 解绑当前所有宿主引用与委托。 */
	void UninitializeMVVM();
	virtual void BeginDestroy() override;

	/** 以下 getter 返回的都是 HUD 层只读快照，不是新的正式状态源。 */
	EHeroWeaponLoadoutSlot GetCurrentEquippedLoadoutSlot() const { return CurrentEquippedLoadoutSlot; }
	EHeroWeaponLoadoutStartupState GetStartupState() const { return StartupState; }
	float GetStartupProgressRatio() const { return StartupProgressRatio; }
	int32 GetStartupPendingSlotCount() const { return StartupPendingSlotCount; }
	int32 GetStartupTotalSlotCount() const { return StartupTotalSlotCount; }
	const FString& GetStartupFailureReason() const { return StartupFailureReason; }
	bool GetbWeaponSwitchBlockedByCooldown() const { return bWeaponSwitchBlockedByCooldown; }
	bool GetbSpecialWeaponSwitchReady() const { return bSpecialWeaponSwitchReady; }

	UFUNCTION(BlueprintCallable, Category = "MVVM|HeroLoadout", meta = (ToolTip = "读取空手槽的单槽 ViewModel。它只返回 HUD 当前持有的单槽显示快照宿主。"))
	UMVVM_HeroLoadoutSlot* GetUnarmedSlotViewModel() const { return UnarmedSlotViewModel; }

	UFUNCTION(BlueprintCallable, Category = "MVVM|HeroLoadout", meta = (ToolTip = "读取近战槽的单槽 ViewModel。它只返回 HUD 当前持有的单槽显示快照宿主。"))
	UMVVM_HeroLoadoutSlot* GetMeleeSlotViewModel() const { return MeleeSlotViewModel; }

	UFUNCTION(BlueprintCallable, Category = "MVVM|HeroLoadout", meta = (ToolTip = "读取远程槽的单槽 ViewModel。它只返回 HUD 当前持有的单槽显示快照宿主。"))
	UMVVM_HeroLoadoutSlot* GetRangedSlotViewModel() const { return RangedSlotViewModel; }

	UFUNCTION(BlueprintCallable, Category = "MVVM|HeroLoadout", meta = (ToolTip = "读取混合槽的单槽 ViewModel。它只返回 HUD 当前持有的单槽显示快照宿主。"))
	UMVVM_HeroLoadoutSlot* GetHybridSlotViewModel() const { return HybridSlotViewModel; }

protected:
	/** 确保四个固定槽位对应的子 ViewModel 已创建。 */
	void EnsureSlotViewModels();
	/** 从 Hero 当前正式读链刷新整份装备 UI 快照。 */
	void RefreshFromHeroCharacter();
	/** 按固定槽位找到对应的单槽 ViewModel。 */
	UMVVM_HeroLoadoutSlot* FindSlotViewModel(EHeroWeaponLoadoutSlot InLoadoutSlot) const;
	/** 响应装备域 UI 状态变化并刷新 ViewModel。 */
	void HandleLoadoutUIStateChanged();
	/** 响应战斗 UI 状态变化并刷新 ViewModel。 */
	void HandleCombatUIStateChanged();
	/** 响应属性变化并刷新依赖属性的 UI 状态，例如特殊切武就绪态。 */
	void HandleHeroAttributeChange(FGameplayAttribute ChangedAttribute, float OldValue, float NewValue);

protected:
	/** 四个固定槽位的子 ViewModel。它们只消费单槽 UI 快照。 */
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout")
	TObjectPtr<UMVVM_HeroLoadoutSlot> UnarmedSlotViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout")
	TObjectPtr<UMVVM_HeroLoadoutSlot> MeleeSlotViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout")
	TObjectPtr<UMVVM_HeroLoadoutSlot> RangedSlotViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout")
	TObjectPtr<UMVVM_HeroLoadoutSlot> HybridSlotViewModel = nullptr;

	/** 以下字段都是装备域/UI 层显示快照。 */
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的正式已装备槽位快照。它是消费结果，不是新的装备状态源。"))
	EHeroWeaponLoadoutSlot CurrentEquippedLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的 startup 状态快照。它只读正式宿主结果，不反向推进启动链。"))
	EHeroWeaponLoadoutStartupState StartupState = EHeroWeaponLoadoutStartupState::None;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的 startup 进度百分比快照。"))
	float StartupProgressRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的 startup 待完成槽位数快照。"))
	int32 StartupPendingSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的 startup 总槽位数快照。"))
	int32 StartupTotalSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的 startup 失败原因快照。"))
	FString StartupFailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的切武冷却阻塞快照。它只服务界面展示，不是新的战斗状态源。"))
	bool bWeaponSwitchBlockedByCooldown = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadout", FieldNotify, Getter, meta = (ToolTip = "HUD 当前显示用的特殊切武就绪快照。它依赖正式属性与战斗状态消费结果，不是新的正式状态源。"))
	bool bSpecialWeaponSwitchReady = false;

private:
	/** 当前绑定的 Hero 引用。它只是消费链宿主绑定关系，不是新的角色状态镜像。 */
	TWeakObjectPtr<AActionHeroCharacter> HeroCharacterPtr = nullptr;
	/** 当前绑定的装备域 UI 快照宿主。它只是正式读取入口绑定关系。 */
	TWeakObjectPtr<UHeroLoadoutStateComponent> HeroLoadoutStateComponentPtr = nullptr;
	/** 当前绑定的战斗 UI 状态宿主。它只服务少量 UI 状态消费，不替代战斗正式状态。 */
	TWeakObjectPtr<UHeroCombatComponent> HeroCombatComponentPtr = nullptr;
	/** 当前绑定的属性广播桥。它只服务特殊切武就绪态等 UI 刷新，不是属性宿主。 */
	TWeakObjectPtr<UHeroAttributeComponent> HeroAttributeComponentPtr = nullptr;
	/** 装备域 UI 状态变化委托句柄。它只是绑定收尾辅助，不构成新的运行态。 */
	FDelegateHandle LoadoutUIStateChangedHandle;
	/** 战斗 UI 状态变化委托句柄。它只是绑定收尾辅助，不构成新的运行态。 */
	FDelegateHandle CombatUIStateChangedHandle;
};
