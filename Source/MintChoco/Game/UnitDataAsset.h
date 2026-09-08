// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnitDataAsset.generated.h"

class UAnimInstance;
class UAnimMontage;
class UNiagaraSystem;
class USkeletalMesh;
class USoundBase;

/**
 * 유닛이 수행하는 동작.
 *
 * 캐릭터가 몇 종이든 동작의 목록과 시스템 동작은 같고, 각 동작에 붙는 애니메이션과
 * 이펙트만 다르다. 새 동작이 생기면 여기에 항목을 하나 추가하고 데이터 에셋에서
 * 채우면 되며, 재생하는 코드는 건드릴 필요가 없다.
 *
 * 이동은 여기에 없다. 이동은 몽타주가 아니라 애님 그래프의 상태 기계가 다루기
 * 때문에, 캐릭터별 차이는 AnimClass 교체만으로 끝난다.
 */
UENUM(BlueprintType)
enum class EUnitAction : uint8
{
	Dash	UMETA(DisplayName = "이동 가속"),
	Fire	UMETA(DisplayName = "페인트 발사"),
	Hit		UMETA(DisplayName = "피격"),
	Death	UMETA(DisplayName = "사망"),
};

/**
 * 동작 하나에 붙는 연출 묶음. 셋 다 선택이며, 비어 있는 항목은 조용히 넘어간다.
 * 캐릭터 사이의 차이는 전부 이 구조체의 값 차이로 표현된다.
 */
USTRUCT(BlueprintType)
struct FUnitActionFeedback
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UNiagaraSystem> FX;

	/**
	 * FX를 붙일 메시 소켓. None이면 붙이지 않고, 재생을 요청한 쪽이 넘긴 월드 위치에
	 * 스폰한다. 총구 화염은 소켓이 필요하고 피격 이펙트는 맞은 지점이 필요하다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FName FXSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<USoundBase> Sound;
};

/**
 * 캐릭터 한 종의 정의 전부. 로직은 없고 무엇을 보여줄지만 들어 있다.
 *
 * 민트와 초코는 이동, 이동 가속, 페인트 총이 모두 같은 코드로 돌고 애니메이션과
 * 이펙트만 다르다. 그래서 캐릭터는 C++ 클래스가 아니라 이 에셋으로만 갈린다.
 * 캐릭터를 추가하는 일은 에셋 하나를 만드는 일이지 컴파일이 필요한 일이 아니다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UUnitDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> Mesh;

	/**
	 * 두 캐릭터의 상태 기계가 같다면 공용 ABP 하나에 Linked Anim Layer를 두고
	 * 여기에는 그대로 공용 ABP를 넣는 편이 낫다. 스켈레톤이 갈릴 때만 ABP를 나눈다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSubclassOf<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TMap<EUnitAction, FUnitActionFeedback> ActionFeedback;

	/** 등록되지 않은 동작이면 nullptr. 호출부는 이 함수만 쓰고 맵을 직접 뒤지지 않는다. */
	const FUnitActionFeedback* FindFeedback(EUnitAction Action) const;
};
