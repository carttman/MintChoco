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
  per call, passing the existing elements back verbatim as read; shrink only with the
  surviving elements unchanged (remove-and-edit in one call is "ambiguous"); in-place
  edits of any size work in one call. To replace element 0 and shrink, first rotate in
  place, then drop the tail. A fresh Custom node already holds one unnamed input.
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
  `execute_tool_script` with a `run()` that returns a dict.
- `UInputMappingContext.Mappings` is deprecated and invisible to the editor UI.
  Read and write `DefaultKeyMappings.mappings` instead.

### Verifying

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
- A new UPROPERTY is invisible to `ObjectTools` until the module is rebuilt and the
  editor restarted; setting it earlier just fails. After a UPROPERTY or struct-layout
  change, hot reload re-instances classes unreliably — restart before trusting PIE.
  The MCP server dies with the editor and its session expires on every restart.
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
  default identical or only one wins.
- A paintable mesh needs a unique UV1: `M_PaintUnwrap` and every paint read use
  TexCoord 1, and a missing channel silently pads with the last one (UV0). Art meshes
  arrive with UV0 only (`LightMapCoordinateIndex 0`,
  `StaticMaterials[].uVChannelData.localUVDensities[1] == 0`). `SourceModels` / Build
  Settings cannot be read or written through `ObjectTools`, so Generate Lightmap UVs
  (destination index 1) is a Static Mesh editor step; verify with the density read.

### Nanite tessellation displacement (UE 5.8)

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
| A Masked brush paints the whole RT | `r.EarlyZPassOnlyMaterialMasking` defaults to 1, so `clip()` is compiled out of the base pass (`BasePassPixelShader.usf`) and the canvas path has no depth prepass to mask instead. Use Opaque and preserve old contents by reading a second RT (ping-pong). |
| Frame drop when drawing several splats in one frame | `DrawMaterialToRenderTarget` rebinds the RT on every call. Batch with Begin/EndDrawCanvasToRenderTarget. |
| Coverage calculation is slow | Read Render Target Raw Pixel is a synchronous GPU wait. Use the cell grid instead. |
| Paint looks flat from the side | Normal/POM cannot change the silhouette. Is WPO or Displacement actually connected, and is the mesh tessellated enough? |
| Splats cut off at actor boundaries | Limitation of the UV approach. Move to the world-position unwrap, or paint neighbors via sphere overlap. |
| Colors differ between clients | Overlap-order differences are expected and allowed. A missing splat means the Unreliable Multicast dropped it — also check that local prediction and the server event are not drawn twice. |
| A texture set via SetTextureParameterValue reaches one sample node but not a Custom node | A TextureSampleParameter2D and a TextureObjectParameter sharing one parameter name: the instance override only reaches the sampler one. Give the object parameter its own name and set both from C++. A stale MaterialInstance can also keep failing after a parameter rename — test with a freshly created instance. |
| Paint mask/roughness respond but the relief normal stays flat on some faces | The height gradient is computed in UV1 (unwrap atlas) space but MP_Normal is applied in the mesh's UV0-derived tangent frame; per-face island orientation makes the result wrong or invisible. Build a world-space normal from position-map-derived axes instead of trusting mesh tangents. |
| Displaced paint shows a silhouette but shades flat | Nanite displacement keeps the vertex normal. Derive the normal from the height field: differentiate the position map for ∂P/∂u, ∂P/∂v (bounds-normalized local → cm via BoundsSize), then `cross(Pu + Hu·N, Pv + Hv·N)` in local space, `Local→World`, with the material's Tangent Space Normal off. Fall back to the vertex normal across atlas seams. |
| Displacement is a plateau with vertical cliffs and texel stairs | The height was derived from the binary id coverage read through a point-filtered buffer. Accumulate a soft height in its own channel (RG8: R id, G height) from a soft brush kernel, and bilinear-filter it by hand in the shader — the id sampler must stay TF_Nearest. |
| Mesh tears open along hard edges when painted | Faces sharing a hard edge displace along different vertex normals. Fade the height to 0 over the last texels of each unwrap island (`M_PaintEdgeFade` bakes distance-to-island-edge once per position map). Art with chamfered edges does not tear. |
| Sparkling noise on the displacement slope near edges | The per-texel deposit noise rides on the fade slope and grazes the specular lobe. Damp the derived normal's gradient by the same fade (`PaintNormalStrength 0` makes it vanish, which confirms it). |
| Paint edges look blocky however they are filtered | The brush binarizes the SDF into the id buffer at texel resolution; read-time bilinear only blurs the stairs. Store the brush distance in its own channel (`1 − d/range`, 0 = far, so clears stay valid), build per-team signed distances from the four corner texels, and threshold with `smoothstep(−w, w, sd)`, `w = clamp(0.5·fwidth, …, 0.5)`. |
| A rim outline appears on painted blobs at a distance | `fwidth(sd)` jumps between the clamped ±range samples under minification and smears the background through the whole rim; a swallowed same-team edge also keeps a small stored `d`. Cap `w` at one texel and pin quads whose four corner ids agree to ±range. |
| Background shows through where two teams meet | Sequential layer blends lerp twice. Carry the coverage already consumed (`S`) through the stack and use `alpha = cov / (1 − S)` per blend. |
| Paint reads as a matte sticker with glossy reflections | Flat team colors with a wet roughness. The fix is per-team looks with albedo texture and roughness designed together; for cream/ice cream go Substrate (slab with SSS MFP + fuzz) rather than overwriting attributes. |
| Paint on an art mesh lands twice or in the wrong place | The mesh has no UV1. TexCoord 1 pads with the last channel, so the unwrap and every read run on the art UV0 with its overlaps and mirroring. Generate Lightmap UVs into index 1 and rebuild. |

## OnlineSubsystem / Steam sessions

| Symptom | Check first |
|---|---|
| Creating a room in one PIE window makes every other PIE window create/travel too | `IOnlineSubsystem::Get()` without a world returns the one shared instance, so every PIE GameInstance subsystem registers on the same `OnCreateSessionCompleteDelegates`. Use `Online::GetSubsystem(GetWorld())` (`OnlineSubsystemUtils.h`) — in the editor it resolves the per-PIE `NULL:Context_N` instance, outside it falls back to the plain one. |
| A room is created successfully but never shows up in search, on any PC | Steam only returns lobbies that are Public/Invisible **and joinable** (`isteammatchmaking.h:231`). UE sets `SetLobbyJoinable(bAllowJoinInProgress && ...)` on every UpdateSession (`OnlineSessionAsyncLobbySteam.cpp:657`), so one Update Session node with that pin unchecked hides the room forever. Watch `SESSIONFLAGS` in the host log with `-LogCmds="LogOnlineSession Verbose"`: bit1 dropping (451 -> 449) is that flag. |
| Rooms from other games appear in the search list | `SteamDevAppId=480` is a globally shared lobby pool. Advertise a private key (`ViaOnlineService` or higher) and add the same key to `SessionSearch->QuerySettings` — Steam translates it into `AddRequestLobbyListStringFilter`, so foreign lobbies never reach the client. `[OnlineSubsystem] bUseBuildIdOverride/BuildIdOverride` adds a second, client-side cut (search results only; it does not touch the NetDriver handshake). |
| Two PCs cannot see each other after a session change | Both the C++ and `Config/DefaultEngine.ini` are baked into the package. Any session filter (game id key, BuildIdOverride) has to be repackaged on **both** machines or they silently stop matching. |
