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

### Impact splash (droplets, secondary marks, phantom score)

- A paintball contact is two things. The replicated `FPaintSplat` carries `Splash`
  (`UPaintSplashProfile`), `IncidentDir/IncidentSpeed/BallRadius`, and every machine's
  `UPaintSubsystem::ApplySplat` turns that into `PaintSplash::PhantomLandings` (analytic parabolas,
  no traces) stamped as `bScoreOnly` splats: cells only, no picture, no sound. The visible droplets
  are per machine: `UPaintballProfile::PlayImpactEffect` books a `UPaintSplashLandingHandler`,
  spawns `ImpactFX` 1 cm off the surface, and `UPaintSplashSubsystem::ConfigureEffect` hands the
  Niagara system `User.Drop{0..3}Offset/Velocity/Radius`, `User.DropletCount` and
  `User.LandingHandler`. `NS_PaintSplash` (CPU emitter, stock modules only) reports each
  collision through `ExportParticleDataToBlueprint` (Position = landing, Velocity = collision
  normal, Size = launch speed) and the handler stamps a `bDrawOnly` splat with the profile's
  `DropletBrush`: picture only, never a cell. Score and picture therefore differ by design; the
  phantom radius is `DropletBrush->ComputeRadius(DropletSplatVolume, speed) *
  PhantomCellRadiusScale`, and a cell is claimed only when its centre falls inside that stamp.
- No secondary marks: check `mc.PaintSplash.Marks` (0 disables them), that the paintball's
  `ImpactFX` is `NS_PaintSplash` and its `Deposit.Splash` profile has a `DropletBrush` with a
  material (`MintChoco.Paint.Weapons.ProfileAssets` covers both), that the surface passes
  `FPaintDeposit::ReceivesSplat` (a decal-only surface leaves no marks), and that the handler pool
  is not exhausted (256 concurrent splashes; `mc.PaintSplash.Debug 1` logs every landing and draws
  the reach). A dedicated server never spawns the effect.
- Droplets fly but nothing lands: the Niagara `Collision` module traces `ECC_Visibility` on the
  CPU; `KillParticles` ends a droplet at `HasCollided` or past `MaxTravel * 1.5`; the system state
  must loop Once (Loop Duration = `User.MaxLifetime`) or the pooled component never finishes and
  `OnSystemFinished` never releases the handler (it still expires after `MaxLifetime + 0.5 s`).
- Never give `Particles.Position` an expression or a dynamic input through the MCP Niagara
  toolset (editor assert, see `docs/UnrealMcp.md`); droplets spawn at the system origin, which is
  why the component is placed 1 cm along the normal.
- The blob: `NS_PaintSplash`'s `Blob` emitter is one local-space mesh particle (a 100 cm cube,
  `Particles.Scale` = `User.BlobScale`, pivot lifted 50 mesh units so the cube stands on the
  plane) whose material `M_PaintSplashBlob` ray-marches four droplets on drag-damped parabolas
  plus a crown torus with the team look. Each droplet is a round cone from its head to a tail:
  while the cohesion holds (C++ `PaintSplash::Cohesion`, `CohesionRadius` smooth-min fading over
  `CohesionDecay`) the tail roots on the crown ring at the droplet's azimuth, so the splash reads
  as fingers rising off the rim; as it pinches off the tail slides `TailSeconds` (material scalar,
  0.06) behind the head and thins to a point, leaving teardrops. `CrownAt` defines the ring.
  `UPaintSplashSubsystem::BuildBlobMaterial` makes a MID per splash (`PaintSplashBlob` names:
  `Drop0..3` xyz offset + w radius, `Vel0..3`, `Phys`, `Crown`, `MarchMax`, `TeamId`) and
  `PaintSplash::BlobScale` sizes the cube. `Droplets` keeps fixed bounds (±450 cm) because its
  sprite renderer is disabled: an emitter with no enabled particle renderer and dynamic bounds
  trips the "only Emitter sourced renderers" warning and has no bounds at all. No blob: `Splash.BlobMaterial` unset, the material
  missing the Niagara mesh particles usage (`ProfileAssets` test), or `User.BlobScale` zero -
  a zero-scale mesh particle silently stops the entire system, droplets and marks included.
  Pixel Depth Offset must be wired by hand in the material editor (the MCP tool cannot), or the
  blob intersects geometry at the cube's surface instead of the fluid's.
- `APaintSplashTestActor` (Blueprintable) fires a fixed contact on a timer without a weapon:
  drop one in a scratch level with `Paintball` set, Simulate, and watch the marks.

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

## Look presets (`mc.Look`)

The shipped look is **baked into the level**, not applied at runtime. As of 2026-09-16 `Lvl_Stage`
and `Lvl_Stage_inside` carry Hybrid in their own data: `PostProcessVolume_0` (18 overrides plus the
`MI_PP_LookStylize_Hybrid` blendable), `DirectionalLight_0` (5800 K, source angle 0.3, specular
0.7), `VolumetricCloud_0` hidden — and the shared assets `MI_StageSkyDome` and `MPC_TeamLook`.
Pre-bake values and how to undo: `docs/history/2026-09-look-bake/PreBake.md`.

`ULookSubsystem` is what remains for comparison. It lays a `ULookPreset` over that baked look on a
game or PIE world at runtime only: a transient unbound post-process volume (priority 1000), the
level's sun, sky light, height fog, sky dome MID and volumetric cloud, console variables, and
material swaps rescanned every 0.5 s. `mc.Look Off` and world teardown restore all of it; console
variables are process-wide, so `Deinitialize` restores them too. Team color is **not** part of a
preset: `MPC_TeamLook` alone decides it.

| Symptom | Check first |
|---|---|
| `mc.Look Off` does not look like the old UE-default look | It is not supposed to. Off is now the baked Hybrid look; `mc.Look Baseline` applies the pre-bake values. |
| Hybrid on a baked map looks over-inked, outlines doubled | `WeightedBlendables` accumulate across volumes rather than override, so applying Hybrid on top of the baked level runs the cel/outline pass twice. `mc.Look Off`. |
| `mc.Look Baseline` restores everything except the clouds | `bHideVolumetricCloud` is one-way — a preset can hide the cloud but none can show it. Tick `VolumetricCloud_0`'s Visible box in the Details panel. |
| A preset's Sun value does nothing in PIE | `ULightComponentBase::SetIntensity` and friends are gated on `AreDynamicDataChangesAllowed()`, false for a Static-mobility light. Both stage maps are Movable; a new map may not be. |
| A preset looks the same as Off | The preset only covers fields whose override flag is on (`bOverride_*` inside `PostProcess`, `bOverride*` in Sun, SkyLight, Fog, SkyDome); the level volume still decides everything else. `mc.Look.List` marks the active preset. |
| Shadows stay hard after leaving Toon | `r.Shadow.Virtual.SMRT.RayCountDirectional` comes back on `mc.Look Off` and when the PIE world ends. The restore is set by code, so a later scalability change does not override it until the editor restarts. |
| A Toon character shows its PBR material again | The camera-overlap fade restores the materials it stored; the rescan swaps them back within 0.5 s. A unit that never swaps uses a slot material missing from the preset's `MaterialSwaps.From`. |
| Toon surfaces sparkle in shade or in the distance | Lumen noise crossing a band edge, or normal-map detail read as creases. Raise `CelParams.z` (band softness) or `OutlineParams.z` (normal threshold), or pull `FadeParams.xy` (outline fade, cm) closer in `MI_PP_LookStylize_Toon`. |
| Metal, emissive or very dark surfaces look wrong under the cel pass | The pass divides scene colour by GBuffer diffuse colour. Below albedo luminance 0.02, on unlit pixels, and on Toon BSDF pixels (`FadeParams.w` = 1) it passes the scene through untouched. |
| Review mannequins or splats missing from a capture | `ULookSettings::ReviewSetup` must be set on the settings CDO before Simulate starts. Splats wait `SplatDelay` for surfaces to register and log `리뷰 스플랫 N/M`. A mannequin placed inside level geometry shows only its shadow: pick open floor with traces first. |

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
| The item box sits still, or bobs while still announced / after pickup | `AItemPickup::Tick` is cosmetic and local (never replicated): it moves the `Mesh` component's *relative* transform around the base the BP set (`BobAmplitude` / `BobFrequency` / `SpinRateDeg`, `FItemPickupMotion`), so the `Trigger` sphere and the label never move. `UpdateMotionEnabled` turns the tick on only while `Active` and not `bCollected`; the base transform is read once in `BeginPlay`, so a BP that moves `Mesh` later fights the tick. |
| The board does not lean when turning, leans the wrong way, or leans only on one machine | While `bDashAnimationActive`, `UUnitAnimInstance` leans into the turn by the bank angle of a turning bike: `FUnitAnimMath::BoardLeanFromTurn` = `atan(GroundSpeed × yaw rate[rad/s] / BoardLeanGravity)` (10000 cm/s²: 1700 cm/s at 90°/s ≈ 15°), clamped to `BoardLeanMaxDegrees` (25, flip the sign to flip), eased by `BoardLeanInterpSpeed`. The yaw rate is `YawRateDegrees` (wrap-safe) of the visible body yaw (`GetBodyYaw`: mesh world rotation with `GetBaseRotationOffset` removed), the same formula on every machine: the owner's and server's mesh follows the actor, and a proxy's mesh is network-smoothed. The body yaw itself comes from the board turn controller (Unit facing section), so the lean follows the smooth carve, not the camera. Keys do not lean. `AUnit::SetMeshLean` rolls the mesh about the capsule's forward axis on top of the rest rotation captured once, and writes the same rotation through `CacheInitialMeshOffset`: network smoothing rewrites the mesh's relative rotation from `GetBaseRotationOffset` every tick, so a plain `SetRelativeRotation` would be erased on other clients. Tests: `MintChoco.Anim.BoardLean.*`, `MintChoco.Game.Facing.Board*`. |
| Speed Star feels no faster, or slower than dashing | The boost is `UUnitMovementComponent::SpeedBoostMultiplier` (1.5) on the base speed and stacks with `DashSpeedMultiplier` in `GetMaxSpeed`. It used to be a fixed 1500 cm/s that ignored dash, which fell below dash (BP_Unit walk 1000 × 1.7 = 1700) once the walk speed was raised. `MintChoco.Game.Movement.SpeedAsset` prints BP_Unit's walk, dash and boosted speeds and fails if the boost does not make both faster. |
| Speed Star rubber-bands on a client | The speed must ride the compressed move flag (`FLAG_Custom_1`, `bWantsSpeedBoost`), never a GAS attribute: `GameplayPrediction.h` says GE prediction and movement prediction are not time-correlated, so a GE-driven speed is simulated at the old speed on the server until the activation RPC lands. |
| An item ability ends after one RTT on the client | `WaitGameplayEffectRemoved` on the client's predicted handle fires when the prediction key catches up. The client ends on `WaitDelay(Duration)`; only the server waits for GE removal. |
| Using the same item twice runs two spinners | `bRetriggerInstancedAbility` must be true and the slot must reuse the existing spec (clear `RemoveAfterActivation`) instead of granting a second one. |
| `SetStackingType` link error | It is `WITH_EDITOR`-only and unexported. Write `StackingType` in the constructor under `PRAGMA_DISABLE_DEPRECATION_WARNINGS`. |
| Weapon still fires during the spinner | `UPaintWeaponComponent::TriggerBlockedTags` holds `State.Item.SweetSpinner`; both `PullTrigger` and `ServerFire` ask the owner's ASC. |
| The spinner's end animation (`SP_End`) blocks firing or items, or firing does not cut it | `SP_End` is a recovery outside the effect. `UGA_SweetSpinner::GetEffectDuration` ends the GE (and `State.Item.SweetSpinner`) when the spin ends; `UItemAbility` then stays alive for `GetRecoveryDuration` (one `SP_End`, capped by `Duration`) and calls `OnRecoveryStarted`, which sets the end pose. `UItemSlotComponent::InterruptItemRecovery` ends it: the owner on an accepted trigger pull (`AUnit::StartFire`), the server on `HandleWeaponFired` and charge start, both on any item activation (`UItemAbility::ActivateAbility`). A premature GE removal (`FinishItem`) skips recovery. The owner's trigger stays blocked for about one round trip into `SP_End`, until the server's GE removal replicates. Tests: `MintChoco.Items.SweetSpinner.Recovery`, `MintChoco.Items.Recovery.Flow`. |
| An item should run only for as long as something else lives (a thrown ball, a dome) | Give the profile `Duration` 0: the ability is instant, applies no GE and no state tag, and ends right after `OnItemActivated`. `ItemProfileAssetTest` skips the state-tag check for instant items. |
| A unit keeps walking while stunned on the server only, or only on the client | Both sides must agree through `UUnitMovementComponent`: `GetMaxSpeed` returns 0 and `ConstrainInputAcceleration` zeroes the input while `AUnit::IsMovementInputLocked()` (Stunned or HeroLanding tag, or a hero-landing phase). The input handler alone is not authoritative because the server consumes the client acceleration verbatim. |
| A unit should be immune (super armor) or show the outline, or keeps it after an item | Super armor is the tag `State.Status.SuperArmor`, from two sources: `UGE_SuperArmor` after a stun (`AUnit::HandleStunEnded`), and any item whose profile `GrantsSuperArmor()` (Speed Star: `USpeedStarProfile::bSuperArmor`, default on), where `UItemAbility::StartItem` adds the tag to the item GE so it lives exactly as long as the effect. `AUnit::HasSuperArmor` gates `TryApplyStun` and `Knockback`; the tag event drives `UpdateSuperArmorOutline` and the `Audio.Unit.SuperArmor.Begin/End` sounds on every machine. Overlapping sources stack the tag count, so the outline stays until both end. Test: `MintChoco.Items.SpeedStar.SuperArmor`. |
| A weapon hit should stun, or stuns the wrong length | `FPaintDeposit::StunDuration` / `StunSuperArmorDuration` on the contact (paintball `Deposit`, sniper `Impact`): `ApplyHit` and the sniper's pawn branch call `StrikeUnit`, which skips same-colour units and calls `AUnit::TryApplyStun(Stun, SuperArmor)`. A Charged profile scales the stun by the release charge (`FPaintFireContext::ChargeFraction`, sent in `ServerFire` as a byte); `MinChargeToFire` below 1 lets a partial charge fire. Items keep `UItemSettings` (2 s / 4 s) through the parameterless `TryApplyStun()`. The stun lives on the **paintball** asset, so swapping a weapon's `Paintball` reference drops it: the shotgun (`BP_Unit` → `DA_Weapon_Fan`) fires `DA_Paintball_Heavy_Trail` (Deposit.StunDuration 0.25), not `DA_Paintball_HeavyStun`, which nothing references any more. |
| The owner's own dash never shows (no board animation, weapon still fires while dashing) | `AUnit::bIsDashing` replicates `COND_SkipOwner`, so the owner must write it itself in `HandleDashStateChanged` (it did only on the server once). `IsDashing()` gates `UPaintWeaponComponent::IsTriggerBlocked` on the owner and the server, the dash start cancels both triggers (a held charge is lost), and `UUnitAnimInstance::bWeaponPoseHeld` is false while dashing. The board start/loop/end are states in the ABP's Locomotion machine keyed on `bIsDashing`, not C++ poses. The board mesh itself is `AUnit::BoardMesh` (a static mesh component like `GunMesh`): `UUnitDataAsset::BoardMesh` on the skeletal mesh's `Board` socket (on `foot_l`). It follows the **animation**, not the key: `UUnitAnimInstance` reads the `Locomotion` machine's current state each frame and calls `AUnit::SetBoardShown` while the state name starts with `Dash` (`LocomotionMachineName` / `DashStatePrefix` on the AnimInstance), so a dash press that never enters `Dash_Start` shows no board; the camera fade still hides it. |
| Knockback rubber-bands or double-launches | Only the server calls `LaunchCharacter` (`AUnit::Knockback`); a client RPC on top races the correction. |
| The hero-landing marker sits on the hovering character and it lands in place | `ComputeAimTarget` no longer rejects a wall, the sky or an out-of-range floor: it takes the aim ray's horizontal end (wall: one capsule radius in front), clamps it to `MaxAimDistance` from the takeoff, and traces down from above the hover apex to `GroundSearchDepth` below the takeoff for a walkable floor. Only a void underneath leaves no target; that is the only case where the hover timeout drops the unit where it is. Both sides compute it from the move's control rotation. |
| Hero landing restarts after landing, or the server never gets the landing effect | The rise starts only on a 0→1 edge of `FLAG_Custom_2` (`bHeroLandingArmed`), and a dive is never aborted when the flag drops; `AUnit::Landed` → `FinishHeroLandingDive` decides. Phase state rides in `FSavedMove_Unit`, so replays reproduce it. |
| Friendly pawns bounce off the chocolate dome, or trapped enemies never get released | The pass-through is a *component* ignore (`IgnoreComponentWhenMoving(Wall)`) applied on every machine; an actor-level ignore would also hide the pawn from the dome's Sensor. Enemies inside at spawn are in the replicated `PassThrough` list until the Sensor reports EndOverlap. |
| A bee dies to its own team, or stops dead when a ball touches it | Balls hit the bee's child `Shell` (Paintball Block only); the root sphere ignores Paintball so the projectile movement never gets a blocking hit from a ball. `ReceivePaintHit` ignores the bee's own paint id. |
| The bee zig-zags or bobs while chasing, or dives into the floor | Steer heading and altitude separately (`FBeeSteering::TurnTowardsSplit`): the obstacle probe only looks along the *flat* heading (the floor is never an obstacle), altitude is a proportional term (`VerticalComponent`) capped by ground clearance (`MaxDescent`), and a chosen avoidance direction is kept for 0.3 s. Turning the full 3D direction on the shortest arc swings through straight-down when the heading change is large, and a threshold-based hover push flips sign every tick. Measured in PIE by placing `BP_Bee` in the level (it picks the nearest unit and the settings profile on its own) and sampling `get_actor_transform`. The wanted heading itself is a quadratic Bezier tangent (`FBeeSteering::CurveHeading`: control point along the current heading, `CurveTension` / `CurveLookahead` on the profile) with a sine yaw wobble on top (`WobbleYawDeg`, fading inside `WobbleSettleDistance` so the bee still hits); avoidance and altitude take that heading as their input. A second sine at a different frequency (`WobblePitchDeg`, `WobblePitchAmplitudeDeg` / `WobblePitchFrequency`) is added to the altitude term *before* the `MaxDescent` clamp, so the two waves combine into a wobble that changes direction every moment (Lissajous) without ever pushing into the floor; keep the two frequencies off an integer ratio or the path collapses to one diagonal. |
| The bee shows the yellow sphere, or its client copy stutters while the server one glides | The bee body is `ABeeProjectile::Body`, a skeletal mesh component (`SKM_Bee_Wing` set on `BP_Bee`), not the inherited static `Mesh`, which `BP_Bee` leaves empty. `AItemProjectile::PostInitializeComponents` hands the static `Mesh` to the projectile movement's net interpolation; the bee re-points it to `Body` right after, so the interpolated component and the drawn one are the same thing. No wing animation asset exists yet: the mesh plays its reference pose. |

## Unit facing (body yaw vs camera)

`AUnit` has `bUseControllerRotationYaw` off. The camera boom follows control rotation on its own
(`bUsePawnControlRotation`), movement input and the shot direction are camera-relative, and the
body yaw is turned by `UUnitMovementComponent::PhysicsRotation`: it runs the engine's
`bUseControllerDesiredRotation` path (`RotationRate.Yaw` 720°/s, constant angular speed) only
while `ShouldFaceControlRotation()` is true, i.e. `Acceleration` is non-zero or
`AUnit::WantsToFaceAim()` (trigger held, aiming, charging, or within `FaceAimHoldSeconds` of the
last `OnFired`), and never while input is locked. Idle look-around leaves the body alone.
While dashing (riding the board) the yaw is not a constant-rate turn: `FBoardTurn::Step` drives it
(angular velocity eased by `BoardYawAccelInterpSpeed` toward
`clamp(|error| × BoardConvergeRate, BoardMinYawRate, BoardMaxYawRate)`, no overshoot), and keeps
driving it after the dash ends until the body has caught up, so the camera turns instantly while
the board carves after it. Move replays after a correction (`CharacterOwner->bClientUpdating`)
skip it.

| Symptom | Check first |
|---|---|
| The body snaps to the camera every frame, even idle | `bUseControllerRotationYaw` must stay false on `AUnit` and must not be overridden in `BP_Unit`; the boom, not the pawn, carries the control rotation. |
| The body never turns, or turns only on one machine | The gate must only read what both sides have: the move's `Acceleration` and control rotation, and weapon state the server also holds (`bCharging` is replicated, `OnFired` fires on the server in `ServerFire`). `bTriggerHeld` / `bAiming` are owner-only and are just an early yes there. `LastFireTime` is written before the dedicated-server early return in `HandleWeaponFired`. |
| A remote pawn's body lags or pops | Simulated proxies never run `PhysicsRotation`; their yaw is the replicated rotation smoothed by the engine. Tune on the owner and the listen host, not the proxy. |
| Tuning the turn | `RotationRate.Yaw` on the movement component (720 = 180° in 0.25 s) and `AUnit::FaceAimHoldSeconds` (0.5, keep equal to the anim `FireHoldTime` so the gun pose and the body release together). Board turn: `BoardMaxYawRate` 360, `BoardMinYawRate` 30, `BoardConvergeRate` 4 (full speed when 90° behind), `BoardYawAccelInterpSpeed` 10, all on the movement component. Test: `MintChoco.Game.Facing.*`. |

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
| Shots or the marker wobble with the animation, or fly below the crosshair | The physics must start from `FPaintFireContext::Muzzle` = `PaintAim::FireOrigin` (the view ray at the pawn's depth + `UPaintWeaponComponent::FireOriginForwardMargin`), never from the gun socket: the socket is only `VisualMuzzle` (FX, tracer start, mesh merge). `BuildContext` is the one place that fills both, on the owner and in `ServerFire` alike. |
| A ball visibly pops out of the shoulder instead of the gun | `UPaintballProfile::VisualMergeSeconds` is 0, or a profile filled `FPaintShot::Muzzle` without `VisualMuzzle`. The mesh slides from the socket onto the path in `APaintProjectile::Tick` (`PaintAim::MergeAlpha`); burst, rain and balloon balls have no offset by design. |
| The marker jitters at range while running | Camera lag moves the view origin; `MarkerInterpSpeed` smooths the screen position. Raise `MarkerShortfallCm` if it flickers at the threshold. |
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

## Audio

Three layers, all in `Source/MintChoco/Audio/`: native tags (`AudioTags::Audio_*`,
`AudioGameplayTags.h`), a bank (`USoundBank`: tag → `FSoundEvent` with sound, attenuation,
concurrency, volume, pitch range, 2D flag; the project bank is `Content/Audio/DA_SoundBank`, set in
`UGameAudioSettings::Bank`), and one playback door (`UGameAudioSubsystem`, a GameInstance
subsystem: `PlayAt` / `Play2D` / `PlayAttached` statics, `PlayMusic` / `StopMusic`). Code only
names a tag; a tag with no bank entry or no sound plays nothing and logs nothing. Per-thing sounds
are small override banks (`UUnitDataAsset::Sounds`, `UPaintWeaponProfile::Sounds`,
`UItemProfile::Sounds`) resolved override → project bank, so an override holds only the tags it
changes. A dedicated server never plays (`CanPlay`). Music: `MusicByPhase` in
`DefaultGame.ini` is applied from `AGameGameState::OnRep_MatchPhase`; maps without an
`AGameGameState` get `LobbyMusic` from `HandlePostLoadMap`; the timer warning edge in
`UGameHudWidget` switches to `Audio.Music.FinalRush` only if the bank has it (`HasEvent`).
`MintChoco.Audio.Bank` checks required tags, sounds, attenuation and pitch once the bank is non-empty.

| Symptom | Check first |
|---|---|
| 3D sounds are equally loud at any distance | An event with no `Attenuation` falls back to `UGameAudioSettings::DefaultAttenuation`, and with that unset to a transient natural-sound sphere the subsystem builds from `FallbackInnerRadius` / `FallbackFalloffDistance` (`GetDefaultAttenuation`). MCP cannot create `SoundAttenuation` / `SoundConcurrency` / `SoundMix` assets (`DataAssetTools.create` refuses non-DataAsset classes); make them in the editor only when the code fallback is not enough. |
| A sound plays twice on one machine, or on the server only | Sounds are never sent by RPC. Each machine plays from its own cosmetic hook: weapon fire next to `PlayMuzzleFX` (owner in `FireOnce`, server in `ServerFire`, watchers in `MulticastShotFired`), impact in `APaintProjectile::OnHit` (each machine's own ball), stun/super armor from the replicated tag callbacks, item pickup/announce from `AItemPickup` RepNotify + the server's direct call, balloon from server call + `OnRep_*`, burst from `APaintBurst::BeginPlay` (replicated actor). Adding a multicast on top doubles it. |
| Nobody else hears a landing or a jump pad | `AUnit::Landed` and `AJumpPad` run on the owner and the server only (proxies do not simulate landing / are skipped on purpose); there is no replicated signal, so bystanders hear nothing. Same for knockback (`Audio.Unit.Knockback` is declared but unbound: `LaunchCharacter` is server-only). |
| Splats or pellets drown everything | `Audio.World.Splat` fires once per `UPaintSubsystem::ApplySplat` and `Audio.Weapon.Impact` once per ball; a volley lands dozens in one frame. The cap is the bank entry's `Concurrency` (`SCC_Dense`), not the code. |
| The empty-tank click repeats every frame | `PlayEmptyCue` is owner-local (`IsLocallyControlled`) and throttled by `EmptyCueInterval`; `FireOnce` is called per tick for Continuous and per shot timer for Automatic. |
| Charge-ready rings at the wrong time for other players | Watchers have no press to measure; each machine arms its own `ChargeReadyTimer` from the charge it saw start (`ApplyChargingVisuals`) for `GetEffectiveChargeTime()`. The loop (`ChargeAudioComponent`) stops in `StopChargeFX`. |
| No lobby music on the first PIE map (title), but it starts after the first travel | PIE duplicates the first world instead of going through `LoadMap`, so `PostLoadMapWithWorld` never fires for it; `UGameAudioSubsystem` also listens to `FWorldDelegates::OnStartGameInstance` (PIE and standalone) and runs the same `UpdateMapMusic`. |
| `MusicByPhase` is empty at runtime (`LogObj: Error: LoadConfig ... import failed for MusicByPhase`) | A `TMap` config property is one line: `MusicByPhase=((Key, (TagName="...")),(Key2, ...))`. `+MusicByPhase=` per-element lines are array syntax; `LoadConfig` takes the last one as the whole map and fails to parse it. |
| Music stops instead of switching | `PlayMusic` on a tag with no sound calls `StopMusic`; ask `HasEvent` first when a switch is optional (FinalRush). Same tag twice is a no-op, which is what keeps Title→Room→Lobby continuous. |
| Win/lose sound is wrong on a client | `HandleMatchEnded` derives the local team from the first local PlayerController's `AGamePlayerState`; no PlayerState (spectator) plays the draw sound. |
| A button that opens a level (title "start") is silent while other buttons play | The click sound was spawned in the world that `OpenLevel` tears down on the next frame. `PlayEvent2D` therefore uses `SpawnSound2D` with `bPersistAcrossLevelTransition` (transient-package outer, like the music), so every 2D event survives a travel. A button that calls `QuitGame` stays silent: the process is gone before the sound. |
| UMG buttons are silent | Menu widgets derive from `UMenuWidget` (`Source/MintChoco/UI/`), which binds `Audio.UI.Click` / `Audio.UI.Hover` to every `UButton` in its own widget tree at `NativeConstruct`; a widget blueprint whose parent is plain `UserWidget` gets nothing. Reparent it (`WBP_Title`, popups, result screen) rather than wiring buttons one by one. A button that plays its own sound (join, create) is excluded through `ShouldAutoSound`; a C++ `NativeConstruct` override must call `Super`. |
| Footsteps never play | `UAnimNotify_Footstep` must be placed in the run sequences (`FootSocket` per notify); nothing in code triggers footsteps. |
| Item activate sound plays twice, or at the wrong frame | An item whose sound should land on a specific frame ticks `bActivateSoundFromAnimation` on its profile (the slot then skips the immediate `Audio.Item.Activate`) and places `UAnimNotify_ItemSound` in its use animation or pose sequence (spinner: `SP_Start`/`SP_Loop`/`SP_End`). The notify resolves the bank through `UItemSlotComponent::GetAnimationItem` (pose item, else the last item that started feedback), so one notify class serves every item. Pose sequences loop in the anim graph: a notify near the clip end can fire again on the wrap. For the spinner the notify must sit in `SP_Start`/`SP_Loop`/`SP_End` (the ability's pose overrides), not in `PoseAnimation`, which is never evaluated while the override is set. |
| HUD item slot frame never shows | `WBP_GameHUD`'s Tick graph owns `Img_Item`: it hides it while no item is held and swaps its brush for the item's `Icon` while one is. Any brush set on `Img_Item` in the designer is overwritten. The frame lives on `Img_Slot` (always visible, HitTestInvisible, same canvas slot as `Img_Item`, placed before it so it draws behind). |
| Item sound must stop when the clip section ends | Use `UAnimNotifyState_ItemSound` ("Item Sound Range") spanning the frames; the slot keeps the audio component per tag (`AnimationSounds`) and stops it on NotifyEnd, on `StopEffectFeedback` (effect ended early), and on EndPlay. A new range start replaces a still-playing sound of the same tag. |

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
