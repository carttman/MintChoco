# Liquid Flipbook 이펙트 5종 연결 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 팩의 나이아가라 시스템 5개를 꿀풍선과 같은 텍스처·색으로 맞춘 프로젝트 복사본으로 만들고,
차지샷 충전/발사, 샷건 발사, 히어로 랜딩 상승/착지 순간에 각각 붙인다.

**Architecture:** 연출이 나가는 시점은 이미 전부 존재한다(무기의 `OnFired` 세 지점, 아이템의 상태 태그,
`APaintBurst`의 복제 스폰). 따라서 C++는 **재생할 에셋과 배율을 담을 데이터 필드**와 **그 세 지점에서
나이아가라를 한 번 띄우는 코드**만 늘린다. 새 액터도 새 서브시스템도 없다. 예외는 충전 루프 하나뿐인데,
충전 상태가 지금 어느 머신에도 복제되지 않으므로 `bCharging` 복제 프로퍼티 하나를 추가한다.
크기 조절은 전부 스폰 시점의 `FVector Scale` 인자로 끝낸다(나이아가라 에셋 자체는 수정하지 않는다).

**Tech Stack:** UE 5.8, C++ (Epic Coding Standard), Niagara, GAS, Unreal MCP 툴셋, `UnrealEditor-Cmd`
헤드리스 자동화 테스트.

**Spec:** [docs/superpowers/specs/2026-09-11-liquid-flip-vfx.md](../specs/2026-09-11-liquid-flip-vfx.md)

## Global Constraints

- C++는 Epic Coding Standard: 탭 인덴트, PascalCase, `U`/`A`/`F`/`E` 접두어. 주석은 그 파일이 쓰는
  언어를 따른다(`Weapons/*`는 영어, `Items/*`·`Game/*`는 한국어).
- 새 `UPROPERTY`는 모듈 재빌드 + **에디터 재시작** 전까지 `ObjectTools`에 보이지 않는다. 에셋에 값을
  넣는 작업(Task 7)은 코드 태스크가 모두 끝나고 에디터를 재시작한 뒤에 한다.
- PIE가 도는 동안 `AssetTools.save_assets` / `exists`는 "Asset does not exist"로 실패한다. 저장은 PIE를 멈추고.
- 헤드리스 테스트는 에디터와 **같이** 띄우지 않는다(포트 8000을 먼저 잡은 쪽이 MCP를 가져간다).
  테스트를 먼저 돌리고 그 다음 에디터를 띄운다.
- 배열 프로퍼티는 한 번에 한 칸만 늘리고, 늘리기 직전에 다시 읽는다. 길이가 그대로인 제자리 수정만
  한 번에 안전하다(`OverrideMaterials`는 길이 1 유지 → 제자리 수정).
- 나이아가라 에셋은 팩 원본(`/Game/Liquid_Flipbook_VFX/**`)을 수정하지 않는다. 복사본만 만진다.
- 스케일은 **균일 배율**이다. 1.0이 에셋 원래 크기.
- 테스트 명령:
  `UnrealEditor-Cmd.exe D:\GitHub\MintChoco\MintChoco.uproject -ExecCmds="Automation RunTests MintChoco; Quit" -unattended -nullrhi -abslog=D:\GitHub\MintChoco\Saved\Logs\AutoTest.log`
  후 로그에서 `Test Completed. Result=` 확인.

---

### Task 1: 파열 연출에 배율 필드

히어로 랜딩 착지 이펙트(`NS_Liquid_Flip_9`)를 1.5배로 띄울 토대. `APaintBurst`는 이미 착지 지점에서
`BurstFX`를 1회 재생하므로, 필요한 것은 배율 인자뿐이다.

**Files:**
- Modify: `Source/MintChoco/Weapons/PaintBurst.h:63` (`FPaintBurstParams`의 `BurstFX` 바로 아래)
- Modify: `Source/MintChoco/Weapons/PaintBurst.cpp:117` (`SpawnBurstFX`의 스폰 호출)
- Test: `Source/MintChoco/Tests/PaintBurstTest.cpp` (파일 끝에 테스트 추가)

**Interfaces:**
- Consumes: 없음
- Produces: `FPaintBurstParams::BurstFXScale` (float, 기본 1.0). Task 7이 `DA_Item_HeroLanding.Burst.BurstFXScale`에 1.5를 넣는다.

- [ ] **Step 1: 실패하는 테스트를 쓴다**

`Source/MintChoco/Tests/PaintBurstTest.cpp`의 `#endif` 앞에 추가:

```cpp
/** 배율 기본값은 에셋 원래 크기다: 값을 넣지 않은 아이템의 연출이 갑자기 커지거나 사라지지 않는다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintBurstFXScaleDefaultTest,
	"MintChoco.Weapons.Burst.FXScaleDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintBurstFXScaleDefaultTest::RunTest(const FString& Parameters)
{
	const FPaintBurstParams Params;
	TestEqual(TEXT("BurstFXScale defaults to 1"), Params.BurstFXScale, 1.0f);
	return true;
}
```

- [ ] **Step 2: 컴파일이 깨지는 것을 확인한다**

Run: `UnrealBuildTool`이 아니라 테스트 명령(Global Constraints)을 그대로 실행.
Expected: 빌드 실패 — `'BurstFXScale': is not a member of 'FPaintBurstParams'`

- [ ] **Step 3: 필드를 추가한다**

`PaintBurst.h`의 `BurstFX` 선언 바로 뒤:

```cpp
	/** BurstFX가 뜨는 균일 배율. 1이 에셋 원래 크기다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burst", meta = (ClampMin = "0.01"))
	float BurstFXScale = 1.0f;
```

- [ ] **Step 4: 스폰이 배율을 쓰게 한다**

`PaintBurst.cpp`의 `SpawnBurstFX` 마지막 호출을 교체:

```cpp
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, Params.BurstFX, GetActorLocation(), FRotator::ZeroRotator, FVector(Params.BurstFXScale));
```

- [ ] **Step 5: 테스트 통과 확인**

Run: 테스트 명령. Expected: `MintChoco.Weapons.Burst.FXScaleDefault` … `Result=Success`

- [ ] **Step 6: 커밋**

```bash
git add Source/MintChoco/Weapons/PaintBurst.h Source/MintChoco/Weapons/PaintBurst.cpp Source/MintChoco/Tests/PaintBurstTest.cpp
git commit -m "feat: 파열 연출에 스폰 배율 필드 추가"
```

---

### Task 2: 아이템 부착 이펙트에 배율 필드

히어로 랜딩 상승 이펙트(`NS_Liquid_Flip_8`)를 1.5배로. 상태 태그가 붙는 순간 캐릭터 메시에 부착
재생하는 경로가 이미 있으므로 배율만 넘긴다.

**Files:**
- Modify: `Source/MintChoco/Items/ItemProfile.h:52` (`ActivateFX` 바로 아래)
- Modify: `Source/MintChoco/Items/ItemSlotComponent.cpp:241` (`StartEffectFeedback`의 스폰 호출)
- Test: `Source/MintChoco/Tests/ItemProfileAssetTest.cpp` (기존 에셋 검사에 한 줄)

**Interfaces:**
- Consumes: 없음
- Produces: `UItemProfile::ActivateFXScale` (float, 기본 1.0). Task 7이 `DA_Item_HeroLanding`에 1.5를 넣는다.

- [ ] **Step 1: 실패하는 테스트를 쓴다**

`ItemProfileAssetTest.cpp`에서 프로필 하나를 검사하는 루프 안(각 `UItemProfile*`를 `Item`으로 들고 있는 자리)에 추가:

```cpp
		TestTrue(*FString::Printf(TEXT("%s: ActivateFXScale is positive"), *Name), Item->ActivateFXScale > 0.0f);
```

해당 루프의 지역 변수 이름(`Item`, `Name`)은 파일의 기존 코드를 그대로 따른다. 루프가 없고 개별
검사만 있으면, 검사 헬퍼 람다 안에 같은 한 줄을 넣는다.

- [ ] **Step 2: 컴파일이 깨지는 것을 확인한다**

Run: 테스트 명령. Expected: `'ActivateFXScale': is not a member of 'UItemProfile'`

- [ ] **Step 3: 필드를 추가한다**

`ItemProfile.h`의 `ActivateFX` 선언 바로 뒤:

```cpp
	/** ActivateFX가 붙을 때의 균일 배율. 1이 에셋 원래 크기다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.01"))
	float ActivateFXScale = 1.0f;
```

- [ ] **Step 4: 스폰이 배율을 쓰게 한다**

`ItemSlotComponent.cpp`의 `StartEffectFeedback` 안 스폰을 교체(배율을 받는 오버로드는 인자 순서가
다르다: `Scale`이 `LocationType` 앞, `PoolingMethod`가 `bAutoActivate` 앞):

```cpp
		UNiagaraComponent* const FX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Item.ActivateFX, AttachTo, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			FVector(Item.ActivateFXScale), EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true,
			ENCPoolMethod::None);
```

`ENCPoolMethod`는 `NiagaraComponentPool.h`에 있다. `ItemSlotComponent.cpp`가 아직 그 헤더를 포함하지
않으면 `#include "NiagaraComponentPool.h"`를 나이아가라 인클루드 옆에 추가한다.

- [ ] **Step 5: 테스트 통과 확인**

Run: 테스트 명령. Expected: `MintChoco.Items.*ProfileAssets*` … `Result=Success`

- [ ] **Step 6: 커밋**

```bash
git add Source/MintChoco/Items/ItemProfile.h Source/MintChoco/Items/ItemSlotComponent.cpp Source/MintChoco/Tests/ItemProfileAssetTest.cpp
git commit -m "feat: 아이템 부착 이펙트에 스폰 배율 필드 추가"
```

---

### Task 3: 무기 프로필의 총구 이펙트 필드와 충전량 배율 공식

무기 연출은 캐릭터가 아니라 무기의 성질이므로 `UnitDataAsset`(캐릭터별)이 아니라 무기 프로필에 넣는다.
계산은 순수 함수라 단위 테스트로 고정한다.

**Files:**
- Modify: `Source/MintChoco/Weapons/PaintWeaponProfile.h` (`FPaintShot`에 필드 1개, 프로필 클래스에 필드 4개 + 함수 1개)
- Test: `Source/MintChoco/Tests/PaintProfileAssetTest.cpp` (새 테스트를 파일 끝 `#endif` 앞에 추가)

**Interfaces:**
- Consumes: 없음
- Produces:
  - `UPaintWeaponProfile::MuzzleFX` (`TObjectPtr<UNiagaraSystem>`)
  - `UPaintWeaponProfile::MuzzleFXScale` (float, 1.0)
  - `UPaintWeaponProfile::MuzzleFXChargeScale` (`FVector2D`, (0.5, 1.5))
  - `UPaintWeaponProfile::ChargeFX` (`TObjectPtr<UNiagaraSystem>`), `ChargeFXScale` (float, 1.0)
  - `float UPaintWeaponProfile::GetMuzzleFXScale(float ChargeFraction) const`
  - `FPaintShot::Charge` (uint8, 기본 255)
  Task 4와 5가 이 이름들을 쓴다.

- [ ] **Step 1: 실패하는 테스트를 쓴다**

`PaintProfileAssetTest.cpp`의 `#endif` 앞에 추가(같은 파일이 이미 `PaintSniperProfile.h`,
`PaintScatterProfile.h`를 포함한다):

```cpp
/**
 * 충전량이 총구 연출의 크기를 정하는 방식. 최소 충전에서 아래끝, 풀충전에서 위끝이고,
 * 풀충전만 발사하는 프로필(MinChargeToFire 1)은 위끝 하나만 쓴다. 차지가 아닌 무기는 배율이 고정이다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintMuzzleFXScaleTest,
	"MintChoco.Paint.Weapons.MuzzleFXScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintMuzzleFXScaleTest::RunTest(const FString& Parameters)
{
	UPaintSniperProfile* const Charged = NewObject<UPaintSniperProfile>();
	Charged->FireMode = EPaintFireMode::Charged;
	Charged->MinChargeToFire = 0.3f;
	Charged->MuzzleFXScale = 1.0f;
	Charged->MuzzleFXChargeScale = FVector2D(0.5, 1.5);

	TestEqual(TEXT("minimum charge is the low end"), Charged->GetMuzzleFXScale(0.3f), 0.5f, 1e-4f);
	TestEqual(TEXT("full charge is the high end"), Charged->GetMuzzleFXScale(1.0f), 1.5f, 1e-4f);
	TestEqual(TEXT("halfway between is halfway"), Charged->GetMuzzleFXScale(0.65f), 1.0f, 1e-4f);
	TestEqual(TEXT("below the minimum clamps to the low end"), Charged->GetMuzzleFXScale(0.0f), 0.5f, 1e-4f);

	// 기본 배율은 곱해진다: 에셋이 두 배로 크면 두 끝도 두 배다.
	Charged->MuzzleFXScale = 2.0f;
	TestEqual(TEXT("MuzzleFXScale multiplies the charge scale"), Charged->GetMuzzleFXScale(1.0f), 3.0f, 1e-4f);

	// 풀충전만 발사하면 도달 가능한 크기는 위끝 하나다.
	Charged->MuzzleFXScale = 1.0f;
	Charged->MinChargeToFire = 1.0f;
	TestEqual(TEXT("a full-charge-only weapon always uses the high end"), Charged->GetMuzzleFXScale(1.0f), 1.5f, 1e-4f);

	UPaintGunProfile* const Single = NewObject<UPaintGunProfile>();
	Single->FireMode = EPaintFireMode::Single;
	Single->MuzzleFXScale = 1.25f;
	Single->MuzzleFXChargeScale = FVector2D(0.5, 1.5);
	TestEqual(TEXT("a non-charged weapon ignores the charge range"), Single->GetMuzzleFXScale(1.0f), 1.25f, 1e-4f);
	TestEqual(TEXT("a non-charged weapon ignores the charge value"), Single->GetMuzzleFXScale(0.0f), 1.25f, 1e-4f);

	return true;
}
```

- [ ] **Step 2: 컴파일이 깨지는 것을 확인한다**

Run: 테스트 명령. Expected: `'MuzzleFXChargeScale': is not a member of ...`

- [ ] **Step 3: 필드와 공식을 추가한다**

`PaintWeaponProfile.h` 상단 전방 선언에 한 줄:

```cpp
class UNiagaraSystem;
```

`FPaintShot`의 `PaintId` 뒤에:

```cpp
	/**
	 * How charged the shot was, in 1/255 steps; 255 for every non-charged mode. The machines that
	 * only replay the shot have no press of their own to measure, so the muzzle FX scale rides here.
	 */
	UPROPERTY()
	uint8 Charge = 255;
```

`UPaintWeaponProfile`의 `InkCostPercent`가 있는 `private:` 앞, 즉 public 프로퍼티 블록 끝에:

```cpp
	/**
	 * Played once per accepted shot, on every machine that renders the shooter. Attached to the
	 * owner's muzzle socket so it follows the gun; unset plays nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<UNiagaraSystem> MuzzleFX;

	/** Uniform scale MuzzleFX spawns at, before any charge scaling. 1 is the asset's own size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0.01"))
	float MuzzleFXScale = 1.0f;

	/**
	 * In Charged, the multiplier at MinChargeToFire (X) and at a full charge (Y). A weapon that
	 * only fires at a full charge can reach Y alone, so lowering MinChargeToFire is what makes the
	 * range visible.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX",
		meta = (EditCondition = "FireMode == EPaintFireMode::Charged"))
	FVector2D MuzzleFXChargeScale = FVector2D(0.5, 1.5);

	/**
	 * Looping FX for a Charged weapon's hold: spawned at the muzzle when the trigger goes down and
	 * switched off when the shot leaves or the hold is cancelled. Unset shows nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX",
		meta = (EditCondition = "FireMode == EPaintFireMode::Charged"))
	TObjectPtr<UNiagaraSystem> ChargeFX;

	/** Uniform scale ChargeFX spawns at. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX",
		meta = (ClampMin = "0.01", EditCondition = "FireMode == EPaintFireMode::Charged"))
	float ChargeFXScale = 1.0f;

	/**
	 * Uniform scale one shot's muzzle FX spawns at. Charged walks between the ends of
	 * MuzzleFXChargeScale by how far past MinChargeToFire the shot got; every other mode is fixed.
	 */
	UFUNCTION(BlueprintPure, Category = "FX")
	float GetMuzzleFXScale(float ChargeFraction) const
	{
		if (FireMode != EPaintFireMode::Charged)
		{
			return MuzzleFXScale;
		}
		const float Minimum = FMath::Clamp(MinChargeToFire, 0.0f, 1.0f);
		// A full-charge-only weapon has one reachable size; dividing by zero here would be it too.
		const float Alpha = Minimum >= 1.0f
			? 1.0f
			: FMath::Clamp((ChargeFraction - Minimum) / (1.0f - Minimum), 0.0f, 1.0f);
		return MuzzleFXScale * FMath::Lerp(static_cast<float>(MuzzleFXChargeScale.X), static_cast<float>(MuzzleFXChargeScale.Y), Alpha);
	}
```

- [ ] **Step 4: 테스트 통과 확인**

Run: 테스트 명령. Expected: `MintChoco.Paint.Weapons.MuzzleFXScale` … `Result=Success`

- [ ] **Step 5: 커밋**

```bash
git add Source/MintChoco/Weapons/PaintWeaponProfile.h Source/MintChoco/Tests/PaintProfileAssetTest.cpp
git commit -m "feat: 무기 프로필에 총구 이펙트 필드와 충전량 배율 공식 추가"
```

---

### Task 4: 발사 순간의 총구 이펙트 (샷건, 차지샷 발사)

`OnFired`가 발생하는 세 지점에 그대로 붙인다. 소유자는 예측 시점, 서버는 실사격 시점, 나머지 머신은
샷 멀티캐스트 시점 — 각 머신에서 정확히 한 번이다. 원격 머신은 자기 누름이 없으므로 충전량을
`FPaintShot::Charge`로 받아 배율을 계산한다.

**Files:**
- Modify: `Source/MintChoco/Weapons/PaintWeaponComponent.h` (private 헬퍼 2개 선언)
- Modify: `Source/MintChoco/Weapons/PaintWeaponComponent.cpp` (인클루드 2개, `FireOnce`, `ServerFire_Implementation`, `MulticastShotFired_Implementation`, 헬퍼 정의)
- Test: 없음(네트워크 3경로는 자동화 테스트로 재현할 수 없다. Task 8의 PIE 검증으로 확인한다)

**Interfaces:**
- Consumes: Task 3의 `MuzzleFX`, `GetMuzzleFXScale`, `FPaintShot::Charge`
- Produces: `void UPaintWeaponComponent::PlayMuzzleFX(float ChargeFraction)`,
  `USkeletalMeshComponent* UPaintWeaponComponent::GetMuzzleMesh() const` — Task 5가 `GetMuzzleMesh`를 재사용한다.

- [ ] **Step 1: 인클루드를 추가한다**

`PaintWeaponComponent.cpp` 상단 엔진 인클루드 블록에:

```cpp
#include "NiagaraComponent.h"
#include "NiagaraComponentPool.h"
#include "NiagaraFunctionLibrary.h"
```

- [ ] **Step 2: 헬퍼를 선언한다**

`PaintWeaponComponent.h`의 `private:` 블록에서 `ComputeMuzzleTransform` 선언 아래:

```cpp
	/** The mesh that carries the muzzle socket, or null when the owner has no such socket. */
	USkeletalMeshComponent* GetMuzzleMesh() const;

	/** Plays the profile's muzzle FX once on this machine. Charge only matters in Charged. */
	void PlayMuzzleFX(float ChargeFraction);
```

- [ ] **Step 3: 헬퍼를 구현한다**

`PaintWeaponComponent.cpp`의 `ComputeMuzzleTransform` 정의 바로 아래:

```cpp
USkeletalMeshComponent* UPaintWeaponComponent::GetMuzzleMesh() const
{
	AActor* const Owner = GetOwner();
	ACharacter* const Character = Cast<ACharacter>(Owner);
	USkeletalMeshComponent* const Mesh =
		Character ? Character->GetMesh() : (Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr);
	return (Mesh && !MuzzleSocketName.IsNone() && Mesh->DoesSocketExist(MuzzleSocketName)) ? Mesh : nullptr;
}

void UPaintWeaponComponent::PlayMuzzleFX(float ChargeFraction)
{
	UWorld* const World = GetWorld();
	if (!Profile || !Profile->MuzzleFX || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FVector Scale(Profile->GetMuzzleFXScale(ChargeFraction));

	// Attached, so a one-shot flash stays on the barrel while the gun moves.
	if (USkeletalMeshComponent* const Mesh = GetMuzzleMesh())
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			Profile->MuzzleFX, Mesh, MuzzleSocketName, FVector::ZeroVector, FRotator::ZeroRotator,
			Scale, EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true, ENCPoolMethod::None);
		return;
	}

	// Socketless: the muzzle hangs off the view, so the flash is left where the shot left from.
	const FTransform Muzzle = GetMuzzleTransform();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, Profile->MuzzleFX, Muzzle.GetLocation(), Muzzle.Rotator(), Scale);
}
```

- [ ] **Step 4: 소유자와 서버의 발사에 붙인다**

`FireOnce`에서 `Profile->Fire`가 성공한 뒤, `SpendShot()` 호출 앞에 충전량을 샷에 실어 보낸다:

```cpp
	// The machines that only replay this shot size their flash by it.
	Shot.Charge = static_cast<uint8>(FMath::RoundToInt(ChargeFraction * 255.0f));
```

같은 함수의 `OnFired.Broadcast(Seed);` 바로 앞에:

```cpp
	PlayMuzzleFX(ChargeFraction);
```

`ServerFire_Implementation`의 성공 블록을 아래처럼 바꾼다(샷을 멀티캐스트하기 **전에** 충전량을 싣는다):

```cpp
	if (Profile->Fire(Context, FreshStroke, Shot))
	{
		Shot.Charge = Charge;
		SpendShot();
		MulticastShotFired(Shot);
		PlayMuzzleFX(Context.ChargeFraction);
		OnFired.Broadcast(Seed);
	}
```

- [ ] **Step 5: 구경하는 머신에 붙인다**

`MulticastShotFired_Implementation`의 `Profile->PlayCosmetic(...)` 호출 뒤, `OnFired.Broadcast(Shot.Seed);` 앞에:

```cpp
	PlayMuzzleFX(Shot.Charge / 255.0f);
```

- [ ] **Step 6: 빌드와 기존 테스트가 깨지지 않는지 확인**

Run: 테스트 명령. Expected: 빌드 성공, `MintChoco` 전체 `Result=Success`

- [ ] **Step 7: 커밋**

```bash
git add Source/MintChoco/Weapons/PaintWeaponComponent.h Source/MintChoco/Weapons/PaintWeaponComponent.cpp
git commit -m "feat: 발사 순간 총구 이펙트를 머신당 한 번 재생"
```

---

### Task 5: 차지샷 충전 루프 이펙트 (모두에게 보이게)

충전 상태는 지금 어디에도 복제되지 않는다(`PressTime`은 로컬, `bTriggerHeld`도 로컬). 적·아군이
상대의 충전을 보려면 상태 하나를 복제해야 한다. 소유자는 자기 예측으로 즉시 켜고, 서버가 나머지
머신에 알린다. 끄는 경로는 `CancelTrigger` 하나로 모은다 — 발사, 취소, 프로필 교체, 컴포넌트 종료가
모두 그리로 지나간다.

**Files:**
- Modify: `Source/MintChoco/Weapons/PaintWeaponComponent.h` (복제 프로퍼티 1개, 컴포넌트 포인터 1개, 함수 4개)
- Modify: `Source/MintChoco/Weapons/PaintWeaponComponent.cpp` (`GetLifetimeReplicatedProps`, `PullTrigger`, `CancelTrigger`, `EndPlay`, 새 함수 정의)
- Test: `Source/MintChoco/Tests/PaintProfileAssetTest.cpp` (차지 프로필의 충전 연출 필드 정합성 검사)

**Interfaces:**
- Consumes: Task 3의 `ChargeFX`, `ChargeFXScale`; Task 4의 `GetMuzzleMesh`
- Produces: `UPaintWeaponComponent::bCharging` (복제, `COND_SkipOwner`), `SetCharging(bool)`,
  `StartChargeFX()`, `StopChargeFX()`, `ServerSetCharging(bool)`, `OnRep_Charging()`

- [ ] **Step 1: 실패하는 테스트를 쓴다**

`PaintProfileAssetTest.cpp`의 `#endif` 앞에 추가:

```cpp
/**
 * 충전 연출은 차지 무기에서만 의미가 있다. 차지가 아닌 프로필에 ChargeFX가 들어 있으면
 * 영원히 재생되지 않는 에셋 참조이므로, 값을 넣은 쪽의 FireMode가 Charged인지 본다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintChargeFXProfileTest,
	"MintChoco.Paint.Weapons.ChargeFXProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPaintChargeFXProfileTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ProfileFolder}, /*bForceRescan=*/true);

	TArray<FAssetData> Profiles;
	FindProfileAssets(Registry, UPaintWeaponProfile::StaticClass(), Profiles);
	TestTrue(TEXT("profile templates were found"), Profiles.Num() > 0);

	for (const FAssetData& Asset : Profiles)
	{
		const UPaintWeaponProfile* const Profile = Cast<UPaintWeaponProfile>(Asset.GetAsset());
		if (!Profile || !Profile->ChargeFX)
		{
			continue;
		}
		TestEqual(
			*FString::Printf(TEXT("%s: ChargeFX is only reachable in Charged"), *Asset.AssetName.ToString()),
			Profile->FireMode, EPaintFireMode::Charged);
		TestTrue(
			*FString::Printf(TEXT("%s: ChargeFXScale is positive"), *Asset.AssetName.ToString()),
			Profile->ChargeFXScale > 0.0f);
	}
	return true;
}
```

- [ ] **Step 2: 테스트가 아직 아무것도 막지 않는 것을 확인한다**

Run: 테스트 명령. Expected: `MintChoco.Paint.Weapons.ChargeFXProfiles` … `Result=Success`
(에셋에 값이 없으니 통과한다. Task 7에서 값을 넣은 뒤 이 테스트가 실제 가드가 된다.)

- [ ] **Step 3: 상태와 함수를 선언한다**

`PaintWeaponComponent.h`의 `protected:` 블록 안 `OnRep_PaintId` 선언 옆에:

```cpp
	UFUNCTION()
	void OnRep_Charging();
```

`private:` 블록의 RPC 선언들 옆에:

```cpp
	/**
	 * The owner's hold, relayed so the other machines can show it. The owner never receives its
	 * own copy: it drove the FX from its own press, and a late echo would restart the loop.
	 */
	UFUNCTION(Server, Reliable)
	void ServerSetCharging(bool bNewCharging);
```

`private:` 블록의 상태 변수들 옆에:

```cpp
	/** True while a Charged trigger is held. Replicated for the machines that only watch. */
	UPROPERTY(ReplicatedUsing = OnRep_Charging)
	bool bCharging = false;

	/** The hold FX loops, so it has to be switched off by hand rather than expiring. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ChargeFXComponent;
```

같은 `private:` 블록의 함수 선언들 옆에:

```cpp
	/** Records the hold locally, relays it to the machines that only watch, and drives the FX here. */
	void SetCharging(bool bNewCharging);
	void StartChargeFX();
	void StopChargeFX();
```

- [ ] **Step 4: 복제를 등록한다**

`GetLifetimeReplicatedProps`에 한 줄 추가:

```cpp
	// The owner started its own loop from its own press; sending it back would only restart it late.
	DOREPLIFETIME_CONDITION(UPaintWeaponComponent, bCharging, COND_SkipOwner);
```

- [ ] **Step 5: 구현한다**

`PaintWeaponComponent.cpp`의 `GetChargeFraction` 정의 아래:

```cpp
void UPaintWeaponComponent::SetCharging(bool bNewCharging)
{
	if (HasAuthority())
	{
		bCharging = bNewCharging;
	}
	else
	{
		ServerSetCharging(bNewCharging);
	}

	// The machine that set the value never gets its own OnRep, and a dedicated server draws nothing.
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bNewCharging)
		{
			StartChargeFX();
		}
		else
		{
			StopChargeFX();
		}
	}
}

void UPaintWeaponComponent::ServerSetCharging_Implementation(bool bNewCharging)
{
	// The server never saw the press, so it takes the owner's word - but only for a weapon that
	// charges at all, and only while the trigger is allowed. A shot is refused here the same way.
	if (bNewCharging && (!Profile || Profile->FireMode != EPaintFireMode::Charged || IsTriggerBlocked()))
	{
		return;
	}

	bCharging = bNewCharging;

	// A listen server renders this pawn too, and OnRep never fires on the machine that assigned.
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bNewCharging)
		{
			StartChargeFX();
		}
		else
		{
			StopChargeFX();
		}
	}
}

void UPaintWeaponComponent::OnRep_Charging()
{
	if (bCharging)
	{
		StartChargeFX();
	}
	else
	{
		StopChargeFX();
	}
}

void UPaintWeaponComponent::StartChargeFX()
{
	if (ChargeFXComponent || !Profile || !Profile->ChargeFX || !GetWorld())
	{
		return;
	}

	const FVector Scale(Profile->ChargeFXScale);
	if (USkeletalMeshComponent* const Mesh = GetMuzzleMesh())
	{
		ChargeFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Profile->ChargeFX, Mesh, MuzzleSocketName, FVector::ZeroVector, FRotator::ZeroRotator,
			Scale, EAttachLocation::SnapToTarget,
			// Deactivate leaves the last particles to finish and then cleans itself up; false would
			// pile a dead component on the mesh for every charge.
			/*bAutoDestroy=*/true, ENCPoolMethod::None);
		return;
	}

	const FTransform Muzzle = GetMuzzleTransform();
	ChargeFXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), Profile->ChargeFX, Muzzle.GetLocation(), Muzzle.Rotator(), Scale);
}

void UPaintWeaponComponent::StopChargeFX()
{
	if (ChargeFXComponent)
	{
		ChargeFXComponent->Deactivate();
		ChargeFXComponent = nullptr;
	}
}
```

- [ ] **Step 6: 트리거에 연결한다**

`PullTrigger`의 `Charged` 분기:

```cpp
	case EPaintFireMode::Charged:
		PressTime = GetWorld()->GetTimeSeconds();
		SetCharging(true);
		break;
```

`CancelTrigger`에서 `bTriggerHeld = false;` 바로 뒤(`ReleaseTrigger`는 발사 후 이 함수를 지나므로
발사 순간 루프가 꺼지는 것도 여기서 끝난다):

```cpp
	SetCharging(false);
```

`EndPlay`는 `CancelTrigger`를 먼저 부르지만, 트리거를 놓은 채 파괴되는 경우(사망, 무기 해제)에는
`CancelTrigger`가 곧바로 반환한다. 남은 루프를 확실히 끄기 위해 `EndPlay`를 이렇게 바꾼다:

```cpp
void UPaintWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelTrigger();
	StopChargeFX();
	Super::EndPlay(Reason);
}
```

- [ ] **Step 7: 빌드와 전체 테스트 확인**

Run: 테스트 명령. Expected: 빌드 성공, `MintChoco` 전체 `Result=Success`

- [ ] **Step 8: 커밋**

```bash
git add Source/MintChoco/Weapons/PaintWeaponComponent.h Source/MintChoco/Weapons/PaintWeaponComponent.cpp Source/MintChoco/Tests/PaintProfileAssetTest.cpp
git commit -m "feat: 차지샷 충전 루프 이펙트를 복제해 모든 머신에 보이게"
```

---

### Task 6: 이펙트 에셋 5종을 꿀풍선 룩으로 복제

팩 원본은 건드리지 않는다. 복제본의 재질만 꿀풍선 MI로 갈아끼우고, 색을 꿀풍선과 같은 값으로 맞춘다.

**대상과 새 이름:**

| 원본 | 복제본 | 렌더러 | 현재 재질 → 바꿀 재질 |
|---|---|---|---|
| `NS_Liquid_Flip_12` | `NS_ChargeHold` | 스프라이트 2 | `MI_Sprite_Inst_5` → `MI_HoneyBalloonBurst_A`, `MI_Sprite_Inst` → `MI_HoneyBalloonBurst_B` |
| `NS_Liquid_Flip_4` | `NS_ChargeFire` | 스프라이트 1 | `MI_Sprite_Inst_4` → `MI_HoneyBalloonBurst_A` |
| `NS_Liquid_Flip_2` | `NS_ShotgunFire` | 스프라이트 1 | `MI_Sprite_Inst` → `MI_HoneyBalloonBurst_B` |
| `NS_Liquid_Flip_8` | `NS_HeroLandingRise` | 메시 1 | `OverrideMaterials[0]` `MI_Sprite_Inst_7` → `MI_HoneyBalloonBurst_A` |
| `NS_Liquid_Flip_9` | `NS_HeroLandingImpact` | 메시 2 | `MI_Sprite_Inst_7` → `MI_HoneyBalloonBurst_A`, `MI_Sprite_Inst_8` → `MI_HoneyBalloonBurst_B` |

**Files:**
- Create: `Content/Assets/Paint/Niagara/NS_ChargeHold.uasset`, `NS_ChargeFire.uasset`, `NS_ShotgunFire.uasset`, `NS_HeroLandingRise.uasset`, `NS_HeroLandingImpact.uasset`
- Test: Task 7의 에셋 테스트가 참조 무결성을 지킨다(여기서는 눈으로 확인)

**Interfaces:**
- Consumes: 없음
- Produces: 위 5개 에셋 경로. Task 7이 데이터 에셋에서 이 경로를 가리킨다.

- [ ] **Step 1: 에디터를 MCP와 함께 띄운다**

```bash
"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" D:/GitHub/MintChoco/MintChoco.uproject -skipcompile -ExecCmds="ModelContextProtocol.StartServer"
```

준비 신호: `netstat -an | grep 8000`에 LISTENING.

- [ ] **Step 2: 5개를 복제한다**

`AssetTools.duplicate`로 원본 → `/Game/Assets/Paint/Niagara/<새 이름>` 5회. 복제 후
`AssetTools.exists`로 5개 모두 확인(PIE는 꺼진 상태여야 한다).

- [ ] **Step 3: 재질을 갈아끼운다**

스프라이트 렌더러는 `Material` 프로퍼티 하나, 메시 렌더러는 `OverrideMaterials[0].explicitMat`이다.
`NiagaraToolset_System.SetRendererData`를 이미터·렌더러 인덱스별로 한 번씩 호출한다
(`rendererRef` = `{system, emitterName, scriptName:"", moduleName:"", rendererIndex, inputNameStack:[]}` —
읽을 때 쓴 것과 같은 모양). `OverrideMaterials`는 길이 1을 유지하는 제자리 수정이므로 한 번에 써도 안전하다.
`bOverrideMaterials`는 이미 true다.

쓰기 전에 같은 `rendererRef`로 `GetRendererData`를 읽어 현재 값을 확인하고, 쓴 뒤 다시 읽어 새 경로가
들어갔는지 본다.

- [ ] **Step 4: 플립북 프레임 수를 맞춘다**

꿀풍선 MI는 3×5(15프레임)이고, `MI_Sprite_Inst`를 쓰던 이미터는 3×4(12프레임) 기준으로 SubUV
인덱스를 굴리고 있었다. 재질만 바꾸면 마지막 세 프레임이 나오지 않거나 잘린다. 각 이미터의 SubUV
애니메이션 모듈(파티클 업데이트의 `SubUVAnimation` 계열) 프레임 수 입력을 15로 맞춘다:
`NiagaraToolset_System.GetEmitterSummary`로 모듈 이름을 찾고, `GetStackInputData`로 현재 값을 읽고,
`SetStackInputData`로 15를 쓴다. `MI_Sprite_Inst_4`/`_5`/`_7`/`_8`을 쓰던 이미터는 이미 3×5이므로
건드리지 않는다(값을 읽어 확인만 한다).

- [ ] **Step 5: 색을 꿀풍선과 맞춘다**

꿀풍선의 색은 시스템 사용자 변수 `User.TintColor` 기본값 (1, 0.85, 0, 1)이다. 복제본 5개에는 사용자
변수가 없으므로, 각 이미터의 색 모듈(파티클 스폰의 `Color` 계열) 기본값을 같은 값으로 쓴다
(`SetStackInputData`). 팀 색 연동은 스펙 범위 밖이므로 사용자 변수를 새로 만들지 않는다.

이 단계가 MCP 쓰기로 막히면(모듈 입력이 다이내믹 입력 체인이라 값 하나로 안 써지는 경우) **개발자
요청**으로 남긴다: "나이아가라 에디터에서 NS_* 5개의 각 이미터 Initialize Particle → Color를
(1, 0.85, 0, 1)로 설정하고 저장" — 이쪽이 한 번의 UI 편집으로 끝난다.

- [ ] **Step 6: 저장하고 확인한다**

PIE를 멈춘 상태에서 `AssetTools.save_assets`. 그 다음 `LogsToolset`으로 로그를 읽어
`LogMaterial: Warning: [AssetLog]`가 없는지 본다. 나이아가라 시스템에 컴파일 오류가 남아 있으면
`GetSystemCompileState`가 알려준다.

- [ ] **Step 7: 커밋**

```bash
git add Content/Assets/Paint/Niagara
git commit -m "feat: 차지샷·샷건·히어로 랜딩 이펙트 에셋을 꿀풍선 룩으로 복제"
```

---

### Task 7: 데이터 에셋 연결

코드가 읽는 값을 실제 무기·아이템 에셋에 넣는다. **모듈 재빌드 + 에디터 재시작 이후**에 한다.
새 `UPROPERTY`는 그 전까지 `ObjectTools`에 보이지 않는다.

**Files:**
- Modify: `Content/Blueprints/Weapons/Profiles/DA_Weapon_Sniper.uasset`
- Modify: `Content/Blueprints/Weapons/Profiles/DA_Weapon_Shotgun.uasset`
- Modify: `Content/Blueprints/Items/DA_Item_HeroLanding.uasset`
- Test: `Source/MintChoco/Tests/PaintProfileAssetTest.cpp` (총구 연출 가드 추가)

**Interfaces:**
- Consumes: Task 1~6의 필드와 에셋 경로 전부
- Produces: 실행 가능한 최종 동작

넣을 값:

| 에셋 | 프로퍼티 | 값 |
|---|---|---|
| `DA_Weapon_Sniper` | `MinChargeToFire` | `0.3` |
| `DA_Weapon_Sniper` | `ChargeFX` | `/Game/Assets/Paint/Niagara/NS_ChargeHold.NS_ChargeHold` |
| `DA_Weapon_Sniper` | `ChargeFXScale` | `1.0` |
| `DA_Weapon_Sniper` | `MuzzleFX` | `/Game/Assets/Paint/Niagara/NS_ChargeFire.NS_ChargeFire` |
| `DA_Weapon_Sniper` | `MuzzleFXScale` | `1.0` |
| `DA_Weapon_Sniper` | `MuzzleFXChargeScale` | `{"X": 0.5, "Y": 1.5}` |
| `DA_Weapon_Shotgun` | `MuzzleFX` | `/Game/Assets/Paint/Niagara/NS_ShotgunFire.NS_ShotgunFire` |
| `DA_Weapon_Shotgun` | `MuzzleFXScale` | `1.0` |
| `DA_Item_HeroLanding` | `ActivateFX` | `/Game/Assets/Paint/Niagara/NS_HeroLandingRise.NS_HeroLandingRise` |
| `DA_Item_HeroLanding` | `ActivateFXScale` | `1.5` |
| `DA_Item_HeroLanding` | `Burst` | `{"BurstFX": {"refPath": "/Game/Assets/Paint/Niagara/NS_HeroLandingImpact.NS_HeroLandingImpact"}, "BurstFXScale": 1.5}` |

- [ ] **Step 1: 실패하는 테스트를 쓴다**

`PaintProfileAssetTest.cpp`의 `#endif` 앞에 추가:

```cpp
/**
 * 총구 연출을 넣은 프로필의 크기 범위가 뒤집혀 있거나 0이면, 발사는 되는데 아무것도 보이지 않는다.
 * 그 조합만 막는다: 연출을 넣지 않은 프로필은 그대로 통과한다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintMuzzleFXProfileTest,
	"MintChoco.Paint.Weapons.MuzzleFXProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPaintMuzzleFXProfileTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ProfileFolder}, /*bForceRescan=*/true);

	TArray<FAssetData> Profiles;
	FindProfileAssets(Registry, UPaintWeaponProfile::StaticClass(), Profiles);

	for (const FAssetData& Asset : Profiles)
	{
		const UPaintWeaponProfile* const Profile = Cast<UPaintWeaponProfile>(Asset.GetAsset());
		if (!Profile || !Profile->MuzzleFX)
		{
			continue;
		}
		const FString Name = Asset.AssetName.ToString();
		TestTrue(*FString::Printf(TEXT("%s: MuzzleFXScale is positive"), *Name), Profile->MuzzleFXScale > 0.0f);
		TestTrue(
			*FString::Printf(TEXT("%s: MuzzleFXChargeScale ends are positive"), *Name),
			Profile->MuzzleFXChargeScale.X > 0.0 && Profile->MuzzleFXChargeScale.Y > 0.0);
		TestTrue(
			*FString::Printf(TEXT("%s: MuzzleFXChargeScale runs from small to large"), *Name),
			Profile->MuzzleFXChargeScale.X <= Profile->MuzzleFXChargeScale.Y);
	}

	// 스나이퍼는 부분 충전으로도 발사돼야 크기 범위가 실제로 보인다.
	const UPaintWeaponProfile* const Sniper = LoadObject<UPaintWeaponProfile>(
		nullptr, TEXT("/Game/Blueprints/Weapons/Profiles/DA_Weapon_Sniper.DA_Weapon_Sniper"));
	if (TestNotNull(TEXT("DA_Weapon_Sniper loads"), Sniper))
	{
		TestNotNull(TEXT("DA_Weapon_Sniper: MuzzleFX"), Sniper->MuzzleFX.Get());
		TestNotNull(TEXT("DA_Weapon_Sniper: ChargeFX"), Sniper->ChargeFX.Get());
		TestTrue(TEXT("DA_Weapon_Sniper: a partial charge can fire"), Sniper->MinChargeToFire < 1.0f);
	}

	const UPaintWeaponProfile* const Shotgun = LoadObject<UPaintWeaponProfile>(
		nullptr, TEXT("/Game/Blueprints/Weapons/Profiles/DA_Weapon_Shotgun.DA_Weapon_Shotgun"));
	if (TestNotNull(TEXT("DA_Weapon_Shotgun loads"), Shotgun))
	{
		TestNotNull(TEXT("DA_Weapon_Shotgun: MuzzleFX"), Shotgun->MuzzleFX.Get());
	}
	return true;
}
```

- [ ] **Step 2: 테스트가 실패하는 것을 확인한다**

Run: 테스트 명령. Expected: `DA_Weapon_Sniper: MuzzleFX` 가 null로 실패, `a partial charge can fire` 실패.

- [ ] **Step 3: 에디터를 재시작하고 값을 쓴다**

에디터를 MCP와 함께 다시 띄운 뒤(Task 6 Step 1의 명령), `ObjectTools.set_properties`로 위 표의 값을
넣는다. `values`는 JSON **문자열**이고, 에셋 참조는 전체 오브젝트 경로다. `Burst`는 중첩
USTRUCT이므로 한 번의 호출로 두 멤버를 함께 쓴다. 쓴 뒤 `get_properties`로 다시 읽어 확인하는데,
중첩 구조체는 멤버 이름이 camelCase(`burstFX`)로 돌아오므로 키가 아니라 값으로 비교한다.

- [ ] **Step 4: 저장한다**

PIE가 꺼진 상태에서 `AssetTools.save_assets`로 세 에셋을 저장.

- [ ] **Step 5: 테스트 통과 확인**

Run: 테스트 명령(에디터를 먼저 닫는다 — 포트 8000을 두고 다투지 않게).
Expected: `MintChoco.Paint.Weapons.MuzzleFXProfiles`, `...ChargeFXProfiles` 모두 `Result=Success`

- [ ] **Step 6: 커밋**

```bash
git add Content/Blueprints/Weapons/Profiles/DA_Weapon_Sniper.uasset Content/Blueprints/Weapons/Profiles/DA_Weapon_Shotgun.uasset Content/Blueprints/Items/DA_Item_HeroLanding.uasset Source/MintChoco/Tests/PaintProfileAssetTest.cpp
git commit -m "feat: 무기·아이템 에셋에 새 이펙트 연결하고 최소 충전 0.3으로"
```

---

### Task 8: PIE 검증

네트워크 3경로와 눈으로 보는 크기는 자동화 테스트가 대신할 수 없다. 리슨 서버 + 클라이언트 1로 본다.

**Files:** 없음(확인만)

**Interfaces:**
- Consumes: Task 1~7 전부
- Produces: 검증 결과

- [ ] **Step 1: PIE를 띄운다**

에디터에서 Play, Net Mode = Play As Listen Server, 플레이어 수 2. 두 창을 나란히 둔다.

- [ ] **Step 2: 충전 루프를 확인한다**

호스트가 스나이퍼를 들고 트리거를 누른다. 확인: 호스트 화면에서 총구에 루프가 켜지고, **클라이언트
화면의 그 캐릭터 총구에도** 켜진다. 발사하는 순간 양쪽에서 꺼진다. 쏘지 않고 트리거를 취소해도 꺼진다.
클라이언트가 충전할 때도 양방향으로 같은지 본다(이쪽이 `ServerSetCharging` 경로다).

- [ ] **Step 3: 충전량 크기를 확인한다**

30%쯤에서 발사한 것과 풀충전으로 발사한 것의 총구 연출 크기를 비교한다. 대략 세 배 차이(0.5 대 1.5)여야 한다.
양쪽 화면에서 같은 크기로 보이는지도 본다(원격 머신은 `FPaintShot::Charge`로 계산한다).

- [ ] **Step 4: 샷건을 확인한다**

샷건으로 연사한다. 발사마다 총구에서 한 번, 두 화면 모두. 겹쳐서 두 번 나오면 `PlayMuzzleFX` 호출
지점이 중복된 것이다.

- [ ] **Step 5: 히어로 랜딩을 확인한다**

히어로 랜딩 아이템을 쓴다. 올라가기 시작할 때 발 밑에서 `NS_HeroLandingRise`가 1.5배로, 착지할 때
착지 지점에서 `NS_HeroLandingImpact`가 1.5배로. 두 화면 모두에서.

- [ ] **Step 6: 로그를 확인한다**

PIE를 멈추고 `Saved/Logs/MintChoco.log`에서 `LogNiagara.*Warning`, `LogMaterial.*Warning`, `LogMintChoco.*Warning`을
훑는다. 에디터가 죽었다면 작업을 멈추고 `Saved/Crashes/*/MintChoco.log`의 `Assertion failed` 줄과 그 앞
몇 줄을 보고한다.

- [ ] **Step 7: 결과를 기록한다**

여섯 개 확인 항목의 결과를 한 줄씩 남긴다. 크기나 색이 마음에 들지 않는 것은 버그가 아니라 튜닝이며,
`MuzzleFXScale`·`ActivateFXScale`·`BurstFXScale`·`MuzzleFXChargeScale`로 조정한다. 코드는 건드리지 않는다.

---

## 미해결 사항 / 위험

- **플립북 프레임 수**(Task 6 Step 4): 꿀풍선 MI는 3×5, 일부 대상 이미터는 3×4 기준이다. 프레임 수를
  맞추지 않으면 애니메이션이 잘린다. 실제로 어느 모듈이 인덱스를 굴리는지는 에디터에서 확인해야 한다.
- **색 모듈 쓰기**(Task 6 Step 5): 색이 다이내믹 입력 체인(커브, 랜덤 범위)으로 들어가 있으면 MCP로 값
  하나를 쓸 수 없다. 그때는 개발자 UI 편집 요청으로 돌린다.
- **`MinChargeToFire` 0.3의 게임 밸런스**: 부분 충전 발사가 열리면 스턴 시간도 충전량에 비례해 짧게 들어간다
  (기존 `FPaintFireContext::ChargeFraction` 로직). 연출만 보려던 변경이 밸런스에 닿는다는 점을 디자이너가 이미 알고 결정했다.
- **총구 소켓**: `MuzzleSocketName` 기본값은 `hand_r`이다. 실제 총구가 아니라 손이므로 연출 위치가
  손에 붙는다. 더 정확한 소켓이 스켈레톤에 있으면 컴포넌트의 `MuzzleSocketName`을 바꾸는 것이
  이 계획 밖의 한 줄짜리 개선이다.
