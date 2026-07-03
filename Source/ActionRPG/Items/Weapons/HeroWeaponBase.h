// 文件说明：声明 HeroWeaponBase 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionProjectileTypes.h"
#include "Items/Weapons/WeaponBase.h"
#include "GameplayTagContainer.h"
#include "HeroWeaponBase.generated.h"

class UDataAsset_WeaponDefinition;

UCLASS()
class ACTIONRPG_API AHeroWeaponBase : public AWeaponBase
{
	GENERATED_BODY()

public:
	AHeroWeaponBase();

public:
	/** 读取当前武器 Tag。武器标签以 WeaponDefinition 为唯一来源。 */
	UFUNCTION(BlueprintCallable)
	FGameplayTag GetWeaponTag() const;

	/** 读取当前武器 subtype Tag。武器小类别标签以 WeaponDefinition 为唯一来源。 */
	UFUNCTION(BlueprintCallable)
	FGameplayTag GetWeaponSubtypeTag() const;

	UFUNCTION(BlueprintCallable)
	UDataAsset_WeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

	/** 为指定目标构建这把武器当前会打出的伤害载荷。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon|Hit")
	FActionDamagePayload BuildDamagePayloadForTarget(AActor* OtherActor) const;

	/** 为指定目标构建处决专用伤害载荷。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon|Execution")
	FActionDamagePayload BuildExecutionDamagePayloadForTarget(AActor* OtherActor) const;

	void SetWeaponDefinition(UDataAsset_WeaponDefinition* InWeaponDefinition);

	/** 按指定基础命中配置与发射物配置构建发射物命中载荷模板。 */
	bool BuildProjectileDamagePayloadTemplate(
		const FActionWeaponHitConfig& InBaseHitConfig,
		const FActionProjectileConfig& InProjectileConfig,
		FActionDamagePayload& OutDamagePayload) const;

protected:
	// 生命周期接口。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FActionDamagePayload BuildDamagePayload(AActor* OtherActor) const override;
	virtual FActionDamagePayload BuildDamagePayloadForHitSource(
		AActor* OtherActor,
		FName InSourceId,
		const UPrimitiveComponent* InSourceComponent) const override;

	/** 按一份命中配置构建正式命中载荷。 */
	bool BuildDamagePayloadFromHitConfig(
		AActor* OtherActor,
		const FActionWeaponHitConfig& InHitConfig,
		FActionDamagePayload& OutDamagePayload,
		FName InPreferredSourceId = NAME_None,
		FName InPreferredSourceComponentName = NAME_None) const;

	/** 按一份处决专用命中配置构建正式处决伤害载荷。 */
	bool BuildExecutionDamagePayloadFromConfig(
		AActor* OtherActor,
		const FActionExecutionHitConfig& InHitConfig,
		FActionDamagePayload& OutDamagePayload) const;

	/** 把当前武器上下文对应的命中来源语义写入载荷。 */
	void FillWeaponHitSourceInfo(
		FActionHitSourceInfo& InOutHitSourceInfo,
		FName InPreferredSourceId = NAME_None,
		FName InPreferredSourceComponentName = NAME_None) const;

	// 当前武器实例绑定的武器定义资产。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ConfigData")
	UDataAsset_WeaponDefinition* WeaponDefinition = nullptr;
};
