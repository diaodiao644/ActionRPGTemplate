#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionInputTypes.generated.h"

class UInputAction;

/**
 * 角色移动状态。
 * 说明：
 * 1. 这里描述的是角色当前移动语义，而不是 CharacterMovement 的底层速度模式；
 * 2. 战斗链主要用它区分普通跑动、冲刺攻击、闪避表现等分支；
 * 3. 后续若继续扩展滑步、锁定绕圈等移动语义，也应优先接在这里统一管理。
 */
UENUM()
enum class EMoveState : uint8
{
	/** 常规移动。 */
	Walk,

	/** 普通奔跑。 */
	Run,

	/** 快速冲刺，通常用于触发冲刺攻击等更激进的战斗分支。 */
	FastRun
};

/**
 * 输入动作与 GameplayTag 的静态映射关系。
 * 作用：
 * 1. 蓝图只需要配置 InputAction 与 InputTag 的对应关系；
 * 2. 运行时由输入组件统一把原始输入事件翻译成 Tag 驱动的战斗输入；
 * 3. 这样攻击、闪避、防御、切武等逻辑都只依赖统一的输入标签，不直接依赖具体按键资源。
 */
USTRUCT(BlueprintType)
struct FActionInputBinding
{
	GENERATED_BODY()

public:
	/** 输入动作资源。 */
	UPROPERTY(EditDefaultsOnly)
	const UInputAction* InputAction = nullptr;

	/**
	 * 该输入动作在战斗框架中的统一语义标签。
	 * 例如可以映射到 Attack、Dodge、Defense、WeaponSwitch 等输入标签。
	 */
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag;
};
