#include "Game/GameGameState.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Paint/PaintSubsystem.h"

void AGameGameState::BeginPlay()
{
	Super::BeginPlay();

	UPaintSubsystem* const Paint = GetWorld()->GetSubsystem<UPaintSubsystem>();
	if (!Paint)
	{
		return;
	}

	if (!HasAuthority())
	{
		// 늦게 들어온 클라이언트는 로그가 월드의 BeginPlay보다 먼저 도착할 수 있다.
		// 그때는 표면이 아직 없으므로 OnRep이 미뤄 두었고, 여기서 처음으로 그린다.
		ApplyNewSplats();
		return;
	}

	// 서브시스템은 게임 모드를 모른다. 서버에서는 제출된 스플랫이 여기로 흘러 들어와 로그가 된다.
	Paint->OnSplatSubmitted.BindUObject(this, &AGameGameState::AddSplat);
	GetWorld()->GetTimerManager().SetTimer(
		CoverageTimer, this, &AGameGameState::RefreshCoverage, CoverageRefreshInterval, /*bLoop=*/true);
}

void AGameGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* const World = GetWorld())
	{
		if (UPaintSubsystem* const Paint = World->GetSubsystem<UPaintSubsystem>())
		{
			Paint->OnSplatSubmitted.Unbind();
		}
		World->GetTimerManager().ClearTimer(CoverageTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AGameGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGameGameState, SplatLog);
	DOREPLIFETIME(AGameGameState, WorldCoverage);
	DOREPLIFETIME(AGameGameState, MatchEndServerTime);
	DOREPLIFETIME(AGameGameState, WinningTeam);
	DOREPLIFETIME(AGameGameState, bMatchEnded);
}

void AGameGameState::SetMatchEndTime(double InServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	MatchEndServerTime = InServerTime;
}

void AGameGameState::SetMatchResult(int32 InWinningTeam)
{
	if (!HasAuthority() || bMatchEnded)
	{
		return;
	}

	WinningTeam = InWinningTeam;
	bMatchEnded = true;

	// RepNotify는 값을 쓴 권한자에게는 오지 않는다. 리슨 호스트의 화면도 갱신되도록 직접 부른다.
	HandleMatchEnded();
}

float AGameGameState::GetRemainingTime() const
{
	if (MatchEndServerTime <= 0.0)
	{
		return 0.0f;
	}

	return static_cast<float>(FMath::Max(0.0, MatchEndServerTime - GetServerWorldTimeSeconds()));
}

void AGameGameState::OnRep_MatchEnded()
{
	HandleMatchEnded();
}

void AGameGameState::HandleMatchEnded()
{
	BP_OnMatchEnded(WinningTeam);
}

void AGameGameState::AddSplat(const FPaintSplat& Splat)
{
	if (!HasAuthority())
	{
		return;
	}

	// 일시 스플랫은 기록이 아니라 연출이다. 멀티캐스트는 서버 자신에서도 실행된다.
	if (Splat.bTransient)
	{
		MulticastTransientSplat(Splat);
		return;
	}

	FPaintSplatLogItem& Item = SplatLog.Items.AddDefaulted_GetRef();
	Item.Splat = Splat;
	SplatLog.MarkItemDirty(Item);

	// RepNotify는 서버에서 오지 않으므로 서버(리슨 호스트)의 표면은 여기서 직접 그린다.
	++AppliedSplatCount;
	if (UPaintSubsystem* const Paint = GetWorld()->GetSubsystem<UPaintSubsystem>())
	{
		Paint->ApplySplat(Splat);
	}
}

void AGameGameState::ClearPaint()
{
	if (!HasAuthority())
	{
		return;
	}

	SplatLog.Items.Empty();
	SplatLog.MarkArrayDirty();
	AppliedSplatCount = 0;
	if (UPaintSubsystem* const Paint = GetWorld()->GetSubsystem<UPaintSubsystem>())
	{
		Paint->ClearPaint();
	}
}

void AGameGameState::OnRep_SplatLog()
{
	if (HasActorBegunPlay())
	{
		ApplyNewSplats();
	}
}

void AGameGameState::MulticastTransientSplat_Implementation(const FPaintSplat& Splat)
{
	if (const auto Paint = GetWorld()->GetSubsystem<UPaintSubsystem>())
	{
		Paint->ApplySplat(Splat);
	}
}

void AGameGameState::ApplyNewSplats()
{
	UPaintSubsystem* const Paint = GetWorld()->GetSubsystem<UPaintSubsystem>();
	if (!Paint)
	{
		return;
	}

	// 로그는 뒤에만 자라고, 줄어드는 유일한 경우는 서버의 ClearPaint다.
	if (SplatLog.Items.Num() < AppliedSplatCount)
	{
		Paint->ClearPaint();
		AppliedSplatCount = 0;
	}

	for (; AppliedSplatCount < SplatLog.Items.Num(); ++AppliedSplatCount)
	{
		Paint->ApplySplat(SplatLog.Items[AppliedSplatCount].Splat);
	}
}

void AGameGameState::RefreshCoverage()
{
	if (const UPaintSubsystem* const Paint = GetWorld()->GetSubsystem<UPaintSubsystem>())
	{
		WorldCoverage = Paint->GetWorldCoverage();
	}
}
