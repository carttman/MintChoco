# 10단계 개발분이 머지 후에도 살아 있는지 기계적으로 확인한다.
#
#   pwsh -File Docs\MergeCheck.ps1          (저장소 루트에서)
#
# 값(숫자)까지는 못 본다 — .uasset 안의 수치는 에디터로 확인해야 하므로
# Docs/MergeRecovery.md 의 표를 보고 손으로 대조할 것.
# 여기서 잡는 것은 "통째로 사라진 것"이다.

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$script:Missing = @()
$script:Ok = 0

function Fail([string]$Step, [string]$What) {
    $script:Missing += [pscustomobject]@{ Step = $Step; What = $What }
}

function Need-File([string]$Step, [string]$Rel) {
    if (Test-Path (Join-Path $Root $Rel)) { $script:Ok++ } else { Fail $Step "파일 없음: $Rel" }
}

function Need-Text([string]$Step, [string]$Rel, [string]$Needle) {
    $full = Join-Path $Root $Rel
    if (-not (Test-Path $full)) { Fail $Step "파일 없음: $Rel"; return }
    if ((Get-Content $full -Raw) -like "*$Needle*") { $script:Ok++ }
    else { Fail $Step "$Rel 에 '$Needle' 없음" }
}

# .uasset 은 바이너리라 이름 문자열만 찾는다. 참조(에셋 경로)와 변수 이름은 평문으로 들어간다.
function Need-Asset([string]$Step, [string]$Rel, [string]$Needle) {
    $full = Join-Path $Root $Rel
    if (-not (Test-Path $full)) { Fail $Step "에셋 없음: $Rel"; return }
    $bytes = [System.IO.File]::ReadAllBytes($full)
    # Latin1 별칭은 .NET Framework(Windows PowerShell 5.1)에 없다. 코드페이지로 부른다.
    $text = [System.Text.Encoding]::GetEncoding(28591).GetString($bytes)
    if ($text.Contains($Needle)) { $script:Ok++ }
    else { Fail $Step "$Rel 에 '$Needle' 참조 없음" }
}

# 2026-09-13 main 통합: 0·2·3단계는 10fd596 에서 carttman 의 조준 구현(HandleFireInput / IsAimingItem /
# FItemAimPreviewStyle)으로 갈아탔으므로 그 이름을 본다. UItemAimAbility·AimArcPreview·ConfirmAim 은 죽은 코드다.
# 2026-09-13 main 통합: 9단계의 도넛 링(KnockoutGaugeWidget)과 점유율 바(PaintCoverageBarWidget)는 액체 바
# UPaintBarWidget(WBP_PaintBar) 하나로 합쳤다. 판정은 그대로 GameState 이고 위젯은 그 값을 읽는다.
# ---------------------------------------------------------------- 신규 파일
Need-File '0' 'Source/MintChoco/Items/ItemAimAbility.h'
Need-File '0' 'Source/MintChoco/Items/ItemAimAbility.cpp'
Need-File '2' 'Source/MintChoco/Items/AimArcPreview.h'
Need-File '2' 'Source/MintChoco/Items/AimArcPreview.cpp'
Need-File '3' 'Source/MintChoco/Items/LandingMarker.h'
Need-File '3' 'Source/MintChoco/Items/LandingMarker.cpp'
Need-File '9' 'Source/MintChoco/Game/PaintBarWidget.h'
Need-File '9' 'Source/MintChoco/Game/PaintBarWidget.cpp'
Need-File '9' 'Source/MintChoco/Tests/KnockoutTest.cpp'
Need-File '5' 'Source/MintChoco/Weapons/PaintVolley.h'
Need-File '5' 'Source/MintChoco/Weapons/PaintVolley.cpp'
Need-File '9' 'Content/Assets/UI/Widgets/Game/WBP_PaintBar.uasset'
Need-File '1' 'Content/Blueprints/Items/BP_BombardmentAimLine.uasset'
Need-File '2' 'Content/Blueprints/Items/BP_HoneyBalloonAimArc.uasset'
Need-File '5' 'Content/Blueprints/Weapons/Paintballs/DA_Paintball_SniperVolley.uasset'

# ------------------------------------------------------- 8단계 초콜릿 분수
Need-Text '8' 'Source/MintChoco/Items/ChocolateFountain.h' 'GetPaintId'

# ------------------------------------------------- 4단계 무한 탄환 발사 속도
Need-Text '4' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'FreeShotInterval'
Need-Text '4' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'FreeShotChargeTime'
Need-Text '4' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'GetEffectiveShotInterval'
Need-Text '4' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'GetEffectiveChargeTime'
Need-Text '4' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'LastShotTime'
Need-Text '4' 'Source/MintChoco/Weapons/PaintWeaponProfile.h' 'case EPaintFireMode::Single'

# ----------------------------------------------------- 0단계 조준 모드 토대
Need-Text '0' 'Source/MintChoco/Items/ItemGameplayTags.h' 'State_Item_Aiming'
Need-Text '0' 'Source/MintChoco/Items/ItemGameplayTags.cpp' 'State.Item.Aiming'
Need-Text '0' 'Source/MintChoco/Items/ItemSlotComponent.h' 'ConfirmAim'
Need-Text '0' 'Source/MintChoco/Items/ItemSlotComponent.h' 'OnAimConfirmed'
Need-Text '0' 'Source/MintChoco/Items/ItemSlotComponent.h' 'ServerConfirmAim'
Need-Text '0' 'Source/MintChoco/Game/Unit.cpp' 'ItemSlot->HandleFireInput()'
Need-Text '0' 'Source/MintChoco/Items/ItemSlotComponent.h' 'HandleFireInput'

# --------------------------------------------------- 1단계 디저트 폭격 조준
Need-Text '1' 'Source/MintChoco/Items/DessertBombardmentProfile.h' 'AimPreviewClass'
Need-Text '1' 'Source/MintChoco/Items/DessertBombardmentAbility.h' 'IsAimingItem'
Need-Text '1' 'Source/MintChoco/Items/ItemGameplayEffect.h' 'UGE_DessertBombardment'
Need-Asset '1' 'Content/Blueprints/Items/DA_Item_DessertBombardment.uasset' 'BP_BombardmentAimLine'

# ------------------------------------------------------- 2단계 꿀풍선 조준
Need-Text '2' 'Source/MintChoco/Items/HoneyBalloonProfile.h' 'FItemAimPreviewStyle AimPreview'
Need-Text '2' 'Source/MintChoco/Items/HoneyBalloonAbility.h' 'IsAimingItem'
Need-Text '2' 'Source/MintChoco/Items/HoneyBalloonAbility.cpp' 'GetThrowOrigin'
Need-Text '2' 'Source/MintChoco/Items/ItemGameplayEffect.h' 'UGE_HoneyBalloon'
Need-Text '2' 'Source/MintChoco/Items/ItemProjectile.h' 'GetCollisionRadius'
Need-Asset '2' 'Content/Blueprints/Items/DA_Item_HoneyBalloon.uasset' 'RT_LifeLock_Start'

# --------------------------------------- 계획 외: 샷건 2단 중력 (7단계 대체)
Need-Text '7' 'Source/MintChoco/Weapons/PaintballProfile.h' 'DropAfter'
Need-Text '7' 'Source/MintChoco/Weapons/PaintballProfile.h' 'DropGravityScale'
Need-Text '7' 'Source/MintChoco/Weapons/PaintProjectile.h' 'ApplyDropGravity'
Need-Text '7' 'Source/MintChoco/Weapons/PaintProjectile.h' 'InDropAfterOverride'

# --------------------------------------------------------- 9단계 KO 판정
Need-Text '9' 'Source/MintChoco/Game/GameGameState.h' 'FKnockoutMath'
Need-Text '9' 'Source/MintChoco/Game/GameGameState.h' 'IsKnockoutPending'
Need-Text '9' 'Source/MintChoco/Game/GameGameState.h' 'KnockoutEndServerTime'
Need-Text '9' 'Source/MintChoco/Game/GameGameState.h' 'BP_OnKnockoutPendingChanged'
Need-Text '9' 'Source/MintChoco/Game/GameGameState.cpp' 'UpdateKnockout()'
Need-Text '9' 'Source/MintChoco/Game/GameGameMode.h' 'EndMatchByKnockout'
Need-Asset '9' 'Content/Assets/UI/Widgets/Game/WBP_GameHUD.uasset' 'WBP_PaintBar'
Need-Text '9' 'Source/MintChoco/Game/PaintBarWidget.cpp' 'GetKnockoutProgress'

# --------------------------------------------------- 5단계 차지샷 순차 발사
Need-Text '5' 'Source/MintChoco/Weapons/PaintSniperProfile.h' 'VolleyPaintball'
Need-Text '5' 'Source/MintChoco/Weapons/PaintSniperProfile.h' 'VolleyDropLead'
Need-Text '5' 'Source/MintChoco/Weapons/PaintSniperProfile.h' 'bSkipTrailWhenVolleying'
Need-Text '5' 'Source/MintChoco/Weapons/PaintSniperProfile.cpp' 'SpawnTrailVolley'
# 풀을 거치는 경로가 발마다 다른 낙하 시각을 버리면 순차 발사가 평범한 산탄이 된다.
Need-Text '5' 'Source/MintChoco/Weapons/ProjectilePoolSubsystem.h' 'DropAfterOverride'
Need-Text '5' 'Source/MintChoco/Weapons/PaintballProfile.cpp' 'bCosmetic, DropAfterOverride'
Need-Asset '5' 'Content/Blueprints/Weapons/Profiles/DA_Weapon_Sniper.uasset' 'DA_Paintball_SniperVolley'

# ------------------------------------- 계획 외: 발사 자세와 총구 정합 (최고 위험)
Need-Text '자세' 'Source/MintChoco/Game/UnitAnimInstance.h' 'bWeaponPoseHeld'
Need-Text '자세' 'Source/MintChoco/Game/UnitAnimInstance.h' 'bIsAiming'
# 헤더에 선언만 남고 .cpp의 계산이 통째로 사라진 적이 있다. 값을 실제로 채우는 줄을 본다.
Need-Text '자세' 'Source/MintChoco/Game/UnitAnimInstance.cpp' 'bWeaponPoseHeld ='
Need-Text '자세' 'Source/MintChoco/Game/UnitAnimInstance.cpp' 'Weapon->IsAiming()'
Need-Text '자세' 'Source/MintChoco/Game/UnitDataAsset.h' 'Charge'
Need-Text '자세' 'Source/MintChoco/Game/Unit.h' 'StartChargePose'
Need-Text '자세' 'Source/MintChoco/Game/Unit.h' 'HandleChargingChanged'
Need-Text '자세' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'OnChargingChanged'
Need-Text '자세' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'GetMuzzleAttachment'
Need-Text '자세' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'FireWhenAimReady'
Need-Text '자세' 'Source/MintChoco/Weapons/PaintWeaponComponent.h' 'AimHoldSeconds'
Need-Asset '자세' 'Content/Assets/RT_UnitAnimations/RT_ABP_Unit_V2.uasset' 'bWeaponPoseHeld'
Need-Asset '자세' 'Content/Game/Data/DA_Unit_Mint.uasset' 'Idle_Combat_V2'
Need-Asset '자세' 'Content/Game/Data/DA_Unit_Choco.uasset' 'Idle_Combat_V2'

# --------------------------------------------------------- 3단계 히어로 랜딩
Need-Text '3' 'Source/MintChoco/Game/UnitMovementComponent.h' 'SetWantsHeroDive'
Need-Text '3' 'Source/MintChoco/Game/UnitMovementComponent.h' 'GetHeroCharge'
Need-Text '3' 'Source/MintChoco/Game/UnitMovementComponent.h' 'SavedHeroCharge'
Need-Text '3' 'Source/MintChoco/Game/UnitMovementComponent.cpp' 'FLAG_Custom_3'
Need-Text '3' 'Source/MintChoco/Items/HeroLandingAbility.cpp' 'SetWantsHeroDive(true)'
Need-Text '3' 'Source/MintChoco/Items/HeroLandingProfile.h' 'ChargeScaleFor'
Need-Text '3' 'Source/MintChoco/Items/HeroLandingProfile.h' 'MinChargeScale'
Need-Text '3' 'Source/MintChoco/Items/HeroLandingAbility.cpp' 'ALandingMarker'
Need-Asset '3' 'Content/Blueprints/Items/BP_LandingMarker.uasset' 'MintChoco.LandingMarker'

# ---------------------------------------------------------------- 결과
Write-Host ''
if ($script:Missing.Count -eq 0) {
    Write-Host "통과: $($script:Ok)개 항목 모두 살아 있음." -ForegroundColor Green
    exit 0
}

Write-Host "통과 $($script:Ok)개 / 실패 $($script:Missing.Count)개" -ForegroundColor Yellow
Write-Host ''
$script:Missing | Sort-Object Step | Format-Table -AutoSize
Write-Host "Docs/MergeRecovery.md 에서 해당 단계를 찾아 복구할 것." -ForegroundColor Yellow
exit 1
