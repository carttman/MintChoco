// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnitInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * 플레이어 조작에 쓰이는 입력 에셋 묶음.
 *
 * 캐릭터가 아니라 플레이어의 속성이므로 UUnitDataAsset과는 분리해 둔다. 민트와
 * 초코의 조작은 같아야 하고, 조작만 다른 프리셋(게임패드, 좌손잡이)이 필요해지면
 * 캐릭터를 건드리지 않고 이 에셋만 갈아끼우면 된다.
 *
 * 액션이 늘 때 AUnit에 UPROPERTY를 하나씩 추가하는 대신 여기에 필드를 넣는다.
 * 그래야 액션 추가가 C++ 컴파일을 요구하지 않는다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UUnitInputConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> GameplayContext;

	/** 컨텍스트 우선순위. UI가 위에 얹힐 자리를 남겨두려고 게임플레이는 0을 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 GameplayContextPriority = 0;

	/** Axis2D. X는 좌우, Y는 앞뒤. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/**
	 * Axis2D. X는 요, Y는 피치.
	 *
	 * 감도와 Y축 반전은 이 액션의 Modifier(Scalar, Negate)에서 처리한다. C++에
	 * 상수로 박으면 나중에 옵션 메뉴를 만들 때 다시 뜯어야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> DashAction;
};
