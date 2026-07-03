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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FGameplayTag DamageNumberStyleTag;

	/** 命中世界特效的静态资源引用，不表示它已经在运行时播放。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	/** 命中世界音效的静态资源引用，不表示它已经在运行时播放。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<USoundBase> ImpactSound;

	/** 这份静态表现模板默认是否显示伤害数字，不反向裁决命中是否成立。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	bool bShowDamageNumber = true;

	/** 这份静态表现模板默认是否播放命中特效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	bool bPlayImpactEffect = true;

	/** 这份静态表现模板默认是否播放命中音效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	bool bPlayImpactSound = true;

public:
	/** 只判断这份静态表现模板是否配置过正式字段。 */
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UNiagaraSystem> Effect;

	/** 生命周期事件对应的音效资源模板，不表示该事件已经发生。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<USoundBase> Sound;

	/** 这份静态表现模板默认是否播放生命周期特效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	bool bPlayEffect = true;

	/** 这份静态表现模板默认是否播放生命周期音效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	bool bPlaySound = true;

public:
	/** 只判断这份发射物生命周期静态表现模板是否写过正式字段。 */
	bool HasAnyConfiguredValue() const
	{
		return !Effect.IsNull()
			|| !Sound.IsNull()
			|| !bPlayEffect
			|| !bPlaySound;
	}

	/** 只收集这份模板依赖的软资源路径，供外层统一预热。 */
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
