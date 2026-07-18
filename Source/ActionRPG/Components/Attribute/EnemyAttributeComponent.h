// 文件说明：声明敌方属性变化广播桥组件。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeSet.h"
#include "EnemyAttributeComponent.generated.h"

class UActionAbilitySystemComponent;
class UActionAttributeSetBase;
struct FOnAttributeChangeData;

/** 敌方属性变化广播。它只转发 ASC / AttributeSet 的变化结果，不持有第二套属性值状态，也不替代 AttributeSet 本体。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyAttributeChange,
	FGameplayAttribute, ChangedAttribute,
	float, OldValue,
	float, NewValue
);

/**
 * 敌方属性变化广播桥组件。
 * 它只负责：
 * 1. 绑定敌方 ASC 属性变化委托；
 * 2. 把属性变化统一转成蓝图可消费的广播；
 * 3. 在 UI / 调试初绑后补发一次当前值。
 * 数值本体权威仍在 `UActionAttributeSetBase`，这里不是第二套属性缓存宿主。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UEnemyAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyAttributeComponent();

	/** 按 Actor 查找敌方属性广播桥。它只是宿主解析入口，不是属性状态查询壳，也不创建新的绑定关系。 */
	UFUNCTION(BlueprintPure, Category = "Enemy|AttributeCmt", meta = (ToolTip = "按 Actor 查找敌方属性广播桥。它只是宿主解析入口，不是属性状态查询壳，也不创建新的绑定关系。"))
	static UEnemyAttributeComponent* FindEnemyAttributeCmt(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UEnemyAttributeComponent>() : nullptr); }

	/** 绑定敌方 ASC 与 AttributeSet 的属性变化广播。它是桥接生命周期入口，不是属性初始化或重算入口。 */
	void InitializeWithAbilitySystem(UActionAbilitySystemComponent* InASC);
	/** 解绑当前属性广播桥。 */
	void UnInitializeWithAbilitySystem();

	/** 在 UI / 调试初绑后手动补发一次当前值，确保监听者拿到初始显示快照。它只补广播，不反向推动属性写回。 */
	UFUNCTION(BlueprintCallable, meta = (ToolTip = "在 UI / 调试初绑后手动补发一次当前值，确保监听者拿到初始显示快照。它只补广播，不反向推动属性写回。"))
	void ForceRefreshAttributeChange();

	/** 属性变化回调。它只消费 ASC 广播并继续转发，不创造新的属性状态，也不缓存第二套数值。 */
	void OnRep_EnemyAttributeChange(const FOnAttributeChangeData& Data);

	/** 组件销毁时统一解绑委托。 */
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

protected:
	virtual void BeginPlay() override;

public:
	/** 对外统一属性变化广播。它只服务敌方蓝图 / 宿主 / 调试消费，不是新的正式属性状态源。 */
	UPROPERTY(BlueprintAssignable)
	FOnEnemyAttributeChange OnEnemyAttributeChangeDelegate;

protected:
	/** 当前绑定的属性集引用。它是宿主绑定关系，不是第二套属性缓存。 */
	UPROPERTY()
	TWeakObjectPtr<const UActionAttributeSetBase> OwnerAttributeSet = nullptr;

	/** 当前绑定的 ASC 引用。它只是桥接绑定关系，不替代正式效果或属性状态。 */
	UPROPERTY()
	TWeakObjectPtr<UActionAbilitySystemComponent> OwnerASC = nullptr;
};
