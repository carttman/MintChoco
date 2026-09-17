# MintChoco

C++ follows the Epic Coding Standard (tabs, PascalCase, U/A/F/E prefixes);
personal styles do not apply here.

This file keeps only what cannot be looked up and does not go stale. Symbol names, numbers,
asset paths and test ids belong in the code, not here — read them there, where they are true.
The reference docs carry the detail and are read on demand, not up front:

- `docs/UnrealMcp.md` — driving the editor over MCP. Read before the first MCP write of a session.
- `docs/Traps.md` — symptom → check-first tables, one section per system. Read when something
  looks wrong in PIE, before forming a theory.

## Working rules

- After an editor crash: stop, report the crash log (`Saved/Crashes/*/MintChoco.log`, the
  `Assertion failed` line and the lines before it), and wait. Never keep working in a relaunched
  editor on your own. A crash can surface minutes after the call that caused it, so read the log
  instead of blaming the last thing you did.
- Never drive the developer's mouse or keyboard. Anything MCP cannot do is a request to the
  developer with exact steps. Launching or relaunching the editor and running a build watcher
  are fine.
- Before "fixing" a graph the developer edited, ask what they changed. A wire that looks wrong
  may be a deliberate carrier convention.
- Try a new node type or property write on a scratch asset first, through save + recompile,
  before touching real assets.

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

## Conventions the code will not tell you

Each of these is a decision, not a fact you can read off a symbol. `docs/Traps.md` has the
systems these belong to.

- Team colors and gloss come from `MPC_TeamLook` and nowhere else. There is no fallback table: a
  collection that will not load leaves both teams neutral grey. Editing the MPC updates every
  material live — no recompile, no restart.
- Material function outputs are addressed **by index** by their call nodes. Append only; never
  reorder or delete one, however dead it looks.
- `TeamId` is 0 Mint, 1 Choco. Per-team material instances differ only by that scalar. A look
  that is not team-tinted opts out of the team color, and one outside the shared paint style
  opts out of the style — both through an explicit scalar, never by skipping the function.
- Paint only shows on the local directions a paintable component keeps. A disabled direction and
  any non-paintable static mesh get a transient decal instead, so "the paint went missing" is
  usually an atlas with no island there. Baking one needs CPU access on the mesh.
- Paint thickness lives in the material and the component reads it back, so the number exists in
  one place. Do not reintroduce it in code.
- Substrate ignores the clear-coat and ambient-occlusion pins, so the looks borrow them to carry
  other values. A number on those pins is not what the pin is named.
- Per-pixel data through a layer stack rides pixel attributes only. `CustomizedUVs` and WPO are
  vertex-frequency and will quantise anything sent through them.
- Height is read through a spline that returns its own analytic slope, never a finite difference.
- Movement-affecting item state rides compressed move flags, never a GAS attribute.
- Never fire a shot from the animated gun socket: it wobbles with the pose and differs between
  owner and server. That socket is for FX only; the shot leaves the sight line at the pawn's depth.
- Never turn `bUseControllerRotationYaw` back on. The body yaw is driven by the movement
  component on purpose, so that idle look-around does not turn the body.
- Every travel goes through the screen fade subsystem. A direct `ServerTravel` skips the cover.
- The end-of-match shot plays locally on every machine off already-replicated values, not off an
  RPC. Anything it needs must already be on a replicated path before the match ends.
- Steam sessions: resolve the subsystem with `Online::GetSubsystem(GetWorld())`, keep
  `bAllowJoinInProgress` on, and filter lobbies with a private key. Any filter change has to be
  repackaged on both PCs or they stop seeing each other.
- The shipped look is **baked into the levels**, not applied at runtime. The `mc.Look` presets
  are a comparison tool laid over it; "off" is the shipped look. Never treat a preset as the
  source of truth, and never assume a level carries the bake without checking that level.
