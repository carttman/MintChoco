#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Game/MatchResult.h"

#include "MatchResultStage.generated.h"

class UAnimSequence;
class UCameraComponent;
class UUnitDataAsset;
class USkeletalMeshComponent;

/** 무대에 세울 한 자리의 내용. 팀과 그 팀의 캐릭터 정의. */
struct MINTCHOCO_API FMatchResultSlotCast
{
	int32 Team = Teams::None;
	TObjectPtr<UUnitDataAsset> UnitData;
};

/**
 * 결과 화면의 무대. 맵에 하나 놓고 뷰포트에서 구도를 잡는다.
 *
 * 카메라는 맵 전체가 들어오는 부감이고, 두 캐릭터 자리는 그 카메라의 자식이라 화면 좌우에
 * 고정된다. 자리를 바닥이 아니라 카메라 앞 허공에 두는 것이 핵심이다: 맵은 멀리, 캐릭터는
 * 가까이 있어 맵 → 캐릭터 → UMG 게이지 순서의 레이어가 합성 없이 그대로 나온다.
 *
 * 구도의 진실은 컴포넌트 트랜스폼이다. 카메라와 두 자리를 뷰포트에서 끌어다 맞추면 되고,
 * 여기 C++ 기본값은 맵에 아무것도 놓지 않았을 때의 시작점일 뿐이다.
 *
 * 액터는 복제하지 않는다. 경기 결과는 이미 AGameGameState가 복제하므로 각 머신이 같은 값으로
 * 같은 연출을 로컬에서 돌린다. 캐릭터도 표시 전용 컴포넌트라 게임플레이 상태가 없다.
 */
UCLASS(Blueprintable)
class MINTCHOCO_API AMatchResultStage : public AActor
{
	GENERATED_BODY()

public:
	AMatchResultStage();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 두 자리에 캐릭터를 세우고 서 있는 애니메이션을 돌린다. 연출이 시작될 때 한 번. */
	void Prepare(const FMatchResultSlotCast& Left, const FMatchResultSlotCast& Right);

	/** 이긴 쪽은 다가와 춤추고, 진 쪽은 물러나 멈추고 회색이 된다. 무승부면 둘 다 그대로 선다. */
	void PlayFinish(int32 WinningTeam);

	/** 캐릭터를 치우고 무대를 처음 상태로 되돌린다. */
	void Dismiss();

	UCameraComponent* GetCamera() const { return Camera; }

	/** 맵에 놓이지 않았을 때, 칠할 수 있는 영역 전체가 화면에 들어오도록 카메라를 잡는다. */
	void FrameBounds(const FBox& Bounds);

protected:
	/**
	 * 캐릭터가 카메라를 정확히 보게 맞추는 요 보정. 스켈레탈 메시가 자기 공간에서 어느 쪽을 보고
	 * 서 있느냐에 달린 값이라 메시를 바꾸면 여기도 바뀐다. 이 프로젝트의 캐릭터는 +Y를 보므로
	 * ACharacter 가 캡슐 아래 메시에 주는 것과 같은 -90 이다. 옆이나 뒤를 보고 서 있으면 이 값을 돌린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Match Result", meta = (ClampMin = "-180", ClampMax = "180"))
	float MeshYawOffset = -90.0f;

	/** 자동으로 구도를 잡을 때 내려다보는 각도. 맵에 놓은 무대는 이 값을 쓰지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Match Result|Fallback", meta = (ClampMin = "-89", ClampMax = "-5"))
	float FallbackPitchDegrees = -50.0f;

	/** 자동으로 구도를 잡을 때 영역이 화면에 들어오고도 남게 두는 여유 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Match Result|Fallback", meta = (ClampMin = "1"))
	float FallbackMargin = 1.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match Result")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match Result")
	TObjectPtr<UCameraComponent> Camera;

	/** 화면 왼쪽 자리. 어느 팀이 서는지는 결과 바의 LeftPaintId가 정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match Result")
	TObjectPtr<USceneComponent> LeftSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match Result")
	TObjectPtr<USceneComponent> RightSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match Result")
	TObjectPtr<USkeletalMeshComponent> LeftCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match Result")
	TObjectPtr<USkeletalMeshComponent> RightCharacter;

private:
	/** 한 자리의 진행 상태. 목표까지 남은 이동과 도착했을 때 할 일을 들고 있다. */
	struct FSlotState
	{
		int32 Team = Teams::None;
		EMatchResultStep Step = EMatchResultStep::Stay;

		/** 카메라 정면 축 위의 거리. 양수가 카메라 쪽이다. */
		float Offset = 0.0f;
		float TargetOffset = 0.0f;

		/** 목표에 닿았을 때의 마무리(춤 또는 정지)를 아직 하지 않았는지. */
		bool bFinishPending = false;
	};

	/** 생성자에서 한 자리를 카메라에 붙이고 기본 구도를 잡는다. Side 는 -1(왼쪽) 또는 1(오른쪽). */
	void LayOutSlot(USceneComponent* Slot, USkeletalMeshComponent* Character, float Side);

	/** 자리에 캐릭터를 올린다. 정의가 없으면 그 자리를 비워 둔다. */
	void Dress(USkeletalMeshComponent* Character, const USceneComponent* Slot, FSlotState& State,
		const FMatchResultSlotCast& Cast) const;

	/** 한 자리를 목표 쪽으로 옮기고, 도착했으면 마무리를 건다. */
	void AdvanceSlot(FSlotState& State, USkeletalMeshComponent* Character, const USceneComponent* Slot,
		float DeltaSeconds) const;

	/**
	 * 자리에서 카메라 쪽으로 Offset 만큼 물린 곳에 캐릭터를 세우고, 카메라 좌표계 기준으로 세워 돌린다.
	 *
	 * 월드가 아니라 카메라의 축을 쓴다: 머리는 카메라의 위쪽을, 얼굴은 카메라를 향한다. 그래서 카메라가
	 * 정수직 탑다운이든 비스듬하든 화면에서는 늘 똑바로 서서 이쪽을 보는 그림이 된다 - 월드 기준으로 보면
	 * 그만큼 누워 있다. 결과 화면의 캐릭터는 무대 위 배우가 아니라 화면에 붙은 전경 요소이기 때문이다.
	 *
	 * 자리의 회전은 쓰지 않는다. 자리는 위치만 뜻하고, 어디에 놓든 캐릭터는 카메라를 본다.
	 */
	void PoseAtOffset(const FSlotState& State, USkeletalMeshComponent* Character, const USceneComponent* Slot) const;

	/** 도착한 캐릭터의 마무리. 승자는 춤, 패자는 정지 + 회색. */
	void ApplyFinish(const FSlotState& State, USkeletalMeshComponent* Character) const;

	static void PlayLooping(USkeletalMeshComponent* Character, UAnimSequence* Animation);

	FSlotState LeftState;
	FSlotState RightState;

	/** 목표까지 남은 시간(초). 속도가 아니라 시간으로 잡아야 거리와 무관하게 같은 박자로 움직인다. */
	float MoveRemaining = 0.0f;
};
