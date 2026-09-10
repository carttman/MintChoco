# MintChoco

C++ follows the Epic Coding Standard (tabs, PascalCase, U/A/F/E prefixes);
personal styles do not apply here.

## Working rules

- After an editor crash: stop, report the crash log (`Saved/Crashes/*/MintChoco.log`,
  the `Assertion failed` line and the log lines just before it), and wait. Never keep
  working in a relaunched editor on your own. A crash can surface minutes after the
  offending call (autosave thumbnail compiles), so read the log instead of blaming the
  last call.
- Never drive the developer's mouse or keyboard. Anything MCP cannot do (the
  `MP_Displacement` wire, console commands, painting in PIE, layer stacks, dynamic
  pins) is a request to the developer with exact steps. Launching or relaunching the
  editor and running a build watcher are fine.
- Before "fixing" a graph the developer edited, ask what they changed. A wire that
  looks wrong (packed ids on Anisotropy, weights on Refraction, coverage on Opacity)
  may be a deliberate carrier convention.
- Try a new node type or property write on a scratch asset first, through
  save + recompile, before touching real assets.

## Unreal MCP pitfalls

Each of these cost real debugging time once.

### Writes that crash the editor

- Never write a layer stack (`MaterialAttributeLayers.DefaultLayers`, an MI's
  `MaterialLayers`) through `ObjectTools.set_properties`: the arrays land one at a time
  and `FMaterialLayersFunctions::Validate` asserts on the first length mismatch.
  Stacks are edited in the UI (node details / MI layer panel); in-place edits that keep
  every array length are the only safe write. Replacing `layers[0]` in place from a
  script with every length kept compiled and saved cleanly (M_PaintableSurface).
- Never grow/shrink `AttributeSetTypes` / `AttributeGetTypes` on Get/SetMaterialAttributes
  through `ObjectTools`: the node's pin arrays are only resized by the editor's
  PostEditChange path, so the next compile indexes out of range. Use
  Make/BreakMaterialAttributes (fixed pins; no FrontMaterial pin, so Substrate slabs
  are built in the master from carried attributes) or have the developer add pins in
  the UI. Plugin templates worth duplicating instead of creating: Material Layers under
  `/Landmass/PreviewContent/MatLayers/`, blends under `/BaseMaterial/Materials/Blends/`.

### Arrays and pins

- `ObjectTools.set_properties` on an array property: **grow** by exactly one element
  per call, **re-reading the array right before each append** (the editor rewrites
  GUIDs and float rounding on write, and a stale copy is rejected as "elements
  changed alongside the size change"); shrink only with the
  surviving elements unchanged (remove-and-edit in one call is "ambiguous"); in-place
  edits of any size work in one call. To replace element 0 and shrink, first rotate in
  place, then drop the tail. The shrink call must spell out every field the read
  returned on each survivor (`PlayerMappableKeySettings: "None"` included); an omitted
  field counts as a change and the removal is rejected as ambiguous. A fresh Custom node already holds one unnamed input.
  Inside a `ProgrammaticToolset` script, one `execute_tool` call per step obeys these
  rules while still batching the round-trips.
- `MaterialTools.get_expression_inputs` mislabels a multi-output source: it prints
  the first output's name whatever the wire really uses. The truth is the raw
  `outputIndex` in `ObjectTools.get_properties(..., ["Inputs"])` on a Custom node.
  `connect_expressions` **by output name does land correctly** (verified with a probe
  Custom node); only the read is unreliable.
  Its unwired pins come back with `expression` as the **string** `"None"`, so test
  `str(ex) == "None"`, not `is None`. Make/Break pins are `FExpressionInput`s that
  `ObjectTools.get_properties` cannot read; verify wiring with `get_expression_inputs`.
- `BreakMaterialAttributes` exposes Refraction as **RG only** (float2). Pixel-frequency
  carriers through a layer stack: Anisotropy (1), Refraction (2), PixelDepthOffset (1),
  Opacity (1), Tangent (3) — every look layer must pass each one through Break → Make.
  In use: Anisotropy / Refraction.rg / PixelDepthOffset = per-team signed distances,
  Opacity = consumed coverage `S`, EmissiveColor.r = thin-paint opacity (slab Emissive
  is deliberately unwired), Tangent = per-look `(SecondRoughness, weight, WetCoat)`
  written by the looks rather than passed through.
- `MaterialTools.connect_to_output` / `get_property_input` silently ignore
  `MP_Displacement`, and `MaterialEditorOnlyData` exposes no `FExpressionInput`
  properties. That one wire is dragged by the developer, and material editor edits
  live in the **preview copy until Apply**: save right after a UI drag stores the
  original without the wire. Drag → Apply → save. `MP_FrontMaterial` connects fine.
  Deleting or reordering a function's outputs shifts the call node's output indices;
  have the developer re-check hand-wired pins afterwards.
- `CustomizedUVs` (and WPO) in a MaterialAttributes struct are **vertex-frequency**
  even when written by Make and read back through Break inside a pixel chain: the
  value is computed per vertex and interpolated. Per-pixel data smuggled through a
  layer stack must ride pixel attributes (Anisotropy, Refraction, Opacity, PDO, ...).
  Parameters inside a Material Layer/Blend are namespaced per slot, so C++ cannot set
  them by name — feed runtime data through the stack `Input` instead.
- `MaterialTools` object references need the full object path (`/Game/X/M_Foo.M_Foo`);
  a bare package path is rejected. Pins without a name (Transform, MaterialLayerOutput)
  are addressed as `"None"` in `connect_expressions`. `ProgrammaticToolset` runs
  `execute_tool_script` with a `run()` that returns a dict; `execute_tool` and
  `call_tool` need the full dotted toolset path
  (`editor_toolset.toolsets.material.MaterialTools.get_expressions`), and
  `ObjectTools` takes `instance` + `properties` / `values` (a JSON string).
- `MaterialInstanceTools.set_scalar_parameter` / `set_vector_parameter` only write
  **GlobalParameter** entries, so a Material Layer parameter (same name in several
  slots) never lands. Write layer overrides through `ObjectTools.set_properties` on
  `ScalarParameterValues` / `VectorParameterValues` with
  `parameterInfo {name, association: LayerParameter, index}` (0 = Background, then
  layers in stack order) and a zero `expressionGUId` — the UI writes zero GUIDs too.
  One append per call. `ExpressionGUID` on parameter nodes is unreadable.
- `UInputMappingContext.Mappings` is deprecated and invisible to the editor UI.
  Read and write `DefaultKeyMappings.mappings` instead.
- A nested USTRUCT writes in one `set_properties` call
  (`{"Deposit": {"BrushProfile": {"refPath": ...}, "SplatVolume": 1}}`), but
  `get_properties` returns its members camelCased (`brushProfile`), so compare by value,
  not by key, when reading back.

### Verifying

- `MaterialTools.recompile` does **not** raise on every shader failure: a decal that was
  still in the Surface domain recompiled "cleanly" while the log said "Failed to compile
  Material ... Default Material will be used in game". Grep `LogMaterial: Warning: [AssetLog]`
  after every recompile; a "Missing Preview connection" line on a material *function* is only
  its thumbnail and can be ignored.
- Under Substrate, `MaterialDomain` is **derived from the Substrate tree** on every compile
  (`UMaterial::RebuildShadingModelField`): writing `MD_DeferredDecal` through `ObjectTools`
  returns true and is silently reverted to Surface. A decal is `SubstrateSlabBSDF` →
  `SubstrateConvertToDecal` (inputs `DecalMaterial`, `Coverage`) → `MP_FrontMaterial`; that node
  sets the domain, `BLEND_Translucent` is the "TranslucentGreyTransmittance" it wants, and the
  decal's alpha goes into `Coverage`. Slab pins have spaces (`Diffuse Albedo`, `SSS MFP`).
- `MaterialTools.recompile` propagates **parameter default** changes to everything
  on screen, but **structural graph changes** (new wires, new outputs, Custom code)
  do not reach materials already rendering in the session — only a freshly created
  MaterialInstanceConstant compiles the current in-memory graph. Verify structural
  edits through a fresh MIC, or save and restart the editor; a PIE-created MID
  renders whatever shader its parent material loaded at editor start.
- `EditorAppToolset.CaptureViewport` with `captureTransform` renders the **editor
  world**, not the PIE world. Use `CaptureEditorImage` for PIE; `SceneTools.find_actors`
  does search the PIE world while it runs, and PIE actors address as
  `/Game/<Path>/UEDPIE_0_<Map>.<Map>:PersistentLevel.<Actor>_C_0`.
- There is no console-command or editor-python route: `ProgrammaticToolset` only
  orchestrates registered tools, and `EditorAppToolset.SearchCVars` only reads.
  `try/except` inside a script does not reliably catch `execute_tool` failures.
- The MCP server is **not** started with the editor: someone has to run the console
  command `ModelContextProtocol.StartServer`. When launching the editor yourself, pass it
  on the command line so no hand is needed:
  `UnrealEditor.exe <uproject> -skipcompile -ExecCmds="ModelContextProtocol.StartServer"`.
  Port 8000 listening (`netstat -an | grep 8000`) is the ready signal.
- `call_tool` takes `toolset_name` + the bare `tool_name` (`IsPIERunning`, not
  `EditorToolset.EditorAppToolset.IsPIERunning`); the fully qualified name is "not found".
- `ObjectTools.get_properties` cannot read a UPROPERTY without an `Edit*`/`Visible*`
  specifier either (`bCollected`, `HeldItem` were unreadable until `VisibleInstanceOnly`).
- `ObjectTools.set_properties` takes `values` as a JSON **string** (an object is accepted and silently
  returns false). Every object reference needs the full object path (`/Game/X/Y.Y`, a class as
  `/Script/Module.Class`, a BP class as `/Game/X/BP_Y.BP_Y_C`, a CDO component as
  `/Game/X/BP_Y.Default__BP_Y_C:Comp`). Collision on a mesh component is written through
  `BodyInstance: {CollisionEnabled: "NoCollision"}`, not a top-level `CollisionEnabled`. A
  `TSoftObjectPtr` array reads back as plain path strings, but an appended element is stored as a
  `{refPath}` object: re-read the array before every append and pass the elements exactly as read.
- There is no create-blueprint tool: `AssetTools.duplicate` a small BP (`BP_ItemSpawnPoint`) and
  `BlueprintTools.set_parent` (`blueprint` + `parent_class`) to the C++ class. `DataAssetTools.create`
  makes data assets (`folder_path`, `asset_name`, `asset_type` = class ref); `MaterialInstanceTools.create`
  makes MICs (`parent`); `TextureTools.import_file` (`folder_path`, `asset_name`, `source_file`).
  `MaterialTools.get_expressions`/`recompile` and `MaterialInstanceTools.list_parameters` take
  `material_or_function` / `material`; `AssetTools.exists` takes `path`; `SceneTools.load_level` takes `level_path`.
- When the MCP tools are not registered in the session (editor launched after the session started),
  drive the server over HTTP from PowerShell, not bash: `ConvertTo-Json -Compress` builds the `values`
  string safely, while every bash quoting route mangled the JSON. Pattern: POST `initialize`, keep the
  `Mcp-Session-Id` header, POST `notifications/initialized`, then `tools/call` with `call_tool`
  (`toolset_name`, bare `tool_name`, `arguments`); responses arrive as SSE `data:` lines.
- `BlueprintTools.write_graph_dsl` **replaces** the event it names and leaves the other
  events alone, and a failed write rolls back; `read_graph_dsl` mislabels property
  getters (an `ItemProfile.DisplayName` getter prints as
  `TypedElementFramework|Testing|GetDisplayName`) — check with `get_node_infos`.
  Ambiguous ids (`Appearance|SetBrushfromTexture` exists on Border and Image) resolve
  to the first class; `Appearance|SetBrushResourceObject` is the Image-only spelling.
  `get_node_type_pins` creates no lasting nodes.
- `EditorAppToolset.CaptureViewport` / `CaptureEditorImage` / `CaptureAssetImage`
  return base64 too large for the tool result; decode the saved result file with
  PowerShell (`ConvertFrom-Json` → `[Convert]::FromBase64String`) and Read the PNG.
  `CaptureAssetImage` on a mesh is a quick way to make a placeholder icon texture
  (`TextureTools.import_file`).
- Automation tests run headless without MCP, even while the editor is open:
  `UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MintChoco.Paint; Quit"
  -unattended -nullrhi -abslog=<log>`; grep the log for `Test Completed. Result=`.
  The `HttpListener unable to bind 127.0.0.1:8000` line is the second instance and harmless —
  unless the editor and the test instance start together: whichever binds first keeps
  port 8000, and an editor that lost it has no MCP until relaunched. Run the headless
  test before launching the editor, not alongside it.
- A new UPROPERTY is invisible to `ObjectTools` until the module is rebuilt and the
  editor restarted; setting it earlier just fails. After a UPROPERTY or struct-layout
  change, hot reload re-instances classes unreliably — restart before trusting PIE.
  The MCP server dies with the editor and its session expires on every restart.
  `ObjectTools.set_properties` also refuses a UPROPERTY without an `Edit*` specifier
  ("could not be set"), so a value meant to be poked in PIE needs `EditAnywhere`.
- While PIE runs, `AssetTools.save_assets` / `exists` / `is_dirty` fail with
  "Asset does not exist" even though compiles succeed. Stop PIE, then save.

### Assets and instances

- `/Engine/BasicShapes/*` are engine assets: duplicate into `/Game` before enabling
  Nanite (`StaticMeshTools.set_nanite_enabled`) or editing anything.
- Editing a BP CDO does not reach level instances that hold a serialized override.
  Verify on the instance (`Lvl:PersistentLevel.<Actor>.<Component>`), never only on
  the CDO. `ObjectTools.reset_properties` resets to the C++ default, not the Blueprint
  default — clear an override by setting the value explicitly.
- `TextureTools.import_file` never overwrites. Reimporting means: unhook referencers,
  delete, import, re-hook — deleting also nulls sampler defaults that pointed at it.
- Duplicated parameter nodes with the same name are one parameter; keep every copy's
  default identical or only one wins. This also holds across functions: a parameter
  declared inside two Material Functions surfaces once on the master, which is the
  safe way to feed a value into a function (adding a FunctionInput leaves the existing
  call nodes' pins stale).
- `SceneTools.create_level_instance` spawns the actor but never loads its sub-level: the
  viewport stays empty, traces return null, and `get_actor_bounds` still reports a plausible
  box (asset-registry bounds, builder brush included). Save and `load_level` the map again;
  `LogLevelInstance: Loaded N levels` is the ready signal. A level made with
  `AssetTools.duplicate` is dirty in memory and `load_level` refuses it until it is saved.
  Setting a LevelInstance's `WorldAsset` through `ObjectTools` does reload it immediately.
- `AssetTools.move` on a map returns false (a "Continue with rename?" dialog is auto-declined)
  and leaves that map open as the current level. Relocate a level by `duplicate` → save →
  repoint every `WorldAsset` → save the map → delete the old file on disk;
  `AssetTools.delete` reports true on a loaded map without removing the file.
- `Config/DefaultEditor.ini` regrows `[/Script/AdvancedPreviewScene.SharedProfiles]`
  whenever an asset editor saves preview-scene settings (`USharedProfiles` is
  `defaultconfig`; `UAssetViewerSettings::Save` writes the three engine profiles).
  Commit it once instead of reverting it.
- Paint no longer needs UV1. The paint buffer is a procedural planar atlas: one island per
  enabled local direction (`bPaintUp` and friends on `UPaintableComponent`, Up by default,
  `bFloorFollowsWorldUp` adds the sky-facing side), packed by `FPaintIslandLayout` from
  the mesh bounds and the actor scale, filled by `PaintAtlasBaker` on the CPU from LOD 0
  (Allow CPU Access is the only mesh requirement; a Nanite mesh bakes from its fallback).
  `MF_PaintOverlay` picks the island from the pixel's local normal (`PaintAtlasUV`), so a
  surface only shows paint on directions it keeps; a hit on any other direction is a
  transient splat (`FPaintSplat::bTransient`) that spawns `UPaintSettings::SideSplatEffectClass`.
  So is a hit on a static mesh with no `UPaintableComponent`: `FPaintDeposit::ReceivesSplat`
  admits a paintable or a non-Movable `UStaticMeshComponent`, and every splat source (deposit,
  stroke, sample click) goes through it. Pawns and Movable meshes take nothing, since the
  decal would stay where the surface was.
- Stamps, the cell grid and the brush's `BoundsMin/BoundsSize` are in the scaled-local
  frame (local × |Scale3D|): every length is world cm, non-uniform scale included. A hit
  normal maps to a local direction with `InverseTransformVectorNoScale(N) * Scale3D`.
- In a Custom node, `VertexNormalWS` is the local normal rotated but never scaled; undo
  the rotation alone (`LocalToWorld` rows divided by `InvNonUniformScale`, transposed).
  The `Transform` node World→Local divides by the scale instead and skews it. Feeding a
  local normal to Local→World needs `/ (Scale*Scale)` first (inverse transpose).
- `Begin/EndDrawCanvasToRenderTarget` uses the world's single canvas: never nest or
  interleave two surfaces' pairs. Rectangles drawn into the shared scratch buffer are
  copied back with `TransitionAndCopyTexture`; both targets must share the pixel format
  (`UPaintSubsystem::CreatePaintBuffer`).
- Paint thickness is `DisplacementScaling.Magnitude` in **world cm** (the overlay divides
  by the primitive scale along the normal); keep `PaintMaxHeight` equal to it or shading
  and silhouette disagree. It was 3 while the sample cubes were 3× scaled, so the old
  look was 9 cm.

### Nanite tessellation displacement (UE 5.8)

- Substrate: a slab stacked on top through `SubstrateVerticalLayering` may only use the
  SimpleVolume SSS type (the node help says so); Diffusion SSS survives only on the
  bottom slab. So paint stays the bottom slab and the wet coat is the top:
  `VL(Top = Weight(coat, Tangent.B), Bottom = body)`. The stack carries per-look
  `(SecondRoughness, SecondRoughnessWeight, WetCoat)` on **Tangent** (pixel-frequency,
  lerped by `BlendMaterialAttributes` in `MF_PaintTeamBlend`); a look that leaves
  `Make.Tangent` unwired leaks the vertex tangent into those slab inputs.
- Tessellation is **on by default** (`r.Nanite.AllowTessellation` no longer exists;
  `r.Nanite.Tessellation`, `ProgrammableRaster`, `ComputeRasterization` all default 1;
  `r.Nanite.DicingRate` is the density knob). The material needs
  `bEnableTessellation=true`, and `DisplacementScaling.Magnitude` is baked into proxy
  bounds — set it once, never animate it.
- The Displacement pin compiles at pixel frequency with the full material evaluated
  per micro-vertex every frame, so sampling a runtime render target through a Custom
  node is fine. It **does not** recompute shading normals, and it displaces along the
  **vertex normal**: two faces meeting at a hard edge move apart and tear.

## Rendering traps (check the right column first)

| Symptom | Check first |
|---|---|
| Find Collision UV always returns (0,0) | Physics → Support UV From Hit Results enabled and editor restarted? Trace Complex + Return Face Index on the trace? |
| UV misaligned on Nanite meshes | Nanite collides against the fallback mesh. Use a separate simple collision mesh, or disable Nanite for the prototype. |
| Splats overwrite instead of accumulate | Is the brush Blend Mode Translucent? Is Clear Render Target called every draw? |
| Splats add instead of overwriting | `DrawMaterialToRenderTarget` can never write the RT's alpha: every translucent blend mode uses `BF_Zero` for source alpha (`TranslucentRendering.cpp`). Encode coverage without alpha (paint-id buffer). |
| A Masked brush paints the whole RT | `r.EarlyZPassOnlyMaterialMasking` defaults to 1, so `clip()` is compiled out of the base pass (`BasePassPixelShader.usf`) and the canvas path has no depth prepass to mask instead. Use Opaque and preserve old contents by reading the surface's own buffer while drawing into the shared scratch buffer, then copy the stamp rectangles back. |
| Frame drop when drawing several splats in one frame | A full-target draw per splat scales with the atlas, not the stamp. Draw only the stamp's rectangles (`BuildStampRects`, one per island it reaches) and copy them; keep one Begin/End per splat because the next splat reads the copy. |
| Paint appears on the wrong face, or not at all, after a hit | The surface keeps only its enabled directions; the hit's local direction (dominant axis of the unscaled local normal) decides. Check `keeps paint on ...` in LogPaint, the six `bPaint*` flags and `bFloorFollowsWorldUp`. Exactly 45° faces are a coin flip between two islands. |
| A shelf or the underside of a bridge shows the paint of the surface above it | Planar projection: an island keeps only the outermost surface along its axis, so stacked same-facing surfaces in one mesh share texels. Split them into separate actors. |
| Big cubes look blobby, small ones fine | Displacement was local units: `Magnitude × scale`. Now it is world cm; if it still scales, the overlay's `PaintDisplaceScale` node or `MF_PaintNormal`'s `localPerWorld` term is unwired. |
| Coverage calculation is slow | Read Render Target Raw Pixel is a synchronous GPU wait. Use the cell grid instead. |
| Paint looks flat from the side | Normal/POM cannot change the silhouette. Is WPO or Displacement actually connected, and is the mesh tessellated enough? |
| A hitscan or aim trace passes straight through units | In 5.8 the `Pawn` capsule profile **and** the `CharacterMesh` profile ignore `ECC_Visibility` (BaseEngine.ini). Trace on `PaintballChannel` (`ECC_GameTraceChannel1`, `Weapons/PaintProjectile.h`): pawns and paintable meshes block it, balls in flight ignore it, so the ray hits what a paintball would. The sniper does this; the gun's aim trace still uses Visibility and converges through pawns. |
| Splats cut off at actor boundaries | The stamp is world-space and every overlapping surface (sphere overlap in `UPaintSubsystem::ApplySplat`) draws its own part; a missing half means that actor has no `UPaintableComponent`, keeps no direction facing the hit, or its collision does not answer `ECC_Visibility`. |
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

Items live in `Source/MintChoco/Items/`: a `UItemProfile` asset per item (display, pickup
mesh, duration, `AbilityClass`), a `UItemAbility` subclass per item, one `UItemGameplayEffect`
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

## Paint hit receivers (balloon)

`FPaintDeposit::HitPower` is the game-balance number a contact carries (sniper `Impact` 100,
`DA_Paintball_Light` 3, Standard 10, Heavy 25, brush 2). `FPaintDeposit::ApplyHit` strikes any hit
actor implementing `IPaintHitReceiver` (`Weapons/PaintHitReceiver.h`) before the surface test, so
every weapon reaches it with one hook and the receiver need not be paintable. `ABalloon`
(`Game/Balloon.h`, `BP_Balloon`) is the first receiver: 100 health, inflates with damage, the last
team to hit it owns the pop, `MulticastBurst(Seed, PaintId)` launches `BurstCount` balls of
`BurstPaintball` (real on the server, cosmetic on clients), and it re-inflates after `RespawnDelay`.
Its collision blocks only the `Paintball` channel, so pawns and the camera pass through.

## Screen fade (map transitions)

`UScreenFadeSubsystem` (`Source/MintChoco/Screen/`) owns the cover widget across maps. Every
travel goes through `TravelWithFade` / `ServerTravelWithFade` / `ClientTravelWithFade` (the lobby
Blueprint and `UOnlineSessionsSubsystem` already do); a server travel spawns `AScreenFadeRelay`
whose multicast darkens every client first. The engine's `PreLoadMapWithContext` pins the cover,
`PostLoadMapWithWorld` starts the ready wait, and the map opens when the local controller has a
PlayerState, a game map (`AGameGameState`) has the local pawn, and no hold is registered
(`AddHold(Owner, Reason)`; `UPaintSubsystem` holds while an atlas bakes). `MaxReadyWait` opens
it regardless. Settings: `[/Script/MintChoco.ScreenFadeSettings]`, widget `WBP_LoadingScreen`.

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
