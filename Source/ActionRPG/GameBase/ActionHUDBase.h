// 文件说明：声明 ActionHUDBase 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "ActionType/ActionCombatFeedbackTypes.h"
#include "GameFramework/HUD.h"
#include "ActionHUDBase.generated.h"

class UHeroCombatFeedbackComponent;
class UMVVM_HeroLoadout;
class UMVVM_HeroAttribute;
class AActionHeroCharacter;
class UHeroAttributeComponent;

UCLASS()
class ACTIONRPG_API AActionHUDBase : public AHUD
{
	GENERATED_BODY()

public:
	AActionHUDBase();

	// HUD初始化函数
	void InitPlayerHUD(AActionHeroCharacter* InHeroCharacter);
	void UninitPlayerHUD();

	UFUNCTION(BlueprintCallable)
	UMVVM_HeroAttribute* GetHeroAttributeViewModel() { return HeroAttributeViewModel; }

	UFUNCTION(BlueprintCallable)
	UMVVM_HeroLoadout* GetHeroLoadoutViewModel() { return HeroLoadoutViewModel; }

	UFUNCTION(BlueprintCallable)
	bool RequestEquipWeaponByLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:

	// 初始化 MVVM 相关内容的函数，创建 ViewModel 实例并进行初始化，传入属性组件指针等
	void InitMVVM(AActionHeroCharacter* InHeroCharacter);

	// 蓝图事件，创建并添加 Widget 到玩家界面上，具体实现交给蓝图
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, DisplayName = "CreateWidgetForPlayer")
	void k2_CreateWidgetForPlayer();

	// 蓝图事件，从玩家界面上移除 Widget，具体实现交给蓝图
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, DisplayName = "DestroyWidgetForPlayer")
	void k2_DestroyWidgetForPlayer();

	// 蓝图事件，供 HUD 处理伤害数字或其他文本类反馈。
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, DisplayName = "OnCombatFeedbackEvent")
	void K2_OnCombatFeedbackEvent(FActionCombatFeedbackEvent CombatFeedbackEvent);

	UFUNCTION()
	void HandleCombatFeedbackHUDDisplayEvent(FActionCombatFeedbackEvent CombatFeedbackEvent);

	UPROPERTY(BlueprintReadOnly) 
	TObjectPtr<UMVVM_HeroAttribute> HeroAttributeViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UMVVM_HeroLoadout> HeroLoadoutViewModel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly)
	TObjectPtr<UHeroCombatFeedbackComponent> BoundCombatFeedbackComponent = nullptr;

	TWeakObjectPtr<AActionHeroCharacter> BoundHeroCharacter;
};
