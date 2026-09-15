# MintChoco

C++ follows the Epic Coding Standard (tabs, PascalCase, U/A/F/E prefixes);
personal styles do not apply here.

Reference docs, read on demand rather than up front:

- `docs/UnrealMcp.md` — driving the editor over MCP: server setup, writes that crash, arrays
  and pins, materials, Niagara, verifying. Read before the first MCP write of a session.
- `docs/Traps.md` — symptom → check-first tables and system summaries: paint pipeline,
  rendering, items (GAS), balloon, match flow, screen fade, Steam sessions. Read when
  something looks wrong in PIE before forming a theory.

## Working rules

- After an editor crash: stop, report the crash log (`Saved/Crashes/*/MintChoco.log`, the
  `Assertion failed` line and the lines before it), and wait. Never keep working in a
  relaunched editor on your own. A crash can surface minutes after the offending call
  (autosave thumbnail compiles), so read the log instead of blaming the last call.
- Never drive the developer's mouse or keyboard. Anything MCP cannot do (the `MP_Displacement`
  wire, console commands, painting in PIE, layer stacks, dynamic pins) is a request to the
  developer with exact steps. Launching or relaunching the editor and running a build watcher
  are fine.
- Before "fixing" a graph the developer edited, ask what they changed. A wire that looks
  wrong (packed ids on Anisotropy, weights on Refraction, coverage on Opacity) may be a
  deliberate carrier convention.
- Try a new node type or property write on a scratch asset first, through save + recompile,
  before touching real assets.

## Team look (colors and gloss)

The single source is `Content/Assets/Paint/Materials/Team/MPC_TeamLook`: `MintColor`,
`MintSubsurface`, `MintSurface` and the same three for `Choco`. `Surface` packs
`(Roughness, Specular [UE 0..1], Metallic, WetCoat)`. Editing the MPC updates every material
live; no recompile, no restart.

- Shaders read it through `MF_TeamLook(TeamId)` → `Color, Subsurface, Roughness, Specular,
  Metallic, WetCoat`. Never reorder or delete those outputs: call nodes address them by index.
- C++ reads it through `TeamLook::Get / GetColor / GetDisplayColor` (`Game/TeamLook.h`), which
  resolve `UPaintSettings::TeamLookCollection`; the fallback table in `TeamLook.cpp` must match
  the MPC seed. `Teams::` holds ids and names only.
- Every team-tinted master declares a scalar `TeamId` (0 Mint, 1 Choco); per-team MIs differ
  only by `TeamId`. A non-team look (`MI_InkLiquid_Red`, `MI_InkSurface_Red`) sets
  `UseTeamLook = 0`. `ML_Look_Mint/Choco` call the function with a constant 0/1, so the old
  `Albedo/Roughness/Specular/SSSMFP/WetCoat` layer parameters no longer exist. MIDs set
  `TeamId` from `Splat.PaintId`; the HUD bar and Niagara `User.TintColor` (splat, burst,
  muzzle flash, charge hold) take `TeamLook::GetColor(PaintId)`.
- Test: `MintChoco.Game.TeamLook.*`.

## MCP: the rules that crash or waste an hour

Details and the rest are in `docs/UnrealMcp.md`.

- Never write a layer stack or `AttributeSetTypes` / `AttributeGetTypes` through `ObjectTools`:
  both assert or index out of range on the next compile. Layer stacks are UI work.
- `ObjectTools.set_properties` takes `values` as a JSON **string**; every object reference is a
  full object path (`/Game/X/Y.Y`, `/Script/Module.Class`, `/Game/X/BP_Y.BP_Y_C`).
- Arrays: grow by exactly one element per call, re-reading the array right before each append;
  shrink only with survivors spelled out exactly as read.
- `MaterialTools.recompile` does not raise on shader failure: grep
  `LogMaterial: Warning: [AssetLog]` after every recompile. Structural graph changes never reach
  materials already rendering; verify through a freshly created MIC or restart the editor.
- Under Substrate `MaterialDomain` is derived from the graph on compile; a decal is
  `SubstrateConvertToDecal` → `MP_FrontMaterial`, not a domain write.
- A new UPROPERTY needs a module rebuild and an editor restart before `ObjectTools` sees it,
  and it needs an `Edit*` specifier to be writable. Restart after any struct-layout change
  before trusting PIE. Live Coding is disabled in this project's editor.
- While PIE runs, `AssetTools.save_assets` / `exists` / `is_dirty` fail. Stop PIE, then save.
- Editing a BP CDO does not reach level instances with a serialized override: verify on the
  instance.
- The MCP server is not started with the editor: launch with
  `-ExecCmds="ModelContextProtocol.StartServer"`; port 8000 listening is the ready signal.
  Headless tests: `UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MintChoco;
  Quit" -unattended -nullrhi -abslog=<log>`, run before launching the editor, not alongside it.
- `EditorAppToolset.CaptureViewport` renders the editor world; capture the simulate world with
  `SetCameraTransform` then `captureTransform: null`. Captures return base64 too large for the
  tool result: decode the saved result file with PowerShell.

## Paint pipeline in one paragraph

The paint buffer is a procedural planar atlas: one island per enabled local direction on
`UPaintableComponent`, packed from the mesh bounds by `FPaintIslandLayout`, baked on the CPU by
`PaintAtlasBaker` from LOD 0 (Allow CPU Access required). `MF_PaintOverlay` picks the island
from the pixel's local normal, so paint only shows on kept directions; anything else, and any
non-paintable static mesh, gets a transient side-splat decal. Stamps and the cell grid are in
the scaled-local frame (world cm). Paint thickness is `DisplacementScaling.Magnitude` in world
cm and must equal `PaintMaxHeight`. Nanite tessellation is on by default in 5.8; displacement
follows the vertex normal and never recomputes shading normals. Per-pixel data through the
layer stack rides pixel attributes only (Anisotropy, Refraction.rg, PixelDepthOffset, Opacity,
Tangent); `CustomizedUVs` and WPO are vertex-frequency. Everything else: `docs/Traps.md`.

## Gameplay systems in one line each

- Items (`Source/MintChoco/Items/`): `UItemProfile` + `UItemAbility` + one
  `UItemGameplayEffect` subclass per item, `UItemSlotComponent` on `AUnit`, list in
  `[/Script/MintChoco.ItemSettings]`. Movement-affecting state rides compressed move flags,
  never a GAS attribute.
- Weapon hits reach any `IPaintHitReceiver` (`ABalloon`) through `FPaintDeposit::ApplyHit`
  before the surface test; `HitPower` is the balance number.
- Facing: the body yaw follows the camera only while moving or firing
  (`UUnitMovementComponent::PhysicsRotation` gate, `RotationRate.Yaw` 720, `AUnit::FaceAimHoldSeconds`);
  idle look-around leaves the body alone. Never turn `bUseControllerRotationYaw` back on.
  While dashing (board) the yaw follows through `FBoardTurn` (smooth angular velocity with min/max
  rate) instead of the constant 720, and the board lean is that turn rate × speed.
- Firing origin: a shot's physics leaves the sight line at the pawn's depth (`PaintAim::FireOrigin`
  → `FPaintFireContext::Muzzle`); the gun socket is only `VisualMuzzle`, for FX, tracers and the
  ball mesh's merge onto the path (`UPaintballProfile::VisualMergeSeconds`). Never fire from the
  animated socket: it wobbles with the pose and differs between owner and server.
- Match flow: `AGameGameState::MatchPhase` WaitingForPlayers → Countdown → Playing → Ended;
  input is locked until Playing through `IsPlayerInputAllowed`.
- Screen fade: every travel goes through `UScreenFadeSubsystem::*TravelWithFade`; a direct
  `ServerTravel` skips the cover.
- Steam sessions: use `Online::GetSubsystem(GetWorld())`, keep `bAllowJoinInProgress` on, and
  filter lobbies with a private key; any filter change must be repackaged on both PCs.
