#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "ItemProfile.generated.h"

class UItemAbility;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;
class UTexture2D;

/**
 * 아이템 한 종의 정의. 아이템 하나 = 에셋 하나이며 로직은 없다.
 *
 * 무엇을 보여주고(아이콘, 픽업 메시, 연출) 얼마나 오래 가는지, 그리고 발동하면 어떤
 * 어빌리티가 도는지만 들어 있다. 효과 자체는 AbilityClass가 가리키는 UItemAbility의
 * 서브클래스가 구현하고, 그 어빌리티가 읽을 튜닝값은 이 클래스의 서브클래스에 둔다.
 * 아이템을 추가하는 일은 프로필 서브클래스 + 어빌리티 서브클래스 + 에셋 하나다.
 */
UCLASS(Abstract, BlueprintType)
class MINTCHOCO_API UItemProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	/** HUD 슬롯에 보이는 아이콘. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	TObjectPtr<UTexture2D> Icon;

	/** 맵에 놓였을 때의 외형. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMesh> PickupMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<USoundBase> PickupSound;

	/** 효과 지속시간(초). 같은 아이템을 효과 중에 다시 쓰면 이 값으로 다시 시작한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float Duration = 2.0f;

	/** 발동하면 도는 어빌리티. 비어 있으면 주울 수는 있어도 아무 일도 하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UItemAbility> AbilityClass;

	/** 효과 중 캐릭터에 붙는 이펙트. 두 캐릭터가 같은 연출을 쓰므로 UnitData가 아니라 여기다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UNiagaraSystem> ActivateFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<USoundBase> ActivateSound;

	/** 효과 중 ASC에 붙는 상태 태그. 어빌리티 클래스가 정하며, 없으면 빈 태그. */
	UFUNCTION(BlueprintPure, Category = "Item")
	FGameplayTag GetStateTag() const;

	/**
	 * 스피드 스타처럼 무브먼트의 속도 부스트 플래그를 쓰는 아이템인지. 서버가 클라이언트의
	 * 플래그를 인정할지 판단할 때 든 아이템을 이걸로 묻는다.
	 */
	virtual bool GrantsSpeedBoost() const { return false; }

	/** 장착·습득 시점에 비어 있으면 "아무 일도 안 일어나는" 참조를 경고로 남긴다. */
	virtual void LogUnsetReferences(const UObject* Owner) const;
};
