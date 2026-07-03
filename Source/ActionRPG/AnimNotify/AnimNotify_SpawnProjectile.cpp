// 文件说明：实现发射物生成动画通知。

#include "AnimNotify/AnimNotify_SpawnProjectile.h"

#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/ActionDebugHelper.h"

UAnimNotify_SpawnProjectile::UAnimNotify_SpawnProjectile()
	: Super()
{
}

FString UAnimNotify_SpawnProjectile::GetNotifyName_Implementation() const
{
	return TEXT("SpawnProjectile");
}

void UAnimNotify_SpawnProjectile::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(MeshComp->GetOwner());
	if (!OwnerHeroCharacter)
	{
		return;
	}

	UHeroAttackComponent* HeroAttackComponent = OwnerHeroCharacter->GetHeroAttackComponent();
	if (!HeroAttackComponent)
	{
		Debug::Print(TEXT("[发射物] 角色缺少 HeroAttackComponent，无法响应生成通知"), FColor::Red, 2.0f);
		return;
	}

	HeroAttackComponent->TrySpawnCurrentAttackProjectile();
}
