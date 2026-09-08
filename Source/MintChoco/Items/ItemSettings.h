#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "ItemSettings.generated.h"

class AItemPickup;
class UItemProfile;

/**
 * 프로젝트 전역 아이템 설정. 값은 Config/DefaultGame.ini에 남는다.
 *
 * 아이템 목록이 여기 있는 이유는 두 곳에서 같은 목록이 필요하기 때문이다. 서버의
 * 게임모드는 여기서 무엇을 스폰할지 고르고, 모든 머신의 슬롯 컴포넌트는 복제된 상태
 * 태그를 다시 프로필로 되돌려(연출을 찾으려고) 같은 목록을 뒤진다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Items"))
class MINTCHOCO_API UItemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UItemSettings& Get() { return *GetDefault<UItemSettings>(); }

	/** 스폰 후보. 매 주기 이 중 하나가 무작위로 나온다. */
	UPROPERTY(Config, EditAnywhere, Category = "Items")
	TArray<TSoftObjectPtr<UItemProfile>> Items;

	/** 맵에 놓이는 픽업 액터. 보통 BP_ItemPickup. */
	UPROPERTY(Config, EditAnywhere, Category = "Items")
	TSoftClassPtr<AItemPickup> PickupClass;

	/** 로드된 아이템 목록. 소프트 참조라 첫 호출에 동기 로드하고, 이후는 캐시에서 돌아온다. */
	void LoadItems(TArray<UItemProfile*>& OutItems) const;

	/** 상태 태그로 아이템을 찾는다. 없으면 nullptr. */
	UItemProfile* FindItemByStateTag(const FGameplayTag& StateTag) const;

	/** 픽업 클래스를 로드한다. 미설정이면 nullptr. */
	UClass* LoadPickupClass() const;
};
