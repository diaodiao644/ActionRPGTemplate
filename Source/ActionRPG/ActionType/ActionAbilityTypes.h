#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ActionAbilityTypes.generated.h"

/**
 * Ability 激活策略。
 * 说明：
 * 1. 这里描述的是“这类 Ability 通常通过什么入口被激活”；
 * 2. 主要用于统一约束输入触发型、事件触发型和授予即激活型 Ability；
 * 3. 后续如果继续补 Ability 优先级与调度策略，也应围绕这一层语义继续扩展。
 */
UENUM()
enum class EActionAbilityActivationPolicy : uint8
{
	OnInput,               /* 通过输入触发。 */
	OnTriggered,           /* 通过事件触发。 */
	BothInputAndTriggered, /* 同时支持输入和事件触发。 */
	OnGiven                /* 授予后立即尝试激活。 */
};

/**
 * 可被授予的 Ability 配置。
 * 作用：
 * 1. 作为数据资产侧对 ASC 授予能力的最小配置单元；
 * 2. 统一描述“输入标签 + Ability 类”这一对运行时绑定关系；
 * 3. 装备槽、角色默认能力和其他可扩展能力集都可以复用这份结构。
 */
USTRUCT(BlueprintType)
struct FActionAbilitySet
{
	GENERATED_BODY()

public:
	/** 该 Ability 在输入层对应的统一 GameplayTag。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag InputTag;

	/** 运行时需要授予到 ASC 的 Ability 类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityToGrant;

	/** 判断这条授予配置是否完整，避免把空 Ability 或空输入标签写入授予链。 */
	bool IsValid() const
	{
		return InputTag.IsValid() && AbilityToGrant;
	}
};
