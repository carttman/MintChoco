// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Unit.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"

AUnit::AUnit()
{
	// 유닛 자체가 매 프레임 할 일은 없다. 이동은 CharacterMovement가 돌리고,
	// 발사와 스킬은 각자의 컴포넌트가 필요한 동안만 틱한다.
	PrimaryActorTick.bCanEverTick = false;
}

void AUnit::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// BeginPlay보다 이른 시점이라 첫 프레임이 그려지기 전에 메시가 확정된다.
	// 여기서 보이는 값은 클라이언트에서도 블루프린트 기본값이므로, 런타임에
	// 교체된 경우는 OnRep_UnitData가 뒤이어 처리한다.
	ApplyUnitData();
}

void AUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUnit, UnitData);
}

void AUnit::SetUnitData(UUnitDataAsset* NewUnitData)
{
	if (!HasAuthority() || UnitData == NewUnitData)
	{
		return;
	}

	UnitData = NewUnitData;

	// OnRep_UnitData는 변경을 수행한 권한 주체에서는 호출되지 않으므로,
	// 서버와 리슨 호스트의 메시는 여기서 직접 맞춰야 한다.
	ApplyUnitData();
}

void AUnit::OnRep_UnitData()
{
	ApplyUnitData();
}

void AUnit::ApplyUnitData()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!UnitData || !MeshComponent)
	{
		return;
	}

	// 메시 교체가 애님 인스턴스를 다시 만들기 때문에 순서를 바꾸면 애님 클래스가 날아간다.
	if (UnitData->Mesh)
	{
		MeshComponent->SetSkeletalMesh(UnitData->Mesh);
	}

	if (UnitData->AnimClass)
	{
		MeshComponent->SetAnimInstanceClass(UnitData->AnimClass);
	}
}

float AUnit::PlayActionFeedback(EUnitAction Action)
{
	return PlayActionFeedbackAtLocation(Action, GetActorLocation());
}

float AUnit::PlayActionFeedbackAtLocation(EUnitAction Action, const FVector& WorldLocation)
{
	const FUnitActionFeedback* Feedback = UnitData ? UnitData->FindFeedback(Action) : nullptr;
	if (!Feedback)
	{
		return 0.0f;
	}

	// 몽타주는 루트 모션과 AnimNotify를 통해 게임플레이에 영향을 주므로 서버에서도 돈다.
	float Duration = 0.0f;
	if (Feedback->Montage)
	{
		Duration = PlayAnimMontage(Feedback->Montage);
	}

	// 이펙트와 소리는 순수 연출이라 데디케이티드 서버에서는 낭비다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return Duration;
	}

	if (Feedback->FX)
	{
		if (Feedback->FXSocket.IsNone())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, Feedback->FX, WorldLocation, GetActorRotation());
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				Feedback->FX,
				GetMesh(),
				Feedback->FXSocket,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true);
		}
	}

	if (Feedback->Sound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, Feedback->Sound, WorldLocation);
	}

	return Duration;
}
