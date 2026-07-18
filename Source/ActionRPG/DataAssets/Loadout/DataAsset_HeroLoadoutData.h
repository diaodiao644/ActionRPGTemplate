// 文件说明：声明英雄负载配置资产，统一描述通用能力与固定武器槽默认配置。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ActionType/ActionAbilityTypes.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "DataAsset_HeroLoadoutData.generated.h"

/**
 * 英雄负载配置资产。
 * 它是 Hero 负载静态模板入口，不是运行时装备状态、当前输入状态或当前攻击状态源。
 * 当前职责分为两部分：
 * 1. 提供角色进入游戏时就应存在的常驻 / 事件驱动 / 通用输入能力静态授予模板；
 * 2. 提供固定武器槽的默认武器配置，以及每个槽位自己的主攻击能力与额外战斗能力静态授予模板。
 *
 * 这样处理后：
 * 1. 通用能力仍然从统一数据资产初始化；
 * 2. 槽位主攻击能力、额外战斗能力与默认槽位武器配置也集中放在同一份角色负载配置中；
 * 3. 武器定义本身只负责武器数据，不再负责“授予哪套战斗 GA”。
 *
 * 这里回答的是“角色一开始拥有哪些能力、固定四槽默认装什么、每个槽授什么”；
 * 它不回答“当前正式装备了什么”或“某条 Ability 当前是否已经在跑”。
 */
UCLASS(BlueprintType)
class ACTIONRPG_API UDataAsset_HeroLoadoutData : public UDataAsset
{
	GENERATED_BODY()

public:
	UDataAsset_HeroLoadoutData();

public:
	// 角色通用能力入口：
	// 这里放与具体武器槽无关的常驻能力、事件驱动能力和全局输入能力。
	/** 进入游戏后立刻授予，且通常会按自身策略自动触发的通用能力静态模板。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LoadoutData|Ability", meta = (ToolTip = "角色进入游戏后立刻授予的通用能力静态模板。适合放授予后自动生效、或不依赖武器槽的常驻能力；它不是已授予快照或已激活快照。"))
	TArray<FActionAbilitySet> ActivateOnGivenAbilities;

	/** 用来响应受击、状态事件等战斗事件的通用能力静态模板。*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LoadoutData|Ability", meta = (ToolTip = "用于响应受击、打断、状态变化等战斗事件的通用能力静态模板。不要把固定武器槽主攻击能力放在这里；它不是运行时事件结果或已触发能力列表。"))
	TArray<FActionAbilitySet> ReactiveAbilities;

	/**
	 * 与具体武器槽无关、但仍然通过输入驱动的通用能力。
	 * 例如处决、切武请求这类全局逻辑，可以继续放在这里。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LoadoutData|Ability", meta = (ToolTip = "与武器槽无关、但依旧由输入驱动的常驻能力静态模板。适合放处决、切武请求这类全局输入能力；这里的 InputTag 仍只是授予链静态入口，不是当前输入运行态。"))
	TArray<FActionAbilitySet> PersistentInputAbilities;

	/**
	 * 默认固定武器槽配置。
	 * 约束如下：
	 * 1. 顺序固定为：空手槽、近战槽、远程槽、混合槽；
	 * 2. 蓝图中只允许修改每个槽位内部的数据，不允许新增或删除槽位；
	 * 3. 每个槽位除了默认武器定义外，还负责保存自己的主攻击能力模板与额外战斗能力。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LoadoutData|Weapon", meta = (EditFixedSize, ToolTip = "固定四槽静态入口。顺序始终为：空手、近战、远程、混合。只修改槽内数据，不要把它当成可增删的背包列表，也不要把它误读成当前 runtime 装备结果。"))
	TArray<FHeroWeaponLoadoutDefinition> WeaponLoadoutDefinitions;

public:
	// 固定四槽编辑器整理工具：
	// 负责保证负载资产始终回到空手、近战、远程、混合四槽这一标准结构。
	/**
	 * 规范化固定武器槽数组。
	 * 该操作会保证：
	 * 1. 始终只保留四个固定槽；
	 * 2. 顺序始终为：空手、近战、远程、混合；
	 * 3. 已配置好的默认武器资产、主攻击能力与额外战斗能力会被尽量保留。
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "EditorTools|WeaponLoadout", meta = (DisplayName = "规范化固定武器槽数组", ToolTip = "将固定武器槽强制整理为：空手、近战、远程、混合四槽顺序，并尽量保留已配置的武器与能力数据。适合在槽位顺序失序或缺项时直接修复当前正式结构。"))
	void NormalizeWeaponLoadoutDefinitions();

	/**
	 * 重建默认四槽结构。
	 * 该操作只重建槽位默认结构，不会替你自动创建武器资产；
	 * 但会保留已经写进对应槽位的默认武器、主攻击能力与额外战斗能力。
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "EditorTools|WeaponLoadout", meta = (DisplayName = "重建默认四槽结构", ToolTip = "重建固定四槽结构，但会尽量保留对应槽位里已经配置的默认武器、主攻击能力和额外能力。适合槽位顺序混乱时使用。"))
	void ResetWeaponLoadoutDefinitionsToDefault();

	// 四槽推荐能力模板工具：
	// 负责把每个固定槽位应授予的五条主攻击 GA 和常用额外能力一次性补齐。
	/** 一键为四个固定武器槽填充推荐的主攻击能力模板与常用额外能力。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "EditorTools|WeaponLoadout", meta = (DisplayName = "套用四槽推荐能力模板", ToolTip = "为四个固定武器槽统一填充当前框架推荐的主攻击能力模板与常用额外战斗能力。它属于推荐模板覆盖入口，不是保守补结构工具。"))
	void ApplyRecommendedWeaponLoadoutAbilityTemplates();

	/** 仅为固定空手槽填充推荐能力模板。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "EditorTools|WeaponLoadout", meta = (DisplayName = "套用空手槽推荐能力模板", ToolTip = "只覆盖空手槽的主攻击能力模板与额外战斗能力，不影响另外三个固定武器槽。适合单独重配空手槽。"))
	void ApplyRecommendedUnarmedLoadoutAbilityTemplate();

	/** 仅为固定近战槽填充推荐能力模板。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "EditorTools|WeaponLoadout", meta = (DisplayName = "套用近战槽推荐能力模板", ToolTip = "只覆盖近战槽的主攻击能力模板与额外战斗能力，不影响其他槽位。适合单独重配近战槽。"))
	void ApplyRecommendedMeleeLoadoutAbilityTemplate();

	/** 仅为固定远程槽填充推荐能力模板。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "EditorTools|WeaponLoadout", meta = (DisplayName = "套用远程槽推荐能力模板", ToolTip = "只覆盖远程槽的主攻击能力模板与额外战斗能力，不影响其他槽位。当前仍使用统一攻击 GA 框架，但已预留未来远程独立分支。"))
	void ApplyRecommendedRangedLoadoutAbilityTemplate();

	/** 仅为固定混合槽填充推荐能力模板。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "EditorTools|WeaponLoadout", meta = (DisplayName = "套用混合槽推荐能力模板", ToolTip = "只覆盖混合槽的主攻击能力模板与额外战斗能力，不影响其他槽位。当前仍使用统一攻击 GA 框架，但已预留未来混合武器独立分支。"))
	void ApplyRecommendedHybridLoadoutAbilityTemplate();

	// 负载资产校验入口：
	// 用于检查四槽顺序、主攻击 GA、额外能力和默认武器定义是否与当前框架契合。
	/** 构建当前负载资产的编辑器校验报告。它只服务静态模板诊断，不推进运行时装备或授予链。 */
	UFUNCTION(BlueprintPure, Category = "EditorTools|Validation", meta = (DisplayName = "构建负载资产校验报告", ToolTip = "返回当前 HeroLoadoutData 的详细校验结果字符串。它只服务静态模板诊断与编辑器展示，不会推进运行时装备、输入或能力授予链。"))
	FString BuildLoadoutValidationReport() const;

	/** 在日志中输出当前负载资产的编辑器校验报告。它只是编辑器维护入口，不是运行时状态提交点。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "EditorTools|Validation", meta = (DisplayName = "校验并输出负载资产报告", ToolTip = "检查固定四槽顺序、主攻击能力、额外能力和默认武器定义，并把结果输出到日志。它只是编辑器维护入口，不是运行时状态提交点。"))
	void ValidateAndLogLoadoutData() const;

	// 内部标准化构建：
	// 运行时与编辑器工具最终都依赖这一个统一出口来产出标准四槽数组。
	/** 构建一份规范化后的固定武器槽静态结果，避免槽位顺序失序或缺项。它只产出标准化模板，不反向推进运行时装备或授予状态。*/
	void BuildNormalizedWeaponLoadoutDefinitions(TArray<FHeroWeaponLoadoutDefinition>& OutDefinitions) const;
};
