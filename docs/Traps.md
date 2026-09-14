# Symptom → check first

Each row is a diagnosis that took real time once. Find the symptom, check the right column
before anything else.

## Paint pipeline facts

- Paint no longer needs UV1. The paint buffer is a procedural planar atlas: one island per
  enabled local direction (`bPaintUp` and friends on `UPaintableComponent`, Up by default,
  `bFloorFollowsWorldUp` adds the sky-facing side), packed by `FPaintIslandLayout` from the mesh
  bounds and the actor scale, filled by `PaintAtlasBaker` on the CPU from LOD 0 (Allow CPU
  Access is the only mesh requirement; a Nanite mesh bakes from its fallback).
  `MF_PaintOverlay` picks the island from the pixel's local normal (`PaintAtlasUV`), so a
  surface only shows paint on directions it keeps; a hit on any other direction is a transient
  splat (`FPaintSplat::bTransient`) that spawns `UPaintSettings::SideSplatEffectClass`. So is a
  hit on a static mesh with no `UPaintableComponent`: `FPaintDeposit::ReceivesSplat` admits a
  paintable or a non-Movable `UStaticMeshComponent`, and every splat source (deposit, stroke,
  sample click) goes through it. Pawns and Movable meshes take nothing, since the decal would
  stay where the surface was.
- Stamps, the cell grid and the brush's `BoundsMin/BoundsSize` are in the scaled-local frame
  (local × |Scale3D|): every length is world cm, non-uniform scale included. A hit normal maps
  to a local direction with `InverseTransformVectorNoScale(N) * Scale3D`.
- In a Custom node, `VertexNormalWS` is the local normal rotated but never scaled; undo the
  rotation alone (`LocalToWorld` rows divided by `InvNonUniformScale`, transposed). The
  `Transform` node World→Local divides by the scale instead and skews it. Feeding a local normal
  to Local→World needs `/ (Scale*Scale)` first (inverse transpose).
- `Begin/EndDrawCanvasToRenderTarget` uses the world's single canvas: never nest or interleave
  two surfaces' pairs. Rectangles drawn into the shared scratch buffer are copied back with
  `TransitionAndCopyTexture`; both targets must share the pixel format
  (`UPaintSubsystem::CreatePaintBuffer`).
- Paint thickness is `DisplacementScaling.Magnitude` in **world cm** (the overlay divides by
  the primitive scale along the normal); keep `PaintMaxHeight` equal to it or shading and
  silhouette disagree. It was 3 while the sample cubes were 3× scaled, so the old look was 9 cm.
- Pixel-frequency carriers through a layer stack: Anisotropy (1), Refraction (2, Break exposes
  RG only), PixelDepthOffset (1), Opacity (1), Tangent (3) — every look layer must pass each one
  through Break → Make. In use: Anisotropy / Refraction.rg / PixelDepthOffset = per-team signed
  distances, Opacity = consumed coverage `S`, EmissiveColor.r = thin-paint opacity (slab
  Emissive is deliberately unwired), Tangent = per-look `(SecondRoughness, weight, WetCoat)`
  written by the looks rather than passed through. `CustomizedUVs` (and WPO) in a
  MaterialAttributes struct are **vertex-frequency** even when written by Make and read back
  through Break inside a pixel chain. Parameters inside a Material Layer/Blend are namespaced
  per slot, so C++ cannot set them by name — feed runtime data through the stack `Input`.

### Nanite tessellation displacement (UE 5.8)

- Substrate: a slab stacked on top through `SubstrateVerticalLayering` may only use the
  SimpleVolume SSS type; Diffusion SSS survives only on the bottom slab. So paint stays the
  bottom slab and the wet coat is the top: `VL(Top = Weight(coat, Tangent.B), Bottom = body)`.
  The stack carries per-look `(SecondRoughness, SecondRoughnessWeight, WetCoat)` on **Tangent**
  (pixel-frequency, lerped by `BlendMaterialAttributes` in `MF_PaintTeamBlend`); a look that
  leaves `Make.Tangent` unwired leaks the vertex tangent into those slab inputs.
- Tessellation is **on by default** (`r.Nanite.AllowTessellation` no longer exists;
  `r.Nanite.Tessellation`, `ProgrammableRaster`, `ComputeRasterization` all default 1;
  `r.Nanite.DicingRate` is the density knob). The material needs `bEnableTessellation=true`,
  and `DisplacementScaling.Magnitude` is baked into proxy bounds — set it once, never animate it.
- The Displacement pin compiles at pixel frequency with the full material evaluated per
  micro-vertex every frame, so sampling a runtime render target through a Custom node is fine.
  It **does not** recompute shading normals, and it displaces along the **vertex normal**: two
  faces meeting at a hard edge move apart and tear.

## Rendering

| Symptom | Check first |
|---|---|
| Find Collision UV always returns (0,0) | Physics → Support UV From Hit Results enabled and editor restarted? Trace Complex + Return Face Index on the trace? |
| UV misaligned on Nanite meshes | Nanite collides against the fallback mesh. Use a separate simple collision mesh, or disable Nanite for the prototype. |
| Splats overwrite instead of accumulate | Is the brush Blend Mode Translucent? Is Clear Render Target called every draw? |
| Splats add instead of overwriting | `DrawMaterialToRenderTarget` can never write the RT's alpha: every translucent blend mode uses `BF_Zero` for source alpha (`TranslucentRendering.cpp`). Encode coverage without alpha (paint-id buffer). |
| A fourth per-texel channel is needed (the speed-star generation) | The A channel is dead either way: the opaque base pass writes `MRT[0].a = 0` (`BasePassPixelShader.usf`) with a full-RGBA blend state. The id only uses 3 bits of R, so the generation rides in R's upper 5 bits (`EncodePaintTexel`, `byte = id + 8·gen`); every reader decodes with `fmod(round(r·255), 8)`. |
| A Masked brush paints the whole RT | `r.EarlyZPassOnlyMaterialMasking` defaults to 1, so `clip()` is compiled out of the base pass (`BasePassPixelShader.usf`) and the canvas path has no depth prepass to mask instead. Use Opaque and preserve old contents by reading the surface's own buffer while drawing into the shared scratch buffer, then copy the stamp rectangles back. |
| Frame drop when drawing several splats in one frame | A full-target draw per splat scales with the atlas, not the stamp. Draw only the stamp's rectangles (`BuildStampRects`, one per island it reaches) and copy them; keep one Begin/End per splat because the next splat reads the copy. |
| Paint appears on the wrong face, or not at all, after a hit | The surface keeps only its enabled directions; the hit's local direction (dominant axis of the unscaled local normal) decides. Check `keeps paint on ...` in LogPaint, the six `bPaint*` flags and `bFloorFollowsWorldUp`. Exactly 45° faces are a coin flip between two islands. |
| A shelf or the underside of a bridge shows the paint of the surface above it | Planar projection: an island keeps only the outermost surface along its axis, so stacked same-facing surfaces in one mesh share texels. Split them into separate actors. |
| Big cubes look blobby, small ones fine | Displacement was local units: `Magnitude × scale`. Now it is world cm; if it still scales, the overlay's `PaintDisplaceScale` node or `MF_PaintNormal`'s `localPerWorld` term is unwired. |
| Coverage calculation is slow | Read Render Target Raw Pixel is a synchronous GPU wait. Use the cell grid instead. |
| Paint looks flat from the side | Normal/POM cannot change the silhouette. Is WPO or Displacement actually connected, and is the mesh tessellated enough? |
| A hitscan or aim trace passes straight through units | In 5.8 the `Pawn` capsule profile **and** the `CharacterMesh` profile ignore `ECC_Visibility` (BaseEngine.ini). Trace on `PaintballChannel` (`ECC_GameTraceChannel1`, `Weapons/PaintProjectile.h`): pawns and paintable meshes block it, balls in flight ignore it, so the ray hits what a paintball would. The sniper does this; the gun's aim trace still uses Visibility and converges through pawns. |
| Splats cut off at actor boundaries | The stamp is world-space and every overlapping surface (sphere overlap in `UPaintSubsystem::ApplySplat`) draws its own part; a missing half means that actor has no `UPaintableComponent`, keeps no direction facing the hit, or its collision does not answer `ECC_Visibility`. |
| A side splat stains the floor at the wall's foot in a hard-edged band, or paints a unit standing next to the wall | The side splat is a deferred decal and lands on every pixel inside its box, whatever the actor; a box as deep as the stamp radius extrudes the blob into a band on any surface perpendicular to the wall. Keep the box a thin slab (`ProjectionHalfDepth` in `APaintSideSplat::OnPaintSplat_Implementation`), keep `M_PaintSideSplat`'s `NormalFade` on Coverage (receiver normal from `cross(ddx(wp), ddy(wp))` of `GetTranslatedWorldPosition(Parameters)`, compared with `ObjectOrientation`, which in a decal material is the decal's projection axis), and keep `bReceivesDecals` off on unit primitives (`AUnit::PostInitializeComponents`). |
| Colors differ between clients | Overlap-order differences are expected and allowed. A missing splat means the Unreliable Multicast dropped it — also check that local prediction and the server event are not drawn twice. |
| A texture set via SetTextureParameterValue reaches one sample node but not a Custom node | A TextureSampleParameter2D and a TextureObjectParameter sharing one parameter name: the instance override only reaches the sampler one. Give the object parameter its own name and set both from C++. A stale MaterialInstance can also keep failing after a parameter rename — test with a freshly created instance. |
| Paint mask/roughness respond but the relief normal stays flat on some faces | The height gradient is computed in atlas space but MP_Normal is applied in the mesh's UV0-derived tangent frame; per-island orientation makes the result wrong or invisible. Build a world-space normal from position-atlas-derived axes instead of trusting mesh tangents. |
| Displaced paint shows a silhouette but shades flat | Nanite displacement keeps the vertex normal. Derive the normal from the height field: differentiate the position atlas for ∂P/∂u, ∂P/∂v (bounds-normalized local → cm via BoundsSize), then `cross(Pu + Hu·N, Pv + Hv·N)` in local space, `/ Scale²`, `Local→World`, with the material's Tangent Space Normal off. Fall back to the vertex normal across island edges. |
| Displacement is a plateau with vertical cliffs and texel stairs | The height was derived from the binary id coverage read through a point-filtered buffer. Accumulate a soft height in its own channel (RG8: R id, G height) from a soft brush kernel, and bilinear-filter it by hand in the shader — the id sampler must stay TF_Nearest. |
| Mesh tears open along hard edges when painted | Faces sharing a hard edge displace along different vertex normals. Fade the height to 0 over the last texels of each island (`PaintAtlasBaker::ComputeEdgeFade` bakes distance-to-edge, including height steps inside an island, once per atlas). Art with chamfered edges does not tear. |
| Sparkling noise on the displacement slope near edges | The per-texel deposit noise rides on the fade slope and grazes the specular lobe. Damp the derived normal's gradient by the same fade (`PaintNormalStrength 0` makes it vanish, which confirms it). |
| Paint edges look blocky however they are filtered | The brush binarizes the SDF into the id buffer at texel resolution; read-time bilinear only blurs the stairs. Store the brush distance in its own channel (`1 − d/range`, 0 = far, so clears stay valid), build per-team signed distances from the four corner texels, and threshold with `smoothstep(−w, w, sd)`, `w = clamp(0.5·fwidth, …, 0.5)`. |
| A rim outline appears on painted blobs at a distance | `fwidth(sd)` jumps between the clamped ±range samples under minification and smears the background through the whole rim; a swallowed same-team edge also keeps a small stored `d`. Cap `w` at one texel and pin quads whose four corner ids agree to ±range. |
| Background shows through where two teams meet | Sequential layer blends lerp twice. Carry the coverage already consumed (`S`) through the stack and use `alpha = cov / (1 − S)` per blend. |
| Paint reads as a matte sticker with glossy reflections | Flat team colors with a wet roughness. The fix is per-team looks with albedo texture and roughness designed together; for cream/ice cream go Substrate (slab with SSS MFP + fuzz) rather than overwriting attributes. |
| Paint on an art mesh lands in the wrong place | The atlas is projected from the mesh bounds, so the mesh's LOD 0 must be CPU-readable (Allow CPU Access) and its Nanite fallback close to the real surface; a curved art mesh with a decimated fallback shifts by the decimation error. Check `paint atlas baked: N of M texels covered` in LogPaint. |
| Debug cells show in the editor, coverage text does not | `DrawDebugString` rides on a player's HUD, so the text is play-only; cells draw through the line batcher and work in the editor viewport with Realtime on (`bDrawDebugCells`, rebuilt when the actor moves). |
| A plane-cut liquid (ink bottle) looks hollow or cut open from above | The two-sided "backface = surface" trick has no top geometry: from above you see the shaded inner walls below the waterline. `UInkBottleComponent` places a real disc (`SM_InkSurface`, `M_InkSurface`) on the cut plane every tick; the disc material clips outside `BottleRadius` and ripples via WPO with the same wave as the walls. Keep the fill clamped off the end caps (`SurfaceFillMargin`) or the disc z-fights them. |
| The camera boom snags on another player, or a player stays translucent | Units ignore `ECC_Camera` on capsule and mesh (set again in `AUnit::PostInitializeComponents`, so a Blueprint override cannot bring it back). Overlap is detected by `CameraProbe`, a sphere on the local player's camera that overlaps other units' capsules only; the overlapped unit swaps every mesh slot to `CameraFadeMaterial` (`M_UnitCameraFade`, set on `BP_Unit`) until EndOverlap, `UpdateCameraProbe` (unpossess) or the prober's `EndPlay` restores it. |
| Other players animate in slow motion on the listen-server host | The anim blueprint derives speed from per-tick position delta. On the server a remotely controlled pawn only moves when a `ServerMove` arrives (`ClientNetSendMoveDeltaTime` 0.0166 = 60 Hz), while the mesh ticks every frame, so the ticks with no displacement drag the average down. Read `Velocity` off the movement component instead — it holds its value between moves, so it is frame-rate independent. `t.MaxFPS 60` making the symptom vanish confirms it. |

## Items (Gameplay Ability System)

Items live in `Source/MintChoco/Items/`: a `UItemProfile` asset per item (display, pickup mesh,
duration, `AbilityClass`), a `UItemAbility` subclass per item, one `UItemGameplayEffect`
subclass per item (only so stacks stay separate; duration is SetByCaller, the state tag is a
`DynamicGrantedTag`), `UItemSlotComponent` on `AUnit`, `AItemPickup` (announced → active),
`AItemSpawnPoint` markers, and `UItemSettings` (`[/Script/MintChoco.ItemSettings]` in
`DefaultGame.ini`: the item list and pickup class). The game mode spawns one item per
`ItemSpawnInterval` at a free point, `ItemSpawnWarning` seconds after the laser.

| Symptom | Check first |
|---|---|
| Speed Star rubber-bands on a client | The speed must ride the compressed move flag (`FLAG_Custom_1`, `bWantsSpeedBoost`), never a GAS attribute: `GameplayPrediction.h` says GE prediction and movement prediction are not time-correlated, so a GE-driven speed is simulated at the old speed on the server until the activation RPC lands. |
| An item ability ends after one RTT on the client | `WaitGameplayEffectRemoved` on the client's predicted handle fires when the prediction key catches up. The client ends on `WaitDelay(Duration)`; only the server waits for GE removal. |
| Using the same item twice runs two spinners | `bRetriggerInstancedAbility` must be true and the slot must reuse the existing spec (clear `RemoveAfterActivation`) instead of granting a second one. |
| `SetStackingType` link error | It is `WITH_EDITOR`-only and unexported. Write `StackingType` in the constructor under `PRAGMA_DISABLE_DEPRECATION_WARNINGS`. |
| Weapon still fires during the spinner | `UPaintWeaponComponent::TriggerBlockedTags` holds `State.Item.SweetSpinner`; both `PullTrigger` and `ServerFire` ask the owner's ASC. |
| An item should run only for as long as something else lives (a thrown ball, a dome) | Give the profile `Duration` 0: the ability is instant, applies no GE and no state tag, and ends right after `OnItemActivated`. `ItemProfileAssetTest` skips the state-tag check for instant items. |
| A unit keeps walking while stunned on the server only, or only on the client | Both sides must agree through `UUnitMovementComponent`: `GetMaxSpeed` returns 0 and `ConstrainInputAcceleration` zeroes the input while `AUnit::IsMovementInputLocked()` (Stunned or HeroLanding tag, or a hero-landing phase). The input handler alone is not authoritative because the server consumes the client acceleration verbatim. |
| A weapon hit should stun, or stuns the wrong length | `FPaintDeposit::StunDuration` / `StunSuperArmorDuration` on the contact (paintball `Deposit`, sniper `Impact`): `ApplyHit` and the sniper's pawn branch call `StrikeUnit`, which skips same-colour units and calls `AUnit::TryApplyStun(Stun, SuperArmor)`. A Charged profile scales the stun by the release charge (`FPaintFireContext::ChargeFraction`, sent in `ServerFire` as a byte); `MinChargeToFire` below 1 lets a partial charge fire. Items keep `UItemSettings` (2 s / 4 s) through the parameterless `TryApplyStun()`. |
| Knockback rubber-bands or double-launches | Only the server calls `LaunchCharacter` (`AUnit::Knockback`); a client RPC on top races the correction. |
| Hero landing restarts after landing, or the server never gets the landing effect | The rise starts only on a 0→1 edge of `FLAG_Custom_2` (`bHeroLandingArmed`), and a dive is never aborted when the flag drops; `AUnit::Landed` → `FinishHeroLandingDive` decides. Phase state rides in `FSavedMove_Unit`, so replays reproduce it. |
| Friendly pawns bounce off the chocolate dome, or trapped enemies never get released | The pass-through is a *component* ignore (`IgnoreComponentWhenMoving(Wall)`) applied on every machine; an actor-level ignore would also hide the pawn from the dome's Sensor. Enemies inside at spawn are in the replicated `PassThrough` list until the Sensor reports EndOverlap. |
| A bee dies to its own team, or stops dead when a ball touches it | Balls hit the bee's child `Shell` (Paintball Block only); the root sphere ignores Paintball so the projectile movement never gets a blocking hit from a ball. `ReceivePaintHit` ignores the bee's own paint id. |
| The bee zig-zags or bobs while chasing, or dives into the floor | Steer heading and altitude separately (`FBeeSteering::TurnTowardsSplit`): the obstacle probe only looks along the *flat* heading (the floor is never an obstacle), altitude is a proportional term (`VerticalComponent`) capped by ground clearance (`MaxDescent`), and a chosen avoidance direction is kept for 0.3 s. Turning the full 3D direction on the shortest arc swings through straight-down when the heading change is large, and a threshold-based hover push flips sign every tick. Measured in PIE by placing `BP_Bee` in the level (it picks the nearest unit and the settings profile on its own) and sampling `get_actor_transform`. |

## Unit facing (body yaw vs camera)

`AUnit` has `bUseControllerRotationYaw` off. The camera boom follows control rotation on its own
(`bUsePawnControlRotation`), movement input and the shot direction are camera-relative, and the
body yaw is turned by `UUnitMovementComponent::PhysicsRotation`: it runs the engine's
`bUseControllerDesiredRotation` path (`RotationRate.Yaw` 720°/s, constant angular speed) only
while `ShouldFaceControlRotation()` is true, i.e. `Acceleration` is non-zero or
`AUnit::WantsToFaceAim()` (trigger held, aiming, charging, or within `FaceAimHoldSeconds` of the
last `OnFired`), and never while input is locked. Idle look-around leaves the body alone.

| Symptom | Check first |
|---|---|
| The body snaps to the camera every frame, even idle | `bUseControllerRotationYaw` must stay false on `AUnit` and must not be overridden in `BP_Unit`; the boom, not the pawn, carries the control rotation. |
| The body never turns, or turns only on one machine | The gate must only read what both sides have: the move's `Acceleration` and control rotation, and weapon state the server also holds (`bCharging` is replicated, `OnFired` fires on the server in `ServerFire`). `bTriggerHeld` / `bAiming` are owner-only and are just an early yes there. `LastFireTime` is written before the dedicated-server early return in `HandleWeaponFired`. |
| A remote pawn's body lags or pops | Simulated proxies never run `PhysicsRotation`; their yaw is the replicated rotation smoothed by the engine. Tune on the owner and the listen host, not the proxy. |
| Tuning the turn | `RotationRate.Yaw` on the movement component (720 = 180° in 0.25 s) and `AUnit::FaceAimHoldSeconds` (0.5, keep equal to the anim `FireHoldTime` so the gun pose and the body release together). Test: `MintChoco.Game.Facing.*`. |

## HUD (crosshair, charge ring, paint bar)

Game HUD visuals are drawn procedurally in `NativePaint` (`FSlateDrawElement::MakeLines` /
`MakeBox` with a `RoundedBox` brush), never from textures: `UPaintChargeWidget`,
`UPaintBarWidget`, the crosshairs. `WBP_GameHUD` (parent `UGameHudWidget`, created by
`BP_GamePlayerController`) hosts them; a widget the Blueprint does not place,
`UGameHudWidget::NativeConstruct` adds to the root canvas full-screen.

Crosshairs: `UPaintCrosshairHostWidget` picks the active weapon every tick (the secondary while
its trigger is held or it is aiming, else the primary), resolves `UPaintWeaponProfile::
CrosshairClass` (unset → the host's default, `UPaintBracketCrosshairWidget`), keeps one instance
per class and cross-fades between them. `UPaintCrosshairWidget` (abstract) owns the shared state
(firing blend, impact marker, presence fade) and draws the marker; a subclass draws its look in
`PaintReticle`. `UPaintScopeCrosshairWidget` is the charged secondary's (set on `DA_Weapon_Sniper`).

| Symptom | Check first |
|---|---|
| The crosshair's impact marker sits in the wrong place, or drifts with window size | The host and every crosshair assume a full-screen canvas slot (anchors 0,0-1,1, offsets 0): the centre is `LocalSize / 2` and `UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition` is used as a local coordinate. Placed in a smaller slot, both are off. |
| A weapon shows the wrong crosshair, or the secondary's never appears | `CrosshairClass` on the profile asset; an abstract class is refused (warning once, then nothing). The secondary's shows only while `IsTriggerHeld() \|\| IsAiming()` on it - a Charged profile sets aiming from press to release. |
| A crosshair keeps ticking or reacting after it was swapped out | The host collapses an instance only once `IsFadedOut()`; a collapsed widget does not tick, so the host re-shows it before `SetShown(true)`. Do not toggle visibility on the crosshair yourself. |
| No impact marker on a weapon | Only `UPaintGunProfile` implements `PredictImpact` (centre pellet, no spread, both gravity phases, `PaintballLifeSpanSeconds`). Hitscan and stroke land on the crosshair, so they return false on purpose. The marker also needs the impact to fall short of the aim point along the view by `MarkerShortfallCm` (50): a wall hit dead-on shows none. |
| The marker jitters at range while running | The muzzle socket rides the animation; `MarkerInterpSpeed` smooths the screen position. Raise `MarkerShortfallCm` if it flickers at the threshold. |
| Checking the look without playing | `mc.CrosshairPreview 0/1/2` (idle / firing / marker at a fixed offset), `mc.ChargeRingPreview 0..1`. Negative restores the real state. |

## Paint hit receivers (balloon)

`FPaintDeposit::HitPower` is the game-balance number a contact carries (sniper `Impact` 100,
`DA_Paintball_Light` 3, Standard 10, Heavy 25, brush 2). `FPaintDeposit::ApplyHit` strikes any
hit actor implementing `IPaintHitReceiver` (`Weapons/PaintHitReceiver.h`) before the surface
test, so every weapon reaches it with one hook and the receiver need not be paintable.
`ABalloon` (`Game/Balloon.h`, `BP_Balloon`) is the first receiver: 100 health, inflates with
damage, the last team to hit it owns the pop, `MulticastBurst(Seed, PaintId)` launches
`BurstCount` balls of `BurstPaintball` (real on the server, cosmetic on clients), and it
re-inflates after `RespawnDelay`. Its collision blocks only the `Paintball` channel, so pawns
and the camera pass through.

## Match flow (game maps)

`AGameGameMode::StartPlay` no longer starts the clock. `AGameGameState::MatchPhase`
(replicated) runs WaitingForPlayers → Countdown → Playing → Ended. Each `AGamePlayerState`
polls on its owning machine until it has a pawn and the screen fade is clear, then sets
`bReady` (server RPC); the mode starts the countdown when every non-spectator PlayerState is
ready and has a pawn, or after `ReadyTimeout`. `StartMatch` starts item spawning and the match
timer. Countdown and end are server timestamps (`GetCountdownRemaining`, `GetRemainingTime`);
before the match `GetRemainingTime` returns the full `MatchDuration`. Movement and both
triggers are locked until Playing (`AGameGameState::IsPlayerInputAllowed`, checked in
`AUnit::IsMovementInputLocked` and `UPaintWeaponComponent::IsTriggerBlocked`; a world without
`AGameGameState` is always allowed). `UGameHudWidget` (parent of `WBP_GameHUD`) drives
`Txt_Timer` (red under `TimerWarningSeconds`) and `Txt_Countdown` (waiting text, 3-2-1, START!,
last `FinalCountdownSeconds`).

| Symptom | Check first |
|---|---|
| The countdown never starts | A PlayerState never reported ready: it needs a local PlayerController, a valid pawn, and `UScreenFadeSubsystem::IsCovered()` false. `LogMintChoco` prints `준비 완료` per player and a warning when `ReadyTimeout` fires. |
| Players can move during the countdown | The phase is read from the world's `AGameGameState`; a map using another GameState class locks nothing. |

## Screen fade (map transitions)

`UScreenFadeSubsystem` (`Source/MintChoco/Screen/`) owns the cover widget across maps. Every
travel goes through `TravelWithFade` / `ServerTravelWithFade` / `ClientTravelWithFade` (the
lobby Blueprint and `UOnlineSessionsSubsystem` already do); a server travel spawns
`AScreenFadeRelay` whose multicast darkens every client first. The engine's
`PreLoadMapWithContext` pins the cover, `PostLoadMapWithWorld` starts the ready wait, and the
map opens when the local controller has a PlayerState, a game map (`AGameGameState`) has the
local pawn, and no hold is registered (`AddHold(Owner, Reason)`; `UPaintSubsystem` holds while
an atlas bakes). `MaxReadyWait` opens it regardless. Settings:
`[/Script/MintChoco.ScreenFadeSettings]`, widget `WBP_LoadingScreen`.

| Symptom | Check first |
|---|---|
| The listen-server PIE window never shows the cover on start | Expected: the first PIE world is duplicated from the editor world, so `PreLoadMap`/`PostLoadMap` never fire for it. Clients and seamless travels do fire them; a packaged game always does. Grep `가림막:` in the log. |
| Screen stays black for `MaxReadyWait` then opens with a warning | A hold never released or the pawn never arrived; the warning lists the remaining holds. |
| Travel happens without a fade | Something called `ServerTravel`/`ClientTravel` directly instead of the subsystem. |

## OnlineSubsystem / Steam sessions

| Symptom | Check first |
|---|---|
| Creating a room in one PIE window makes every other PIE window create/travel too | `IOnlineSubsystem::Get()` without a world returns the one shared instance, so every PIE GameInstance subsystem registers on the same `OnCreateSessionCompleteDelegates`. Use `Online::GetSubsystem(GetWorld())` (`OnlineSubsystemUtils.h`) — in the editor it resolves the per-PIE `NULL:Context_N` instance, outside it falls back to the plain one. |
| A room is created successfully but never shows up in search, on any PC | Steam only returns lobbies that are Public/Invisible **and joinable** (`isteammatchmaking.h:231`). UE sets `SetLobbyJoinable(bAllowJoinInProgress && ...)` on every UpdateSession (`OnlineSessionAsyncLobbySteam.cpp:657`), so one Update Session node with that pin unchecked hides the room forever. Watch `SESSIONFLAGS` in the host log with `-LogCmds="LogOnlineSession Verbose"`: bit1 dropping (451 -> 449) is that flag. |
| Rooms from other games appear in the search list | `SteamDevAppId=480` is a globally shared lobby pool. Advertise a private key (`ViaOnlineService` or higher) and add the same key to `SessionSearch->QuerySettings` — Steam translates it into `AddRequestLobbyListStringFilter`, so foreign lobbies never reach the client. `[OnlineSubsystem] bUseBuildIdOverride/BuildIdOverride` adds a second, client-side cut (search results only; it does not touch the NetDriver handshake). |
| Two PCs cannot see each other after a session change | Both the C++ and `Config/DefaultEngine.ini` are baked into the package. Any session filter (game id key, BuildIdOverride) has to be repackaged on **both** machines or they silently stop matching. |
