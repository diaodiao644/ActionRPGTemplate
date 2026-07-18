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

	/** 初始化 HUD 宿主。它负责绑定 Hero、反馈组件和顶层 ViewModel，不是 Hero 启动状态源，也不创建第二套业务状态。 */
	void InitPlayerHUD(AActionHeroCharacter* InHeroCharacter);
	/** 释放 HUD 当前绑定的 Hero、反馈组件、ViewModel 与页面桥。 */
	void UninitPlayerHUD();

	/** 读取属性 ViewModel。它是 HUD 层只读消费入口，不反向写业务状态。 */
	UFUNCTION(BlueprintCallable, meta = (ToolTip = "读取 HUD 当前持有的属性 ViewModel。它只是只读消费入口，不反向写业务状态。"))
	UMVVM_HeroAttribute* GetHeroAttributeViewModel() { return HeroAttributeViewModel; }

	/** 读取负载 ViewModel。它只服务 HUD/MVVM 展示。 */
	UFUNCTION(BlueprintCallable, meta = (ToolTip = "读取 HUD 当前持有的装备域负载 ViewModel。它只服务 HUD/MVVM 展示，不是装备域新的正式状态源。"))
	UMVVM_HeroLoadout* GetHeroLoadoutViewModel() { return HeroLoadoutViewModel; }

	/** 从 HUD 发起一次正式装备请求。最终仍回到装备域正式入口处理。 */
	UFUNCTION(BlueprintCallable, meta = (ToolTip = "从 HUD 发起一次正式装备请求。最终仍回到装备域正式入口处理；HUD 这里只负责操作桥，不持有装备事务状态。"))
	bool RequestEquipWeaponByLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:

	/** 初始化 MVVM 相关内容，创建并绑定属性/负载 ViewModel。它只建立 HUD 消费链，不创建第二套业务状态。 */
	void InitMVVM(AActionHeroCharacter* InHeroCharacter);

	/** 蓝图页面创建桥。C++ 只负责调用时机，具体页面结构交给蓝图。 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, DisplayName = "CreateWidgetForPlayer", meta = (ToolTip = "蓝图页面创建桥。C++ 只负责调用时机，具体页面结构和表现交给蓝图。"))
	void k2_CreateWidgetForPlayer();

	/** 蓝图页面销毁桥。 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, DisplayName = "DestroyWidgetForPlayer", meta = (ToolTip = "蓝图页面销毁桥。它只服务 HUD 页面收尾，不推进业务正式状态。"))
	void k2_DestroyWidgetForPlayer();

	/** 蓝图反馈展示桥。只服务 HUD 表现层，例如伤害数字或提示，不推进命中结果。 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, DisplayName = "OnCombatFeedbackEvent", meta = (ToolTip = "蓝图反馈展示桥。它只服务 HUD 表现层，例如伤害数字或提示，不推进命中结果或战斗正式状态。"))
	void K2_OnCombatFeedbackEvent(FActionCombatFeedbackEvent CombatFeedbackEvent);

	/** 处理过滤后的 HUD 展示反馈事件，再桥接给蓝图页面。这里消费的是反馈事件快照，不是新的战斗状态镜像。 */
	UFUNCTION()
	void HandleCombatFeedbackHUDDisplayEvent(FActionCombatFeedbackEvent CombatFeedbackEvent);

	/** HUD 当前持有的属性 ViewModel。它只是 HUD 只读消费结果，不是属性正式状态源。 */
	UPROPERTY(BlueprintReadOnly, meta = (ToolTip = "HUD 当前持有的属性 ViewModel。它只是 HUD 只读消费结果，不是属性正式状态源。"))
	TObjectPtr<UMVVM_HeroAttribute> HeroAttributeViewModel = nullptr;

	/** HUD 当前持有的负载 ViewModel。它只是 HUD 只读消费结果，不是装备域正式状态源。 */
	UPROPERTY(BlueprintReadOnly, meta = (ToolTip = "HUD 当前持有的装备域负载 ViewModel。它只是 HUD 只读消费结果，不是装备域正式状态源。"))
	TObjectPtr<UMVVM_HeroLoadout> HeroLoadoutViewModel = nullptr;

	/** 当前绑定的反馈组件引用。它是宿主绑定关系，不是战斗状态镜像。 */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (ToolTip = "当前绑定的反馈组件引用。它只是 HUD 与反馈总线之间的宿主绑定关系，不是战斗状态镜像。"))
	TObjectPtr<UHeroCombatFeedbackComponent> BoundCombatFeedbackComponent = nullptr;

	/** 当前绑定的 Hero 引用。它只是 HUD 宿主绑定关系，不是新的角色状态镜像。 */
	TWeakObjectPtr<AActionHeroCharacter> BoundHeroCharacter;
};
