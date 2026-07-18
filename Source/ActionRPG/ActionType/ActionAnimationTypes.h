#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
#include "GameplayTagContainer.h"
#include "ActionGameplayTags.h"
#include "ActionType/ActionCombatRuntimeTypes.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionType/ActionHitSourceTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionInputTypes.h"
#include "ActionType/ActionProjectileTypes.h"
#include "ActionAnimationTypes.generated.h"

class UActionHeroLinkedAnimLayer;
class UAnimMontage;

/**
 * 单个普通攻击段落配置。
 * 一个 Clip 同时绑定本段要播放的蒙太奇、命中配置和可选发射物配置，
 * 避免连段动画与伤害参数分散在不同字段里产生错位。
 */
USTRUCT(BlueprintType)
struct FActionAttackClipConfig
{
	GENERATED_BODY()

public:
	/** 当前攻击段实际播放的蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前攻击段实际播放的蒙太奇。留空时这段 AttackClip 无法正式起手。"))
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	/** 当前攻击段需要多高优先级的受击才能接管自己。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ClampMin = "0", ToolTip = "当前攻击段需要多高优先级的受击才能接管自己。值越高，越不容易在演出中被普通受击打断。"))
	int32 MinIncomingReactPriorityToInterrupt = 0;

	/** 当前攻击段正式使用的命中配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前攻击段正式使用的命中配置。普通攻击现在只认 Clip 级 HitConfig，不再回退到武器级全局 HitConfig。"))
	FActionWeaponHitConfig HitConfig;

	/** 当前攻击段是否会在动画通知帧额外生成发射物。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack|Projectile", meta = (ToolTip = "当前攻击段是否会在指定动画通知时额外生成发射物。关闭时下面的条目级发射物配置不会被消费。"))
	bool bShouldSpawnProjectile = false;

	/** 当前攻击段的发射物生成配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack|Projectile", meta = (EditCondition = "bShouldSpawnProjectile", EditConditionHides, ToolTip = "当前攻击段的发射物生成配置。只有 bShouldSpawnProjectile 为 true 时才会参与正式解析。"))
	FActionProjectileSpawnConfig ProjectileSpawnConfig;

public:
	/** 只判断这段静态攻击模板是否至少配置了可播放蒙太奇。 */
	bool IsValidConfig() const
	{
		return !AttackMontage.IsNull();
	}

	/** 只读取这段静态攻击模板当前引用的蒙太奇资源。 */
	UAnimMontage* GetMontage() const
	{
		return AttackMontage.Get();
	}

	/** 只收集这段静态攻击模板依赖的软资源路径，不生成任何运行时实例。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!AttackMontage.IsNull())
		{
			OutAssetPaths.AddUnique(AttackMontage.ToSoftObjectPath());
		}

		HitConfig.CollectSoftObjectPaths(OutAssetPaths);

		if (bShouldSpawnProjectile)
		{
			ProjectileSpawnConfig.CollectSoftObjectPaths(OutAssetPaths);
		}
	}
};

/**
 * 单条切武表现段配置。
 * 它只负责描述“这类切武表现播哪条蒙太奇、需要多高优先级受击才能接管、若要命中则用哪份 HitConfig”，
 * 不承载取消窗口白名单或其它运行时时序语义。
 */
USTRUCT(BlueprintType)
struct FActionWeaponSwitchClipConfig
{
	GENERATED_BODY()

public:
	/** 当前切武表现段实际播放的蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|WeaponSwitch", meta = (ToolTip = "当前切武表现段实际播放的蒙太奇。留空时这条切武表现段无效。"))
	TSoftObjectPtr<UAnimMontage> WeaponSwitchMontage;

	/** 当前切武表现段需要多高优先级的受击才能接管自己。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|WeaponSwitch", meta = (ClampMin = "0", ToolTip = "当前切武表现段需要多高优先级的受击才能接管自己。"))
	int32 MinIncomingReactPriorityToInterrupt = 0;

	/** 当前切武表现段正式使用的命中配置。只有动画上显式放了 HitWindow 才会被消费。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|WeaponSwitch", meta = (ToolTip = "当前切武表现段正式使用的命中配置。只有切武蒙太奇上显式存在 HitWindow 通知时才会被消费。"))
	FActionWeaponHitConfig HitConfig;

public:
	/** 只判断这段静态切武模板是否至少配置了可播放蒙太奇。 */
	bool IsValidConfig() const
	{
		return !WeaponSwitchMontage.IsNull();
	}

	/** 只读取这段静态切武模板当前引用的蒙太奇资源。 */
	UAnimMontage* GetMontage() const
	{
		return WeaponSwitchMontage.Get();
	}

	/** 只读取这段静态切武模板当前引用的命中配置。 */
	const FActionWeaponHitConfig& GetHitConfig() const
	{
		return HitConfig;
	}

	/** 只收集这段静态切武模板依赖的软资源路径。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!WeaponSwitchMontage.IsNull())
		{
			OutAssetPaths.AddUnique(WeaponSwitchMontage.ToSoftObjectPath());
		}

		HitConfig.CollectSoftObjectPaths(OutAssetPaths);
	}
};

/**
 * 正式攻击条目配置。
 * 这里收口请求路由、输入阶段和连段推进规则；
 * 真正的动画、命中和发射物数据归每个 AttackClip。
 */
USTRUCT(BlueprintType)
struct FActionAttackEntryConfig
{
	GENERATED_BODY()

public:
	/** 当前条目对应的攻击请求标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前条目对应的攻击请求标签，例如默认攻击、长按重击、闪避反击、冲刺攻击或空中攻击。"))
	FGameplayTag RequestTag = ActionGameplayTags::Attack_Request_Default;

	/** 当前请求最终落到的攻击分支语义标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前请求最终路由到的攻击分支标签。通常与 RequestTag 保持一对一的正式语义映射。"))
	FGameplayTag BranchTag = ActionGameplayTags::Attack_Branch_Light;

	/** 当前请求真正允许出招的输入阶段。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前请求真正允许出招的输入阶段。运行时会用它判断是按下、释放还是长按才正式触发这条攻击。"))
	EActionInputEvent TriggerInputEvent = EActionInputEvent::Released;

	/** 当前条目可使用的攻击段落集合。普通攻击连段按 ComboIndex 选择 Clip。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前条目可使用的攻击段落集合。启用连段索引后，运行时会按 ComboIndex 选择具体 AttackClip。"))
	TArray<FActionAttackClipConfig> AttackClips;

	/** 当前条目是否按连段索引读取攻击段落。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前条目是否按连段索引选择攻击段落。关闭时无论当前 ComboIndex 是多少，都只读取第 0 个 AttackClip。"))
	bool bUseComboIndex = false;

	/** 当前条目播放成功后是否推进一次连段索引。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前条目成功起手后是否推进一次连段索引。通常普通轻击连段会开启它。"))
	bool bAdvanceComboIndexOnPlay = false;

	/** 当前条目播放前是否先把连段索引重置为 0。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前条目播放前是否先把连段索引重置为 0。通常重击、闪避反击和空中攻击会用它来避免串用普通轻击连段索引。"))
	bool bResetComboIndexOnPlay = false;

public:
	/** 只判断这条静态攻击路由模板里是否至少存在一段有效攻击 Clip。 */
	bool IsValidConfig() const
	{
		return AttackClips.ContainsByPredicate([](const FActionAttackClipConfig& AttackClip)
		{
			return AttackClip.IsValidConfig();
		});
	}

	/** 只读取这条静态攻击路由模板当前可用的 Clip 数量。 */
	int32 GetMontageCount() const
	{
		return AttackClips.Num();
	}

	/** 按静态模板规则解析当前连段索引该落到哪一段 Clip，不推进行为提交。 */
	const FActionAttackClipConfig* ResolveAttackClip(const int32 InComboIndex) const
	{
		if (AttackClips.Num() <= 0)
		{
			return nullptr;
		}

		if (!bUseComboIndex)
		{
			return &AttackClips[0];
		}

		const int32 SafeIndex = FMath::Abs(InComboIndex) % AttackClips.Num();
		return AttackClips.IsValidIndex(SafeIndex)
			? &AttackClips[SafeIndex]
			: nullptr;
	}

	/** 只从静态路由模板中读取当前连段索引对应的蒙太奇。 */
	UAnimMontage* ResolveMontage(const int32 InComboIndex) const
	{
		if (const FActionAttackClipConfig* AttackClip = ResolveAttackClip(InComboIndex))
		{
			return AttackClip->GetMontage();
		}

		return nullptr;
	}

	/** 只收集这条静态攻击路由模板依赖的软资源路径。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		for (const FActionAttackClipConfig& AttackClip : AttackClips)
		{
			AttackClip.CollectSoftObjectPaths(OutAssetPaths);
		}
	}
};

/**
 * 一次攻击段落解析后的只读执行计划。
 * 它只表示“当前请求 + 当前连段索引”在静态模板里最终读出了什么，
 * 供攻击 GA 与攻击组件复用，不在这里保存长期攻击状态或命中结果。
 */
USTRUCT(BlueprintType)
struct FActionResolvedAttackExecutionConfig
{
	GENERATED_BODY()

public:
	// 已解析的攻击执行计划：
	// 进入运行时后，攻击 GA 与攻击组件优先消费这份“本次读取结果”，而不是反复回头重查静态模板数组。
	/** 这次攻击实际落到的分支标签。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "这次攻击请求最终解析到的分支标签。它只是本次读取结果，不是长期攻击运行态。"))
	FGameplayTag BranchTag = ActionGameplayTags::Attack_Branch_Light;

	/** 这次攻击实际要播放的蒙太奇。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "这次攻击最终解析出的蒙太奇资源。它来自静态模板读取结果，不代表动画已经成功播放。"))
	TObjectPtr<UAnimMontage> ResolvedMontage = nullptr;

	/** 当前分支是否按连段索引选择蒙太奇。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前分支是否按连段索引解释 AttackClips。它只是把静态模板翻译成执行计划的结果。"))
	bool bUseComboIndex = false;

	/** 当前分支播放成功后是否推进连段索引。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前分支若成功起手，是否建议推进连段索引。它是本次读取到的静态规则，不是运行时推进结果。"))
	bool bAdvanceComboIndexOnPlay = false;

	/** 当前分支播放前是否先重置连段索引。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前分支播放前是否建议先把连段索引重置为 0。它是静态配置解释结果，不是实际重置提交点。"))
	bool bResetComboIndexOnPlay = false;

	/** 当前分支可用的蒙太奇总数，主要用于推进连段索引。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "当前分支模板里可用的 AttackClip 数量。主要用于解释后续连段推进，不是长期状态缓存。"))
	int32 MontageCount = 0;

	/** 这次攻击最终要结算的命中参数。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "这次攻击最终解析出的命中配置。它只是本次读取结果，不是命中结算后的反馈快照。"))
	FActionWeaponHitConfig ResolvedHitConfig;

	/** 这次攻击段允许被来袭受击接管的阈值。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "这次攻击段读取到的受击接管阈值。它只表达静态模板要求，不代表当前已进入受击裁决。"))
	int32 MinIncomingReactPriorityToInterrupt = 0;

	/** 当前攻击段落是否需要在指定动画帧生成发射物。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack|Projectile", meta = (ToolTip = "这次攻击段落是否需要在指定动画帧生成发射物。它只表示本次读取结果，不代表发射物已经生成。"))
	bool bShouldSpawnProjectile = false;

	/** 当前攻击段落最终解析出的发射物生成配置。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponAnimation|Attack|Projectile", meta = (ToolTip = "这次攻击段落最终解析出的发射物生成配置。它只是本次读取结果，不是发射物实例状态。"))
	FActionProjectileSpawnConfig ResolvedProjectileSpawnConfig;

public:
	/** 只判断这份已解析执行计划是否已经落到可播放蒙太奇。 */
	bool IsValid() const
	{
		return ::IsValid(ResolvedMontage);
	}
};

/**
 * 可复用的命中窗口模板。
 * 这里收口的是“某段窗口启用哪些命中源、使用什么结算策略”，
 * 供多个动画通知按名字复用，避免每个 Notify 都重复手填同一套窗口参数。
 */
USTRUCT(BlueprintType)
struct FActionHitWindowTemplateConfig
{
	GENERATED_BODY()

public:
	/** 模板名。动画通知通过这个名字解析模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow", meta = (ToolTip = "命中窗口模板名。动画通知通过这个名字解析模板；同一把武器内应保持唯一。"))
	FName TemplateName = NAME_None;

	/** 当前模板是否同时驱动武器碰撞检测。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow", meta = (ToolTip = "当前模板是否同时驱动武器碰撞检测。关闭后只消费显式启用的命中源列表。"))
	bool bUseWeaponCollisionDetection = true;

	/** 当前模板额外启用的命中源列表。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow", meta = (DisplayName = "EnabledHitSourceIds", ToolTip = "当前模板额外启用的命中源枚举列表。适合补充武器碰撞之外的点位、范围或自定义命中源。"))
	TArray<EActionHitSourceId> EnabledHitSourceIdEnums;

	/** 当前模板额外启用的命中源组列表。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow", meta = (DisplayName = "EnabledHitSourceGroupIds", ToolTip = "当前模板额外启用的命中源组列表。适合批量启用一组相关命中源。"))
	TArray<EActionHitSourceGroupId> EnabledHitSourceGroupIdEnums;

	/** 当前模板内同一来源对同一目标的结算策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow", meta = (ToolTip = "当前模板内同一来源对同一目标的结算策略，例如单次命中或持续接触重复结算。"))
	EActionHitWindowResolvePolicy ResolvePolicy = EActionHitWindowResolvePolicy::SingleHitPerSourceTarget;

	/** 当使用持续接触重复结算策略时，两次结算之间的最小间隔。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow", meta = (ClampMin = "0.01", UIMin = "0.01", EditCondition = "ResolvePolicy == EActionHitWindowResolvePolicy::IntervalWhileOverlapping", EditConditionHides, ToolTip = "持续接触重复结算时，两次结算之间的最小间隔。只在 ResolvePolicy 选择 IntervalWhileOverlapping 时可配置；普通单次命中模板不需要填写这里。"))
	float RepeatResolveInterval = 0.2f;

	/** 当前模板是否显式覆写默认命中配置里的击退强度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow|Hit", meta = (ToolTip = "当前模板是否显式覆写默认命中配置里的击退强度。关闭时继续沿用 HitConfig 或更上层模板里的正式击退配置。"))
	bool bOverrideKnockbackStrength = false;

	/** 当模板显式覆写击退时，本窗口最终使用的击退强度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow|Hit", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bOverrideKnockbackStrength", EditConditionHides, ToolTip = "当模板显式覆写击退时，本窗口最终使用的击退强度。只在 bOverrideKnockbackStrength 为 true 时可配置。"))
	float OverrideKnockbackStrength = 0.f;

public:
	/** 判断模板是否至少具备可解析的名字。 */
	bool HasValidTemplateName() const
	{
		return TemplateName != NAME_None;
	}

	/** 只校验静态命中窗口模板自身是否合法。 */
	bool IsValidTemplate(FString& OutFailureReason) const
	{
		if (!HasValidTemplateName())
		{
			OutFailureReason = TEXT("TemplateName 不能为空。");
			return false;
		}

		for (const EActionHitSourceId HitSourceId : EnabledHitSourceIdEnums)
		{
			if (HitSourceId == EActionHitSourceId::None)
			{
				OutFailureReason = FString::Printf(
					TEXT("模板 %s 的 EnabledHitSourceIds 中存在 None 命中源。"),
					*TemplateName.ToString());
				return false;
			}
		}

		for (const EActionHitSourceGroupId HitSourceGroupId : EnabledHitSourceGroupIdEnums)
		{
			if (HitSourceGroupId == EActionHitSourceGroupId::None)
			{
				OutFailureReason = FString::Printf(
					TEXT("模板 %s 的 EnabledHitSourceGroupIds 中存在 None 命中源组。"),
					*TemplateName.ToString());
				return false;
			}
		}

		if (ResolvePolicy == EActionHitWindowResolvePolicy::IntervalWhileOverlapping
			&& RepeatResolveInterval <= 0.f)
		{
			OutFailureReason = FString::Printf(
				TEXT("模板 %s 使用持续接触重复结算时，RepeatResolveInterval 必须大于 0。"),
				*TemplateName.ToString());
			return false;
		}

		OutFailureReason.Reset();
		return true;
	}

	/** 把静态模板桥接成正式运行时窗口配置，不在这里激活任何命中窗口。 */
	FActionHitWindowRuntimeConfig ToRuntimeConfig(const FName InRuntimeWindowName) const
	{
		FActionHitWindowRuntimeConfig RuntimeConfig;
		RuntimeConfig.WindowName = InRuntimeWindowName;
		RuntimeConfig.bUseWeaponCollisionDetection = bUseWeaponCollisionDetection;
		for (const EActionHitSourceId SourceIdEnum : EnabledHitSourceIdEnums)
		{
			if (const FName SourceId = ActionHitSourceDefaults::ResolveHitSourceIdName(SourceIdEnum); SourceId != NAME_None)
			{
				RuntimeConfig.EnabledHitSourceIds.AddUnique(SourceId);
			}
		}

		for (const EActionHitSourceGroupId GroupIdEnum : EnabledHitSourceGroupIdEnums)
		{
			if (const FName GroupId = ActionHitSourceDefaults::ResolveHitSourceGroupIdName(GroupIdEnum); GroupId != NAME_None)
			{
				RuntimeConfig.EnabledHitSourceGroupIds.AddUnique(GroupId);
			}
		}

		RuntimeConfig.ResolvePolicy = ResolvePolicy;
		RuntimeConfig.RepeatResolveInterval = RepeatResolveInterval;
		RuntimeConfig.bOverrideKnockbackStrength = bOverrideKnockbackStrength;
		RuntimeConfig.OverrideKnockbackStrength = FMath::Max(OverrideKnockbackStrength, 0.f);
		return RuntimeConfig;
	}
};

/**
 * 处决专用配置。
 * 这里把执行者处决蒙太奇、目标转向等待时长和正式处决命中配置收口在同一份数据里，
 * 避免处决表现和处决伤害继续散落在不同字段层级。
 */
USTRUCT(BlueprintType)
struct FActionExecutionConfig
{
	GENERATED_BODY()

public:
	/** 执行者侧真正播放的处决蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ToolTip = "执行者侧真正播放的处决蒙太奇。留空时这份处决配置无法正式开播。"))
	TSoftObjectPtr<UAnimMontage> ExecutionMontage;

	/** 目标在正式开播前被强制转向执行者的时长。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "处决正式开播前，目标被强制转向执行者的最长时长。设为 0 表示不额外等待目标转向完成。"))
	float VictimTurnDurationSeconds = 0.f;

	/** 双边处决开播前期望保持的水平距离；只有大于当前实际距离时才会尝试补距。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "双边处决正式开播前希望保持的水平距离。只有大于当前实际距离时，运行时才会尝试补距；若不想额外挪位，可保持 0。"))
	float ExecutionStartDistance = 0.f;

	/** 处决正式命中时使用的专用命中配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution", meta = (ToolTip = "处决正式命中时使用的专用命中配置。处决不再回退到普通攻击 HitConfig。"))
	FActionExecutionHitConfig HitConfig;

public:
	/** 只读取这份静态处决模板当前引用的执行者蒙太奇。 */
	UAnimMontage* GetExecutionMontage() const
	{
		return ExecutionMontage.Get();
	}

	/** 只收集这份静态处决模板依赖的软资源路径。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!ExecutionMontage.IsNull())
		{
			OutAssetPaths.AddUnique(ExecutionMontage.ToSoftObjectPath());
		}

		HitConfig.CollectSoftObjectPaths(OutAssetPaths);
	}
};

/**
 * 武器动画静态总配置。
 * 这里是 WeaponDefinition 侧攻击、命中窗口、姿态切换、闪避、防御与切武表现的正式静态入口；
 * 对外公开的解析接口只返回“当前读取结果”，不在这里自持任何运行时状态。
 */
USTRUCT(BlueprintType)
struct FActionWeaponAnimationConfig
{
	GENERATED_BODY()

public:
	// 武器动画资源主体：
	// 这些字段共同描述一把武器在攻击、闪避、防御与特殊切武时要用到的全部动画入口。
	/** 装备该武器时需要挂接到角色身上的动画层。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ToolTip = "装备该武器时需要挂接到角色身上的动画层。它决定当前武器在主 ABP 上叠哪套表现逻辑。"))
	TSoftClassPtr<UActionHeroLinkedAnimLayer> LinkedAnimLayer;

	/** 正式攻击条目配置集合。当前运行时优先消费这组数据。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|Attack", meta = (ToolTip = "正式攻击条目配置集合。当前运行时优先消费这组数据，不再靠旧分散字段拼装攻击链。"))
	TArray<FActionAttackEntryConfig> AttackEntryConfigs;

	/** 命中窗口模板集合。用于给多个动画通知复用同一套命中源和结算策略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|HitWindow", meta = (ToolTip = "可复用的命中窗口模板集合。动画通知可按模板名直接引用，避免每个 Notify 都手填同一套命中源和结算策略。"))
	TArray<FActionHitWindowTemplateConfig> HitWindowTemplateConfigs;

	/** 姿态切换蒙太奇：Idle -> Combo。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ToolTip = "姿态切换蒙太奇：Idle -> Combo。若当前主动链允许从 Idle 直接起手，这条蒙太奇就是进入战斗姿态的正式表现入口。"))
	TSoftObjectPtr<UAnimMontage> EnterCombatModeMontage;

	/** 进入战斗姿态动画需要多高优先级的受击才能接管自己。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ClampMin = "0", ToolTip = "进入战斗姿态动画需要多高优先级的受击才能接管自己。"))
	int32 EnterCombatModeMinIncomingReactPriorityToInterrupt = 0;

	/** 姿态切换蒙太奇：Combo -> Idle。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ToolTip = "姿态切换蒙太奇：Combo -> Idle。留空时当前武器退出战斗姿态不会有独立退场演出。"))
	TSoftObjectPtr<UAnimMontage> ExitCombatModeMontage;

	/** 退出战斗姿态动画需要多高优先级的受击才能接管自己。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ClampMin = "0", ToolTip = "退出战斗姿态动画需要多高优先级的受击才能接管自己。"))
	int32 ExitCombatModeMinIncomingReactPriorityToInterrupt = 0;

	/** 原地闪避蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ToolTip = "原地闪避蒙太奇。只在角色当前没有移动输入时作为正式闪避表现入口。"))
	TSoftObjectPtr<UAnimMontage> StandingDodgeMontage;

	/** 原地闪避动画需要多高优先级的受击才能接管自己。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ClampMin = "0", ToolTip = "原地闪避动画需要多高优先级的受击才能接管自己。"))
	int32 StandingDodgeMinIncomingReactPriorityToInterrupt = 0;

	/** 移动闪避蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ToolTip = "移动闪避蒙太奇。只在角色当前存在移动输入时作为正式闪避表现入口。"))
	TSoftObjectPtr<UAnimMontage> MovingDodgeMontage;

	/** 移动闪避动画需要多高优先级的受击才能接管自己。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ClampMin = "0", ToolTip = "移动闪避动画需要多高优先级的受击才能接管自己。"))
	int32 MovingDodgeMinIncomingReactPriorityToInterrupt = 0;

	/** 防御蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ToolTip = "防御蒙太奇。它是当前武器正式防御起手的表现入口，不负责长期持盾/持续防御状态源。"))
	TSoftObjectPtr<UAnimMontage> DefenseMontage;

	/** 防御动画需要多高优先级的受击才能接管自己。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ClampMin = "0", ToolTip = "防御动画需要多高优先级的受击才能接管自己。"))
	int32 DefenseMinIncomingReactPriorityToInterrupt = 0;

	/** 普通格挡命中后的单次受击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation", meta = (ToolTip = "普通格挡命中后的单次受击蒙太奇。"))
	TSoftObjectPtr<UAnimMontage> BlockedHitMontage;

	/** 特殊切武表现段配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAnimation|WeaponSwitch", meta = (ToolTip = "特殊切武表现段配置。用于特殊切武或带额外表现的切武链。"))
	FActionWeaponSwitchClipConfig SpecialWeaponSwitchClip;

public:
	// 运行时攻击链查询：
	// 攻击组件与攻击 GA 会优先通过这组静态查询 helper 读取请求路由、分支配置和最终执行计划。
	bool HasAttackEntryConfigs() const
	{
		return AttackEntryConfigs.Num() > 0;
	}

	/** 只校验这套武器动画总配置里的攻击请求链和攻击分支链是否完整。 */
	bool ValidateRequiredAttackConfig(FString& OutFailureReason) const
	{
		const FGameplayTag RequiredRequestTags[] =
		{
			ActionGameplayTags::Attack_Request_Default,
			ActionGameplayTags::Attack_Request_Held,
			ActionGameplayTags::Attack_Request_DodgeCounter,
			ActionGameplayTags::Attack_Request_Sprint,
			ActionGameplayTags::Attack_Request_Airborne
		};

		if (HasAttackEntryConfigs())
		{
			for (const FGameplayTag& RequestTag : RequiredRequestTags)
			{
				FActionAttackEntryConfig EntryConfig;
				if (!TryGetAttackEntryConfigByRequestTag(RequestTag, EntryConfig))
				{
					OutFailureReason = FString::Printf(
						TEXT("AttackEntryConfigs 缺少请求标签配置。RequestTag=%s"),
						*RequestTag.ToString());
					return false;
				}
			}

			OutFailureReason.Reset();
			return true;
		}

		OutFailureReason = TEXT("AttackEntryConfigs 为空。当前武器定义缺少正式攻击条目配置。");
		return false;
	}

	/** 只按静态配置把攻击请求标签解析成最终攻击分支标签。 */
	FGameplayTag ResolveAttackBranchTag(const FGameplayTag& InRequestTag) const
	{
		if (HasAttackEntryConfigs())
		{
			FActionAttackEntryConfig EntryConfig;
			if (TryGetAttackEntryConfigByRequestTag(InRequestTag, EntryConfig))
			{
				return EntryConfig.BranchTag;
			}

			ensureMsgf(
				false,
				TEXT("正式攻击条目缺失，请补齐 AttackEntryConfigs。RequestTag=%s"),
				*InRequestTag.ToString());
			return ActionGameplayTags::Attack_Branch_Light;
		}

		return ActionGameplayTags::Attack_Branch_Light;
	}

	/** 只解析指定请求在静态模板里真正应落到哪个输入阶段。 */
	EActionInputEvent ResolveAttackTriggerInputEvent(const FGameplayTag& InRequestTag) const
	{
		if (HasAttackEntryConfigs())
		{
			FActionAttackEntryConfig EntryConfig;
			if (TryGetAttackEntryConfigByRequestTag(InRequestTag, EntryConfig))
			{
				return EntryConfig.TriggerInputEvent;
			}

			ensureMsgf(
				false,
				TEXT("正式攻击条目缺失，请补齐 AttackEntryConfigs。RequestTag=%s"),
				*InRequestTag.ToString());
			return EActionInputEvent::Released;
		}

		return EActionInputEvent::Released;
	}

	/** 按攻击请求标签与当前连段索引直接解析一份本次读取到的执行计划。 */
	bool TryResolveAttackExecutionConfigByRequestTag(
		const FGameplayTag& InRequestTag,
		const int32 InComboIndex,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const
	{
		OutResolvedConfig = FActionResolvedAttackExecutionConfig();

		if (HasAttackEntryConfigs())
		{
			FActionAttackEntryConfig EntryConfig;
			if (!TryGetAttackEntryConfigByRequestTag(InRequestTag, EntryConfig))
			{
				return false;
			}

			return BuildResolvedConfigFromAttackEntry(EntryConfig, InComboIndex, OutResolvedConfig);
		}

		return false;
	}

	/** 只按攻击请求标签从静态模板中读取正式命中配置，不在这里缓存长期运行态。 */
	bool TryResolveAttackHitConfigByRequestTag(
		const FGameplayTag& InRequestTag,
		FActionWeaponHitConfig& OutHitConfig) const
	{
		OutHitConfig = FActionWeaponHitConfig();

		if (HasAttackEntryConfigs())
		{
			FActionAttackEntryConfig EntryConfig;
			if (!TryGetAttackEntryConfigByRequestTag(InRequestTag, EntryConfig))
			{
				return false;
			}

			const FActionAttackClipConfig* AttackClip = EntryConfig.ResolveAttackClip(0);
			if (!AttackClip)
			{
				return false;
			}

			OutHitConfig = AttackClip->HitConfig;
			return true;
		}

		return false;
	}

	/** 按攻击分支标签与当前连段索引解析一份本次读取到的执行计划。 */
	bool TryResolveAttackExecutionConfig(
		const FGameplayTag& InBranchTag,
		const int32 InComboIndex,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const
	{
		OutResolvedConfig = FActionResolvedAttackExecutionConfig();

		if (HasAttackEntryConfigs())
		{
			FActionAttackEntryConfig EntryConfig;
			if (!TryGetAttackEntryConfigByBranchTag(InBranchTag, EntryConfig))
			{
				return false;
			}

			return BuildResolvedConfigFromAttackEntry(EntryConfig, InComboIndex, OutResolvedConfig);
		}

		return false;
	}

	/** 只读取指定攻击分支在静态模板里可用的蒙太奇数量。 */
	int32 GetAttackMontageCountForBranch(const FGameplayTag& InBranchTag) const
	{
		if (HasAttackEntryConfigs())
		{
			FActionAttackEntryConfig EntryConfig;
			return TryGetAttackEntryConfigByBranchTag(InBranchTag, EntryConfig)
				? EntryConfig.GetMontageCount()
				: 0;
		}

		return 0;
	}

	/** 按模板名把静态命中窗口模板桥接成运行时窗口配置，不在这里打开窗口。 */
	bool TryResolveHitWindowConfigByName(
		const FName InTemplateName,
		const FName InRuntimeWindowName,
		FActionHitWindowRuntimeConfig& OutRuntimeConfig) const
	{
		OutRuntimeConfig = FActionHitWindowRuntimeConfig();

		if (const FActionHitWindowTemplateConfig* TemplateConfig = FindHitWindowTemplateConfig(InTemplateName))
		{
			FString FailureReason;
			if (!TemplateConfig->IsValidTemplate(FailureReason))
			{
				ensureMsgf(
					false,
					TEXT("命中窗口模板配置非法，请检查 WeaponDefinition。TemplateName=%s Failure=%s"),
					*InTemplateName.ToString(),
					*FailureReason);
				return false;
			}

			OutRuntimeConfig = TemplateConfig->ToRuntimeConfig(InRuntimeWindowName);
			return true;
		}

		return false;
	}

	/** 只按当前战斗姿态读取静态姿态切换蒙太奇。 */
	UAnimMontage* GetCombatModeTransitionMontage(const EHeroCombatMode InCombatMode) const
	{
		// 当前约定：
		// 1. 传入 Combo 表示“准备退出战斗态”，读取 ExitCombatModeMontage；
		// 2. 其余情况表示“准备进入战斗态”，读取 EnterCombatModeMontage。
		return (InCombatMode == EHeroCombatMode::Combo)
			? ExitCombatModeMontage.Get()
			: EnterCombatModeMontage.Get();
	}

	/** 只按当前战斗姿态读取对应的受击接管阈值。 */
	int32 GetCombatModeTransitionReactGuardThreshold(const EHeroCombatMode InCombatMode) const
	{
		return InCombatMode == EHeroCombatMode::Combo
			? ExitCombatModeMinIncomingReactPriorityToInterrupt
			: EnterCombatModeMinIncomingReactPriorityToInterrupt;
	}

	/** 只按当前是否存在移动输入读取静态闪避蒙太奇。 */
	UAnimMontage* GetDodgeMontage(const bool bHasMoveInput) const
	{
		return bHasMoveInput
			? MovingDodgeMontage.Get()
			: StandingDodgeMontage.Get();
	}

	/** 只按当前是否存在移动输入读取对应的受击接管阈值。 */
	int32 GetDodgeReactGuardThreshold(const bool bHasMoveInput) const
	{
		return bHasMoveInput
			? MovingDodgeMinIncomingReactPriorityToInterrupt
			: StandingDodgeMinIncomingReactPriorityToInterrupt;
	}

	/** 只读取静态防御蒙太奇。 */
	UAnimMontage* GetDefenseMontage() const
	{
		return DefenseMontage.Get();
	}

	/** 只读取静态防御受击接管阈值。 */
	int32 GetDefenseReactGuardThreshold() const
	{
		return DefenseMinIncomingReactPriorityToInterrupt;
	}

	/** 只读取静态普通格挡受击蒙太奇。 */
	UAnimMontage* GetBlockedHitMontage() const
	{
		return BlockedHitMontage.Get();
	}

	/** 只读取静态特殊切武蒙太奇。 */
	UAnimMontage* GetSpecialWeaponSwitchMontage() const
	{
		return SpecialWeaponSwitchClip.GetMontage();
	}

	/** 只读取静态特殊切武受击接管阈值。 */
	int32 GetSpecialWeaponSwitchReactGuardThreshold() const
	{
		return SpecialWeaponSwitchClip.MinIncomingReactPriorityToInterrupt;
	}

	/** 只读取静态特殊切武命中配置。 */
	const FActionWeaponHitConfig& GetSpecialWeaponSwitchHitConfig() const
	{
		return SpecialWeaponSwitchClip.GetHitConfig();
	}

	/** 收集整套武器动画总配置依赖的软资源路径，供 AssetManager 统一预热。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		// 这里收集的是“当前武器所有可能会在运行时被即时读到的动画资源”。
		// 这样装备组件和 AssetManager 只需要调一次入口，就能把整把武器的动画依赖统一预热。
		if (!LinkedAnimLayer.IsNull())
		{
			OutAssetPaths.AddUnique(LinkedAnimLayer.ToSoftObjectPath());
		}

		if (HasAttackEntryConfigs())
		{
			for (const FActionAttackEntryConfig& AttackEntryConfig : AttackEntryConfigs)
			{
				AttackEntryConfig.CollectSoftObjectPaths(OutAssetPaths);
			}
		}

		if (!EnterCombatModeMontage.IsNull())
		{
			OutAssetPaths.AddUnique(EnterCombatModeMontage.ToSoftObjectPath());
		}

		if (!ExitCombatModeMontage.IsNull())
		{
			OutAssetPaths.AddUnique(ExitCombatModeMontage.ToSoftObjectPath());
		}

		if (!StandingDodgeMontage.IsNull())
		{
			OutAssetPaths.AddUnique(StandingDodgeMontage.ToSoftObjectPath());
		}

		if (!MovingDodgeMontage.IsNull())
		{
			OutAssetPaths.AddUnique(MovingDodgeMontage.ToSoftObjectPath());
		}

		if (!DefenseMontage.IsNull())
		{
			OutAssetPaths.AddUnique(DefenseMontage.ToSoftObjectPath());
		}

		if (!BlockedHitMontage.IsNull())
		{
			OutAssetPaths.AddUnique(BlockedHitMontage.ToSoftObjectPath());
		}

		SpecialWeaponSwitchClip.CollectSoftObjectPaths(OutAssetPaths);
	}

private:
	/** 内部静态解析 helper：从攻击条目模板组装一份已解析执行计划。 */
	bool BuildResolvedConfigFromAttackEntry(
		const FActionAttackEntryConfig& InEntryConfig,
		const int32 InComboIndex,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const
	{
		OutResolvedConfig = FActionResolvedAttackExecutionConfig();
		OutResolvedConfig.BranchTag = InEntryConfig.BranchTag;
		const FActionAttackClipConfig* AttackClip = InEntryConfig.ResolveAttackClip(InComboIndex);
		if (!AttackClip)
		{
			return false;
		}

		OutResolvedConfig.ResolvedMontage = AttackClip->GetMontage();
		OutResolvedConfig.bUseComboIndex = InEntryConfig.bUseComboIndex;
		OutResolvedConfig.bAdvanceComboIndexOnPlay = InEntryConfig.bAdvanceComboIndexOnPlay;
		OutResolvedConfig.bResetComboIndexOnPlay = InEntryConfig.bResetComboIndexOnPlay;
		OutResolvedConfig.MontageCount = InEntryConfig.GetMontageCount();
		OutResolvedConfig.ResolvedHitConfig = AttackClip->HitConfig;
		OutResolvedConfig.MinIncomingReactPriorityToInterrupt = AttackClip->MinIncomingReactPriorityToInterrupt;
		OutResolvedConfig.bShouldSpawnProjectile = AttackClip->bShouldSpawnProjectile;
		OutResolvedConfig.ResolvedProjectileSpawnConfig = AttackClip->ProjectileSpawnConfig;
		return OutResolvedConfig.IsValid();
	}

	// 内部数组查询助手：
	// 外层统一走公开解析接口，这里只负责在配置数组里查找对应请求或分支条目。
	/** 读取指定攻击请求标签的正式攻击条目。 */
	bool TryGetAttackEntryConfigByRequestTag(
		const FGameplayTag& InRequestTag,
		FActionAttackEntryConfig& OutEntryConfig) const
	{
		for (const FActionAttackEntryConfig& EntryConfig : AttackEntryConfigs)
		{
			if (EntryConfig.RequestTag == InRequestTag)
			{
				OutEntryConfig = EntryConfig;
				return true;
			}
		}

		return false;
	}

	/** 读取指定攻击分支标签的正式攻击条目。 */
	bool TryGetAttackEntryConfigByBranchTag(
		const FGameplayTag& InBranchTag,
		FActionAttackEntryConfig& OutEntryConfig) const
	{
		for (const FActionAttackEntryConfig& EntryConfig : AttackEntryConfigs)
		{
			if (EntryConfig.BranchTag == InBranchTag && EntryConfig.IsValidConfig())
			{
				OutEntryConfig = EntryConfig;
				return true;
			}
		}

		return false;
	}

	/** 按模板名查找命中窗口模板。 */
	const FActionHitWindowTemplateConfig* FindHitWindowTemplateConfig(const FName InTemplateName) const
	{
		if (InTemplateName == NAME_None)
		{
			return nullptr;
		}

		for (const FActionHitWindowTemplateConfig& TemplateConfig : HitWindowTemplateConfigs)
		{
			if (TemplateConfig.TemplateName == InTemplateName)
			{
				return &TemplateConfig;
			}
		}

		return nullptr;
	}
};

/** 基础角色动画实例代理，只承载给动画线程消费的只读镜像。 */
USTRUCT()
struct FCharacterAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FCharacterAnimInstanceProxy()
		: FAnimInstanceProxy()
	{
	}

	explicit FCharacterAnimInstanceProxy(UAnimInstance* InInstance)
		: FAnimInstanceProxy(InInstance)
	{
	}

	/** 游戏线程写入的速度镜像，不是正式移动状态源。 */
	FVector Velocity = FVector::ZeroVector;

	/** 当前加速度大小镜像，不替代 CharacterMovement 的正式底层移动状态。 */
	float CurrentAcceleration = 0.f;

	/** 当前战斗模式镜像，供 ABP 消费，不反向驱动业务状态。 */
	EHeroCombatMode CombatMode = EHeroCombatMode::Idle;

	/** 当前移动状态镜像，供 ABP 消费，不替代控制器侧正式移动输入镜像。 */
	EMoveState MoveState = EMoveState::Walk;

	/** 游戏线程向动画线程同步只读镜像的入口钩子。 */
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	/** 动画线程消费前更新当前帧镜像。 */
	virtual void Update(float DeltaSeconds) override;
	/** 当前帧镜像同步完成后的收尾钩子。 */
	virtual void PostUpdate(UAnimInstance* InAnimInstance) const override;
};

/** 英雄角色动画实例代理，在基础代理镜像上补充英雄专用只读镜像。 */
USTRUCT()
struct FHeroAnimInstanceProxy : public FCharacterAnimInstanceProxy
{
	GENERATED_BODY()

	FHeroAnimInstanceProxy()
		: FCharacterAnimInstanceProxy()
	{
	}

	explicit FHeroAnimInstanceProxy(UAnimInstance* InInstance)
		: FCharacterAnimInstanceProxy(InInstance)
	{
	}

	/** 当前是否处于下落状态的镜像，不替代 CharacterMovement 的正式 Falling 状态。 */
	bool bIsFalling = false;

	/** 在基础代理镜像上补充 Hero 专用只读镜像的同步入口。 */
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	/** 更新 Hero 动画线程本帧要消费的附加镜像。 */
	virtual void Update(float DeltaSeconds) override;
	/** Hero 专用镜像同步完成后的收尾钩子。 */
	virtual void PostUpdate(UAnimInstance* InAnimInstance) const override;
};
