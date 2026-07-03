// 文件说明：声明 ActionEnemyCharacter 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Characters/ActionCharacterBase.h"
#include "ActionEnemyCharacter.generated.h"

class UEnemyAttributeComponent;

UCLASS()
class ACTIONRPG_API AActionEnemyCharacter : public AActionCharacterBase
{
	GENERATED_BODY()

public:
	AActionEnemyCharacter();

protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UEnemyAttributeComponent* EnemyAttributeComponent = nullptr;
};
