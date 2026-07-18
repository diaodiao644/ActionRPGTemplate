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
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon")
	FGameplayTag GetWeaponTag() const;

	/** 读取当前武器 subtype Tag。武器小类别标签以 WeaponDefinition 为唯一来源。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon")
	FGameplayTag GetWeaponSubtypeTag() const;

	/** 读取当前武器实例绑定的 WeaponDefinition。它是动画、命中和标签语义的正式入口。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon")
	UDataAsset_WeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

	/** 为指定目标构建这把武器当前会打出的伤害载荷。返回的是已解析好的运行时载荷，不是资产快照。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon|Hit")
	FActionDamagePayload BuildDamagePayloadForTarget(AActor* OtherActor) const;

	/** 为指定目标构建处决专用伤害载荷。返回的是本次读取并拼装好的运行时 payload，不是目标侧处决窗口状态。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon|Execution")
	FActionDamagePayload BuildExecutionDamagePayloadForTarget(AActor* OtherActor) const;

	/** 为当前武器实例写入 WeaponDefinition。正式应在装备/生成链中调用，不建议在战斗过程中临时切换。 */
	void SetWeaponDefinition(UDataAsset_WeaponDefinition* InWeaponDefinition);

	/** 按指定基础命中配置与发射物配置构建发射物命中载荷模板。结果会交给单枚发射物初始化链，不会反写资产。 */
	bool BuildProjectileDamagePayloadTemplate(
		const FActionWeaponHitConfig& InBaseHitConfig,
		const FActionProjectileConfig& InProjectileConfig,
		FActionDamagePayload& OutDamagePayload) const;

protected:
	// 生命周期接口。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** 默认近战命中路径下的载荷构建。它会把 `WeaponDefinition` 中的正式命中语义桥接成运行时载荷，不创建第二套武器配置状态。 */
	virtual FActionDamagePayload BuildDamagePayload(AActor* OtherActor) const override;
	/** 按指定命中源构建载荷，并补齐来源组件语义。这里补的是“这次 payload 从哪里来”，不是正式命中窗口的开关宿主。 */
	virtual FActionDamagePayload BuildDamagePayloadForHitSource(
		AActor* OtherActor,
		FName InSourceId,
		const UPrimitiveComponent* InSourceComponent) const override;

	/** 按一份命中配置构建正式命中载荷。它是 WeaponDefinition 到运行时 DamagePayload 的桥接入口。 */
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

	/** 把当前武器上下文对应的命中来源语义写入载荷。它只补来源信息，不替代 HeroHitSourceComponent 的正式命中窗口状态。 */
	void FillWeaponHitSourceInfo(
		FActionHitSourceInfo& InOutHitSourceInfo,
		FName InPreferredSourceId = NAME_None,
		FName InPreferredSourceComponentName = NAME_None) const;

	// 当前武器实例绑定的武器定义资产。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ConfigData", meta = (ToolTip = "当前武器实例绑定的 WeaponDefinition。它是武器标签、命中语义和动画配置的唯一正式数据来源。"))
	UDataAsset_WeaponDefinition* WeaponDefinition = nullptr;
};
