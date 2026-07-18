#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatWeaponPresentationSwitch.generated.h"

/**
 * Combat 过渡武器挂点切换通知。
 * 它只负责在精确动画帧通知当前英雄：当前已装备武器应切到手持 WeaponSocket，
 * 还是切回 HolsterSocket。
 * 它服务 Combat 表现桥，不替代切武事务或正式已装备状态源。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_CombatWeaponPresentationSwitch : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_CombatWeaponPresentationSwitch();

public:
	/** 返回编辑器内显示名，方便资产作者识别这是一枚武器挂点表现桥接通知。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在精确帧通知英雄把当前已装备武器切到手持或待命挂点。它只推进这一帧的表现桥，不替代切武事务或正式装备状态。 */
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/** true 表示切到手持 WeaponSocket，false 表示切回 HolsterSocket。它只是通知本地表现配置，不是正式装备状态。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ToolTip = "true 表示把当前已装备武器切到手持 WeaponSocket，false 表示切回 HolsterSocket。它只影响这一帧的武器表现挂点，不会重新决定谁是当前正式装备武器。"))
	bool bAttachToWeaponSocket = true;
};
