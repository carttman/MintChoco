#include "Game/GameGameState.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GameGameMode.h"
#include "Game/TeamTypes.h"
#include "MintChoco.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Paint/PaintSubsystem.h"

static TAutoConsoleVariable<int32> CVarShowCoverage(
	TEXT("mc.ShowCoverage"),
	0,
	TEXT("1이면 팀별 페인트 점유 면적을 화면에 표시한다. 서버와 클라이언트가 각자 가진 값을 그린다."),
	ECVF_Default);

/** 화면 디버그 줄의 키. 같은 키에 다시 쓰면 새 줄이 쌓이지 않고 제자리에서 갱신된다. */
static constexpr uint64 CoverageDebugKeyBase = 0x4D430001;

AGameGameState::AGameGameState()
{
	StarPaint.SetNum(PaintTeamIdCount);
}

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
		PushStarPaint();
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

	DOREPLIFETIME(AGameGameState, StarPaint);
	DOREPLIFETIME(AGameGameState, SplatLog);
	DOREPLIFETIME(AGameGameState, WorldCoverage);
	DOREPLIFETIME(AGameGameState, MatchEndServerTime);
	DOREPLIFETIME(AGameGameState, WinningTeam);
	DOREPLIFETIME(AGameGameState, bMatchEnded);
	DOREPLIFETIME(AGameGameState, MatchPhase);
	DOREPLIFETIME(AGameGameState, CountdownEndServerTime);
	DOREPLIFETIME(AGameGameState, MatchDuration);
	DOREPLIFETIME(AGameGameState, KnockoutEndServerTime);
	DOREPLIFETIME(AGameGameState, KnockoutTeam);
	DOREPLIFETIME(AGameGameState, bEndedByKnockout);
}

bool AGameGameState::AllowsPlayerInput(EMatchPhase Phase)
{
	return Phase == EMatchPhase::Playing || Phase == EMatchPhase::Ended;
}

bool AGameGameState::IsPlayerInputAllowed(const UWorld* World)
{
	const AGameGameState* const State = World ? World->GetGameState<AGameGameState>() : nullptr;
	return !State || AllowsPlayerInput(State->MatchPhase);
}

void AGameGameState::SetMatchPhase(EMatchPhase NewPhase)
{
	if (!HasAuthority() || MatchPhase == NewPhase)
	{
		return;
	}
	MatchPhase = NewPhase;
	// RepNotify는 권한자에게 오지 않으므로 리슨 호스트는 직접 부른다.
	OnRep_MatchPhase();
}

void AGameGameState::OnRep_MatchPhase()
{
	UE_LOG(LogMintChoco, Log, TEXT("경기 단계: %s"), *UEnum::GetDisplayValueAsText(MatchPhase).ToString());
	BP_OnMatchPhaseChanged(MatchPhase);
}

void AGameGameState::SetCountdownEndTime(double InServerTime)
{
	if (HasAuthority())
	{
		CountdownEndServerTime = InServerTime;
	}
}

void AGameGameState::SetMatchDuration(float InSeconds)
{
	if (HasAuthority())
	{
		MatchDuration = FMath::Max(InSeconds, 0.0f);
	}
}

float AGameGameState::GetCountdownRemaining() const
{
	if (MatchPhase != EMatchPhase::Countdown || CountdownEndServerTime <= 0.0)
	{
		return 0.0f;
	}
	return static_cast<float>(FMath::Max(0.0, CountdownEndServerTime - GetServerWorldTimeSeconds()));
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
	SetMatchPhase(EMatchPhase::Ended);

	// RepNotify는 값을 쓴 권한자에게는 오지 않는다. 리슨 호스트의 화면도 갱신되도록 직접 부른다.
	HandleMatchEnded();
}

float AGameGameState::GetRemainingTime() const
{
	if (MatchEndServerTime <= 0.0)
	{
		// 경기 전: 아직 줄지 않은 한 판의 길이. 타이머를 걸지 않는 디버그 구성이면 0.
		return MatchDuration;
	}

	return static_cast<float>(FMath::Max(0.0, MatchEndServerTime - GetServerWorldTimeSeconds()));
}

void AGameGameState::OnRep_WorldCoverage()
{
	DrawCoverageDebug();
}

void AGameGameState::OnRep_MatchEnded()
{
	HandleMatchEnded();
}

void AGameGameState::HandleMatchEnded()
{
	DrawCoverageDebug();

	BP_OnMatchEnded(WinningTeam);
}

void AGameGameState::DrawCoverageDebug() const
{
	if (!GEngine || CVarShowCoverage.GetValueOnGameThread() == 0)
	{
		return;
	}

	// 같은 키에 다시 쓰면 줄이 제자리에서 갱신된다. 커버리지 갱신 주기보다 넉넉히 잡아
	// 갱신이 잠깐 끊겨도 화면에서 사라지지 않게 한다.
	constexpr float Duration = 2.0f;
	const TCHAR* const Side = HasAuthority() ? TEXT("서버") : TEXT("클라");

	GEngine->AddOnScreenDebugMessage(
		CoverageDebugKeyBase, Duration, FColor::White,
		FString::Printf(TEXT("[%s] 페인트 면적  (총 %.0f cm^2)"), Side, WorldCoverage.TotalArea));

	for (int32 Team = 0; Team < Teams::Count; ++Team)
	{
		const uint8 PaintId = static_cast<uint8>(Team);
		GEngine->AddOnScreenDebugMessage(
			CoverageDebugKeyBase + 1 + Team, Duration, Teams::GetDisplayColor(Team),
			FString::Printf(TEXT("  %s  %6.2f%%   %.0f cm^2"),
				Teams::GetDisplayName(Team),
				WorldCoverage.GetFraction(PaintId) * 100.0f,
				WorldCoverage.AreaByPaintId.IsValidIndex(PaintId) ? WorldCoverage.AreaByPaintId[PaintId] : 0.0f));
	}

	GEngine->AddOnScreenDebugMessage(
		CoverageDebugKeyBase + 1 + Teams::Count, Duration, FColor::Silver,
		FString::Printf(TEXT("  미도포  %6.2f%%"), WorldCoverage.GetFraction(PaintIdNone) * 100.0f));

	// 승패는 절대 점유율이 아니라 두 팀 사이의 상대 격차로 갈린다(AGameGameMode::OnMatchTimeExpired).
	// 무승부로 끝난 이유를 화면에서 바로 읽을 수 있도록 같은 값을 여기서도 보여준다.
	{
		const float MintFraction = WorldCoverage.GetFraction(static_cast<uint8>(Teams::Mint));
		const float ChocoFraction = WorldCoverage.GetFraction(static_cast<uint8>(Teams::Choco));
		const float Painted = MintFraction + ChocoFraction;
		const float Margin = Painted > 0.0f ? FMath::Abs(MintFraction - ChocoFraction) / Painted : 0.0f;

		GEngine->AddOnScreenDebugMessage(
			CoverageDebugKeyBase + 2 + Teams::Count, Duration, FColor::White,
			FString::Printf(TEXT("  상대 격차  %.1f%%"), Margin * 100.0f));
	}

	if (bMatchEnded)
	{
		GEngine->AddOnScreenDebugMessage(
			CoverageDebugKeyBase + 3 + Teams::Count, Duration, Teams::GetDisplayColor(WinningTeam),
			FString::Printf(TEXT("  경기 종료 — %s (WinningTeam %d)"),
				Teams::IsValidId(WinningTeam) ? Teams::GetDisplayName(WinningTeam) : TEXT("무승부"),
				WinningTeam));
	}
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

uint8 AGameGameState::BeginStarPaint(uint8 PaintId, float Duration, float FadeDuration)
{
	if (!HasAuthority() || !StarPaint.IsValidIndex(PaintId))
	{
		return 0;
	}

	const uint8 Gen = StarPaint[PaintId].Begin(GetServerWorldTimeSeconds(), Duration, FadeDuration);
	PushStarPaint();
	return Gen;
}

void AGameGameState::EndStarPaint(uint8 PaintId)
{
	if (!HasAuthority() || !StarPaint.IsValidIndex(PaintId))
	{
		return;
	}

	StarPaint[PaintId].End(GetServerWorldTimeSeconds());
	PushStarPaint();
}

void AGameGameState::OnRep_StarPaint()
{
	PushStarPaint();
}

void AGameGameState::OnRep_ReplicatedWorldTimeSecondsDouble()
{
	Super::OnRep_ReplicatedWorldTimeSecondsDouble();

	// 자국이 빛나거나 바래는 중일 때만 다시 민다. 평소에는 아무 표면도 이 값을 안 본다.
	const double ServerNow = GetServerWorldTimeSeconds();
	const bool bVisible = StarPaint.ContainsByPredicate(
		[ServerNow](const FStarPaintState& Team) { return Team.IsVisible(ServerNow); });
	if (bVisible)
	{
		PushStarPaint();
	}
}

void AGameGameState::PushStarPaint()
{
	UPaintSubsystem* const Paint = GetWorld()->GetSubsystem<UPaintSubsystem>();
	if (!Paint)
	{
		return;
	}

	// 재질의 Time 노드는 이 월드의 GetTimeSeconds를 센다. 서버 시각과의 차이만큼 옮겨 두면
	// 모든 머신이 같은 순간에 바래기 시작한다.
	const double Offset = GetServerWorldTimeSeconds() - GetWorld()->GetTimeSeconds();
	FPaintStarShaderState State;
	for (int32 Id = 0; Id < FMath::Min<int32>(StarPaint.Num(), PaintTeamIdCount); ++Id)
	{
		const FStarPaintState& Team = StarPaint[Id];
		State.Gen[Id] = Team.Gen;
		State.FadeStart[Id] = static_cast<float>(Team.GetFadeStart() - Offset);
		State.FadeDuration[Id] = FMath::Max(Team.FadeDuration, UE_KINDA_SMALL_NUMBER);
	}
	Paint->SetStarPaint(State);
}

void AGameGameState::RefreshCoverage()
{
	if (const UPaintSubsystem* const Paint = GetWorld()->GetSubsystem<UPaintSubsystem>())
	{
		WorldCoverage = Paint->GetWorldCoverage();
	}

	// RepNotify는 값을 쓴 권한자에게 오지 않으므로, 서버 화면은 여기서 직접 갱신한다.
	DrawCoverageDebug();

	// 커버리지가 막 갱신된 지금이 KO를 판정할 순간이다. 따로 타이머를 두면 두 값이 어긋난다.
	UpdateKnockout();
}

// ---------------------------------------------------------------- KO 판정

int32 FKnockoutMath::LeaderAboveThreshold(const FPaintCoverage& Coverage, float Threshold)
{
	if (Threshold <= 0.0f)
	{
		return Teams::None;
	}

	int32 Leader = Teams::None;
	float LeaderFraction = 0.0f;
	for (int32 Team = 0; Team < Teams::Count; ++Team)
	{
		const float Fraction = Coverage.GetFraction(static_cast<uint8>(Team));
		if (Fraction >= Threshold && Fraction > LeaderFraction)
		{
			Leader = Team;
			LeaderFraction = Fraction;
		}
	}
	return Leader;
}

float FKnockoutMath::Progress(float Remaining, float HoldSeconds)
{
	if (HoldSeconds <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(1.0f - Remaining / HoldSeconds, 0.0f, 1.0f);
}

bool AGameGameState::IsKnockoutPending() const
{
	// 팀과 시각이 같은 프레임에 복제되므로 둘 다 보고 판단한다. 경기가 끝난 뒤에는
	// 남아 있던 게이지가 결과 화면 위에 겹치지 않도록 꺼진다.
	return Teams::IsValidId(KnockoutTeam) && KnockoutEndServerTime > 0.0 && !bMatchEnded;
}

float AGameGameState::GetKnockoutRemaining() const
{
	if (!IsKnockoutPending())
	{
		return 0.0f;
	}
	return static_cast<float>(FMath::Max(0.0, KnockoutEndServerTime - GetServerWorldTimeSeconds()));
}

float AGameGameState::GetKnockoutProgress() const
{
	return IsKnockoutPending() ? FKnockoutMath::Progress(GetKnockoutRemaining(), KnockoutHoldSeconds) : 0.0f;
}

void AGameGameState::OnRep_KnockoutTeam()
{
	BP_OnKnockoutPendingChanged(IsKnockoutPending(), KnockoutTeam);
}

void AGameGameState::SetKnockoutTeam(int32 NewTeam)
{
	if (KnockoutTeam == NewTeam)
	{
		return;
	}

	KnockoutTeam = NewTeam;
	// 시각을 팀보다 먼저 써 둔다. 한 번치로 함께 복제되므로 RepNotify가 둘 다 본 상태에서 돈다.
	KnockoutEndServerTime = Teams::IsValidId(NewTeam)
		? GetServerWorldTimeSeconds() + KnockoutHoldSeconds
		: 0.0;

	if (Teams::IsValidId(NewTeam))
	{
		UE_LOG(LogMintChoco, Log, TEXT("KO 카운트다운 시작: %s (점유율 %.1f%%, %.1f초)"),
			Teams::GetDisplayName(NewTeam), WorldCoverage.GetFraction(static_cast<uint8>(NewTeam)) * 100.0f, KnockoutHoldSeconds);
	}
	else
	{
		UE_LOG(LogMintChoco, Log, TEXT("KO 카운트다운 취소: 기준 아래로 내려왔다."));
	}

	// RepNotify는 값을 쓴 권한자에게 오지 않는다. 리슨 호스트의 UI도 같이 움직이도록 직접 부른다.
	OnRep_KnockoutTeam();
}

void AGameGameState::UpdateKnockout()
{
	// 경기 중에만 센다. 카운트다운 단계나 종료 뒤의 점유율로 이길 수는 없다.
	if (MatchPhase != EMatchPhase::Playing || bMatchEnded)
	{
		SetKnockoutTeam(Teams::None);
		return;
	}

	const int32 Leader = FKnockoutMath::LeaderAboveThreshold(WorldCoverage, KnockoutThreshold);

	// 기준 아래로 내려왔거나 선두가 바뀌었다면 처음부터 다시 센다. SetKnockoutTeam이
	// 같은 팀이면 아무것도 하지 않으므로, 유지되는 동안 시각은 그대로 남는다.
	SetKnockoutTeam(Leader);
	if (!Teams::IsValidId(Leader))
	{
		return;
	}

	if (GetServerWorldTimeSeconds() < KnockoutEndServerTime)
	{
		return;
	}

	// 다 버텼다. 게임 모드가 아이템 스폰까지 정리하고 끝낸다.
	bEndedByKnockout = true;
	if (AGameGameMode* const Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameGameMode>() : nullptr)
	{
		Mode->EndMatchByKnockout(Leader);
	}
	else
	{
		// 게임 모드가 없는 구성(샘플 맵)에서도 결과는 남긴다.
		SetMatchResult(Leader);
	}
}
