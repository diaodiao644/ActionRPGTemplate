#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatWeaponPresentationSwitch.generated.h"

/**
 * Combat 过渡武器挂点切换通知。
 * 它只负责在精确动画帧通知当前英雄：当前已装备武器应切到手持 WeaponSocket，
 * 还是切回 HolsterSocket。
 */
UCLASS()
class ACTIONRPG_API UAnimNotify_CombatWeaponPresentationSwitch : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_CombatWeaponPresentationSwitch();

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/** true 表示切到手持 WeaponSocket，false 表示切回 HolsterSocket。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bAttachToWeaponSocket = true;
};
