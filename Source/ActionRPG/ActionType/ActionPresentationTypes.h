#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionPresentationTypes.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * 单次命中世界表现的静态表现模板。
 * 它只定义伤害数字样式、命中特效、命中音效和默认表现开关，
 * 不是运行时反馈事件，也不持有命中生命周期状态。
 */
USTRUCT(BlueprintType)
struct FActionHitPresentationConfig
{
	GENERATED_BODY()

public:
	/** 伤害数字样式标签，供 HUD / MVVM / 蓝图把这次命中路由到对应数字表现。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "伤害数字样式标签。HUD / MVVM / 蓝图会用它把这次命中路由到对应数字表现；留空时回退到默认数字样式。"))
	FGameplayTag DamageNumberStyleTag;

	/** 命中世界特效的静态资源引用，不表示它已经在运行时播放。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "命中世界时默认播放的 Niagara 特效。它只是静态资源入口；若同时关闭播放开关，这里即便填了也不会自动播放。"))
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	/** 命中世界音效的静态资源引用，不表示它已经在运行时播放。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "命中世界时默认播放的音效资源。它只是静态资源入口；若关闭播放开关，这里不会被消费。"))
	TSoftObjectPtr<USoundBase> ImpactSound;

	/** 这份静态表现模板默认是否显示伤害数字，不反向裁决命中是否成立。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "这次命中默认是否显示伤害数字。它只影响表现，不会反向决定命中是否成立。"))
	bool bShowDamageNumber = true;

	/** 这份静态表现模板默认是否播放命中特效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "这次命中默认是否播放命中特效。关闭时会忽略上面的特效资源引用。"))
	bool bPlayImpactEffect = true;

	/** 这份静态表现模板默认是否播放命中音效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "这次命中默认是否播放命中音效。关闭时会忽略上面的音效资源引用。"))
	bool bPlayImpactSound = true;

public:
	/** 只判断这份静态表现模板是否配置过正式字段，不参与运行时播放裁决。 */
	bool HasAnyConfiguredValue() const
	{
		return DamageNumberStyleTag.IsValid()
			|| !ImpactEffect.IsNull()
			|| !ImpactSound.IsNull()
			|| !bShowDamageNumber
			|| !bPlayImpactEffect
			|| !bPlayImpactSound;
	}

	/** 只收集这份模板依赖的软资源路径，供外层统一预热，不推进表现播放。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!ImpactEffect.IsNull())
		{
			OutAssetPaths.AddUnique(ImpactEffect.ToSoftObjectPath());
		}

		if (!ImpactSound.IsNull())
		{
			OutAssetPaths.AddUnique(ImpactSound.ToSoftObjectPath());
		}
	}
};

/**
 * 发射物生命周期事件的静态表现模板。
 * 它只描述某个生命周期事件默认要播什么特效 / 音效以及默认是否播放，
 * 不持有发射物实例的运行时生命周期，也不是反馈事件快照。
 */
USTRUCT(BlueprintType)
struct FActionProjectileLifecyclePresentationConfig
{
	GENERATED_BODY()

public:
	/** 生命周期事件对应的特效资源模板，不表示该事件已经发生。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "发射物某个生命周期事件默认播放的特效模板，例如生成、撞墙或销毁。它只是静态资源入口，不代表该生命周期事件已经发生。"))
	TSoftObjectPtr<UNiagaraSystem> Effect;

	/** 生命周期事件对应的音效资源模板，不表示该事件已经发生。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "发射物某个生命周期事件默认播放的音效模板。它只是静态资源入口；若关闭播放开关，这里不会被消费。"))
	TSoftObjectPtr<USoundBase> Sound;

	/** 这份静态表现模板默认是否播放生命周期特效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "当前生命周期事件默认是否播放特效。关闭时会忽略上面的特效资源。"))
	bool bPlayEffect = true;

	/** 这份静态表现模板默认是否播放生命周期音效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "当前生命周期事件默认是否播放音效。关闭时会忽略上面的音效资源。"))
	bool bPlaySound = true;

public:
	/** 只判断这份发射物生命周期静态表现模板是否写过正式字段，不表示某个生命周期事件已经发生。 */
	bool HasAnyConfiguredValue() const
	{
		return !Effect.IsNull()
			|| !Sound.IsNull()
			|| !bPlayEffect
			|| !bPlaySound;
	}

	/** 只收集这份模板依赖的软资源路径，供外层统一预热，不参与某个生命周期事件是否播放的裁决。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!Effect.IsNull())
		{
			OutAssetPaths.AddUnique(Effect.ToSoftObjectPath());
		}

		if (!Sound.IsNull())
		{
			OutAssetPaths.AddUnique(Sound.ToSoftObjectPath());
		}
	}
};
