#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "NetAssetRetainerSubsystem.generated.h"

/**
 * 네트워크로 참조되는 자산과 클래스를 게임 인스턴스 수명 동안 붙들어 둔다.
 *
 * 서버는 자산(붓 재질, 아이템 프로필, 유닛 데이터)과 클래스(픽업 원형, 유닛)를 NetGUID로 보내고,
 * 클라이언트가 한 번 확인(ack)한 GUID의 경로는 다시 보내지 않는다. 그런데 클라이언트가 로비로
 * 돌아가 게임 맵 자산을 GC하면 그 GUID의 캐시 항목이 90초 뒤 지워진다. 다음 경기에서 서버는
 * 여전히 경로 없이 GUID만 보내므로 그 클라이언트에서만 NOT_IN_CACHE가 되어 페인트가 안 칠해지고
 * 아이템이 안 보인다. 호스트는 서버 객체를 그대로 쓰므로 겪지 않는다.
 *
 * DefaultEngine.ini의 net.ResetAckStatePostSeamlessTravel / net.AllowClientRemapCacheObject가
 * 프로토콜 쪽에서 같은 문제를 막고, 이 서브시스템은 애초에 GC가 일어나지 않게 해서 그 둘이
 * 놓치는 경우(경기 도중 GC, 90초 만료)까지 덮는다. 붙드는 목록은 GetRetainedAssetClasses와
 * GetRetainedActorClasses에 있다. 새 데이터 에셋 클래스가 복제 참조에 실리면 거기에 더한다.
 */
UCLASS()
class MINTCHOCO_API UNetAssetRetainerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 에셋 레지스트리에서 대상 자산과 블루프린트 클래스를 찾아 로드하고 붙든다. 다시 불러도 된다:
	 * 이미 붙든 것은 건너뛰고 새로 찾은 것만 더한다. 붙든 객체 수(누적)를 돌려준다.
	 */
	int32 Retain();

	int32 NumRetained() const { return Retained.Num(); }

	bool IsRetained(const UObject* Object) const;

	/** 복제 참조에 실리는 데이터 에셋 클래스. /Game 아래의 모든 인스턴스(하위 클래스 포함)를 붙든다. */
	static TArray<const UClass*> GetRetainedAssetClasses();

	/** 서버가 스폰하는 액터의 네이티브 부모. 이를 상속한 블루프린트 클래스를 붙든다. */
	static TArray<const UClass*> GetRetainedActorClasses();

private:
	void RetainObject(UObject* Object);

	/** 붙들린 자산과 클래스. UPROPERTY라 GC가 가져가지 못한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Retained;
};
