# Driving the editor over MCP

Every item here cost real debugging time once. Read before any MCP write.

## Server and sessions

- The MCP server is **not** started with the editor: someone has to run the console command
  `ModelContextProtocol.StartServer`. When launching the editor yourself, pass it on the command
  line so no hand is needed:
  `UnrealEditor.exe <uproject> -skipcompile -ExecCmds="ModelContextProtocol.StartServer"`.
  Port 8000 listening (`netstat -an | grep 8000`) is the ready signal.
- The MCP server dies with the editor and its session expires on every restart.
- `call_tool` takes `toolset_name` + the bare `tool_name` (`IsPIERunning`, not
  `EditorToolset.EditorAppToolset.IsPIERunning`); the fully qualified name is "not found".
  The `toolset_name` is the opposite: it must be the full registered name, so
  `editor_toolset.toolsets.object.ObjectTools`, not `ObjectTools`. `list_toolsets` prints the
  registered names; the Python-side toolsets are `editor_toolset.toolsets.<module>.<Class>`
  (object, asset, blueprint, material, material_instance, data_asset, data_table, curve_table,
  string_table, actor, scene, primitive, static_mesh, skeletal_mesh, texture, programmatic), the
  C++-side ones are `EditorToolset.EditorAppToolset` / `.LogsToolset`,
  `ConfigSettingsToolset.ConfigSettingsToolset`, `LiveCodingToolset.LiveCodingToolset`,
  `UMGToolSet.UMGToolSet`, `NiagaraToolsets.NiagaraToolset_*`.
- When the MCP tools are not registered in the session (editor launched after the session
  started), drive the server over HTTP from PowerShell, not bash: `ConvertTo-Json -Compress`
  builds the `values` string safely, while every bash quoting route mangled the JSON. Pattern:
  POST `initialize`, keep the `Mcp-Session-Id` header, POST `notifications/initialized`, then
  `tools/call` with `call_tool` (`toolset_name`, bare `tool_name`, `arguments`); responses arrive
  as SSE `data:` lines. Over HTTP the `toolset_name` is the full dotted path and
  `describe_toolset` returns every tool's argument schema.
- There is no console-command or editor-python route: `ProgrammaticToolset` only orchestrates
  registered tools, and `EditorAppToolset.SearchCVars` only reads. `ProgrammaticToolset` runs
  `execute_tool_script` with a `run()` that returns a dict; inside the script the call is
  `execute_tool('<full.toolset.path>.<tool>', json.dumps({...}))` — one dotted string naming
  the tool and the arguments as a JSON **string** (a dict, kwargs, or a separate tool-name
  argument all raise TypeError). `ObjectTools` takes `instance` + `properties` / `values`
  (a JSON string). `try/except` inside a script does not reliably catch `execute_tool` failures.
  `time.sleep` inside a script lets the engine tick (Lumen settles), but a long script loses the
  MCP session and its result: 14 viewport captures did, 6 plus a 30 s sleep did not.
- Automation tests run headless without MCP, even while the editor is open:
  `UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MintChoco.Paint; Quit"
  -unattended -nullrhi -abslog=<log>`; grep the log for `Test Completed. Result=`.
  The `HttpListener unable to bind 127.0.0.1:8000` line is the second instance and harmless —
  unless the editor and the test instance start together: whichever binds first keeps port
  8000, and an editor that lost it has no MCP until relaunched. Run the headless test before
  launching the editor, not alongside it.
- Packaging fails with `UATHelper: Error: LogHttpListener: Error: HttpListener unable to bind to
  127.0.0.1:8000` and `Cook failed` even though the cook log ends with `CookCommandlet ... result 0`:
  the cook commandlet is a second editor instance and a commandlet exits 1 whenever any
  Error-level line was logged (`LaunchEngineLoop.cpp`, `GWarn->GetNumErrors() > 0`). It only tries
  to bind because Editor Preferences → Plugins → Model Context Protocol → **Auto Start Server** is
  on (`bAutoStartServer=True` in `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`),
  and the open editor already owns the port. Turn Auto Start off and start the server with the
  `-ExecCmds` line above (or the console command), or close the editor before packaging.

## Writes that crash the editor

- Never write a layer stack (`MaterialAttributeLayers.DefaultLayers`, an MI's `MaterialLayers`)
  through `ObjectTools.set_properties`: the arrays land one at a time and
  `FMaterialLayersFunctions::Validate` asserts on the first length mismatch. Stacks are edited
  in the UI (node details / MI layer panel); in-place edits that keep every array length are the
  only safe write. Replacing `layers[0]` in place from a script with every length kept compiled
  and saved cleanly (M_PaintableSurface).
- Never grow/shrink `AttributeSetTypes` / `AttributeGetTypes` on Get/SetMaterialAttributes
  through `ObjectTools`: the node's pin arrays are only resized by the editor's PostEditChange
  path, so the next compile indexes out of range. Use Make/BreakMaterialAttributes (fixed pins;
  no FrontMaterial pin, so Substrate slabs are built in the master from carried attributes) or
  have the developer add pins in the UI. Plugin templates worth duplicating instead of creating:
  Material Layers under `/Landmass/PreviewContent/MatLayers/`, blends under
  `/BaseMaterial/Materials/Blends/`.

## ObjectTools

- `set_properties` takes `values` as a JSON **string** (an object is accepted and silently
  returns false). Every object reference needs the full object path (`/Game/X/Y.Y`, a class as
  `/Script/Module.Class`, a BP class as `/Game/X/BP_Y.BP_Y_C`, a CDO component as
  `/Game/X/BP_Y.Default__BP_Y_C:Comp`). Collision on a mesh component is written through
  `BodyInstance: {CollisionEnabled: "NoCollision"}`, not a top-level `CollisionEnabled`.
- Array property: **grow** by exactly one element per call, **re-reading the array right before
  each append** (the editor rewrites GUIDs and float rounding on write, and a stale copy is
  rejected as "elements changed alongside the size change"); shrink only with the surviving
  elements unchanged (remove-and-edit in one call is "ambiguous"); in-place edits of any size
  work in one call. To replace element 0 and shrink, first rotate in place, then drop the tail.
  The shrink call must spell out every field the read returned on each survivor
  (`PlayerMappableKeySettings: "None"` included); an omitted field counts as a change and the
  removal is rejected as ambiguous.
- Map property (`TMap<FGameplayTag, FStruct>`, e.g. `USoundBank::Events`): filling an **empty**
  map accepts plain tag-string keys (`"Audio.UI.Click": {...}`) and writes the whole map in one
  call. Editing values afterwards must reuse the keys exactly as the read returned them
  (`"(TagName=\"Audio.UI.Click\")"`); plain keys on a non-empty map are rejected as "keys swapped
  without size change". Read, edit the values in place, write the full map back.
  Inside a `ProgrammaticToolset` script, one `execute_tool`
  call per step obeys these rules while still batching the round-trips.
- A `TSoftObjectPtr` array reads back as plain path strings, but an appended element is stored
  as a `{refPath}` object: re-read the array before every append and pass the elements exactly
  as read.
- A nested USTRUCT writes in one call (`{"Deposit": {"BrushProfile": {"refPath": ...},
  "SplatVolume": 1}}`), but `get_properties` returns its members camelCased (`brushProfile`), so
  compare by value, not by key, when reading back.
- `get_properties` fails as a whole when any requested property is unreadable on that class;
  ask per class. It cannot read a UPROPERTY without an `Edit*`/`Visible*` specifier
  (`bCollected`, `HeldItem` were unreadable until `VisibleInstanceOnly`), and `set_properties`
  refuses one without an `Edit*` specifier ("could not be set"), so a value meant to be poked in
  PIE needs `EditAnywhere`.
- A new UPROPERTY is invisible to `ObjectTools` until the module is rebuilt and the editor
  restarted; setting it earlier just fails. After a UPROPERTY or struct-layout change, hot
  reload re-instances classes unreliably — restart before trusting PIE.
- `UInputMappingContext.Mappings` is deprecated and invisible to the editor UI. Read and write
  `DefaultKeyMappings.mappings` instead.
- Editing a BP CDO does not reach level instances that hold a serialized override. Verify on
  the instance (`Lvl:PersistentLevel.<Actor>.<Component>`), never only on the CDO.
  `reset_properties` resets to the C++ default, not the Blueprint default — clear an override
  by setting the value explicitly.

## Materials

- `MaterialTools` object references need the full object path (`/Game/X/M_Foo.M_Foo`); a bare
  package path is rejected. Pins without a name (Transform, MaterialLayerOutput, Saturate,
  ComponentMask, FunctionOutput) are addressed as `"None"` in `connect_expressions`.
  `MaterialTools.get_expressions`/`recompile` and `MaterialInstanceTools.list_parameters` take
  `material_or_function` / `material`.
- `get_expression_inputs` mislabels a multi-output source: it prints the first output's name
  whatever the wire really uses. The truth is the raw `outputIndex` in
  `ObjectTools.get_properties(..., ["Inputs"])` on a Custom node. `connect_expressions` **by
  output name does land correctly**; only the read is unreliable. Its unwired pins come back
  with `expression` as the **string** `"None"`, so test `str(ex) == "None"`, not `is None`.
  Make/Break pins are `FExpressionInput`s that `ObjectTools.get_properties` cannot read; verify
  wiring with `get_expression_inputs`.
- A fresh Custom node already holds one unnamed input. Appending a Custom node input triggers an
  automatic compile that fails with "missing input N" until the pin is wired; only the
  recompile after wiring counts. Setting a call node's `MaterialFunction` through `ObjectTools`
  does refresh its pins, and `MaterialTools.create_function` creates a function asset without
  duplicating a template.
- `MaterialTools.connect_to_output` / `get_property_input` silently ignore `MP_Displacement`,
  and `MaterialEditorOnlyData` exposes no `FExpressionInput` properties. That one wire is
  dragged by the developer, and material editor edits live in the **preview copy until
  Apply**: save right after a UI drag stores the original without the wire. Drag → Apply →
  save. `MP_FrontMaterial` connects fine. Deleting or reordering a function's outputs shifts
  the call node's output indices; have the developer re-check hand-wired pins afterwards.
- `MaterialInstanceTools.set_scalar_parameter` / `set_vector_parameter` only write
  **GlobalParameter** entries, so a Material Layer parameter (same name in several slots) never
  lands. Write layer overrides through `ObjectTools.set_properties` on
  `ScalarParameterValues` / `VectorParameterValues` with
  `parameterInfo {name, association: LayerParameter, index}` (0 = Background, then layers in
  stack order) and a zero `expressionGUId` — the UI writes zero GUIDs too. One append per call.
  `ExpressionGUID` on parameter nodes is unreadable.
- Duplicated parameter nodes with the same name are one parameter; keep every copy's default
  identical or only one wins. This also holds across functions: a parameter declared inside two
  Material Functions surfaces once on the master, which is the safe way to feed a value into a
  function (adding a FunctionInput leaves the existing call nodes' pins stale).
- Team look: `MaterialTools.create_parameter_collection`; append MPC entries one per
  `set_properties` call on `VectorParameters` (GUIDs are created automatically). A
  `MaterialExpressionCollectionParameter` needs `Collection` written first and `ParameterName`
  in a second call (`ParameterId` is synced by PostEditChange and is invisible to ObjectTools);
  a clean material compile proves the id landed. MPC default values are live in the session
  without a restart. To add a team-tinted master: scalar parameter `TeamId` (default 0, group
  "Team") → `MaterialExpressionMaterialFunctionCall` with `MaterialFunction` set to
  `/Game/Assets/Paint/Materials/Team/MF_TeamLook.MF_TeamLook` (pins appear) →
  `connect_expressions` by output name (`Color`, `Roughness`, …; slab F0 goes through
  `SubstrateMetalnessToDiffuseAlbedoF0`) → recompile → per-team MI `clear_parameters` +
  `set_scalar_parameter TeamId 0/1` → save.

## Verifying

- `MaterialTools.recompile` does **not** raise on every shader failure: a decal that was still
  in the Surface domain recompiled "cleanly" while the log said "Failed to compile Material ...
  Default Material will be used in game". Grep `LogMaterial: Warning: [AssetLog]` after every
  recompile; a "Missing Preview connection" line on a material *function* is only its
  thumbnail and can be ignored.
- Under Substrate, `MaterialDomain` is **derived from the Substrate tree** on every compile
  (`UMaterial::RebuildShadingModelField`): writing `MD_DeferredDecal` through `ObjectTools`
  returns true and is silently reverted to Surface. A decal is `SubstrateSlabBSDF` →
  `SubstrateConvertToDecal` (inputs `DecalMaterial`, `Coverage`) → `MP_FrontMaterial`; that
  node sets the domain, `BLEND_Translucent` is the "TranslucentGreyTransmittance" it wants, and
  the decal's alpha goes into `Coverage`. Slab pins have spaces (`Diffuse Albedo`, `SSS MFP`).
- `MaterialTools.recompile` propagates **parameter default** changes to everything on screen,
  but **structural graph changes** (new wires, new outputs, Custom code) do not reach materials
  already rendering in the session — only a freshly created MaterialInstanceConstant compiles
  the current in-memory graph. Verify structural edits through a fresh MIC, or save and restart
  the editor; a PIE-created MID renders whatever shader its parent material loaded at editor
  start.
- `EditorAppToolset.CaptureViewport` with `captureTransform` renders the **editor world**, not
  the PIE world. During Simulate, `SetCameraTransform` then `CaptureViewport` with
  `captureTransform: null` renders the simulate world at that camera, without editor sprites
  or wireframes; `CaptureEditorImage` grabs the whole editor window instead.
  `SceneTools.find_actors` does search the PIE world while it runs, and PIE actors address as
  `/Game/<Path>/UEDPIE_0_<Map>.<Map>:PersistentLevel.<Actor>_C_0`.
- `CaptureViewport` / `CaptureEditorImage` / `CaptureAssetImage` return base64 too large for
  the tool result; decode the saved result file with PowerShell (`ConvertFrom-Json` →
  `[Convert]::FromBase64String`) and Read the PNG. `CaptureAssetImage` on a mesh is a quick way
  to make a placeholder icon texture (`TextureTools.import_file`).
- While PIE runs, `AssetTools.save_assets` / `exists` / `is_dirty` fail with "Asset does not
  exist" even though compiles succeed. Stop PIE, then save. `is_dirty` takes `asset_path`,
  `save_assets` takes `asset_paths`, `exists` takes `path`.

## Niagara

`NiagaraToolsets.NiagaraToolset_System`: `GetSystemSummary` lists user variables and emitters;
`AddUserVariables` creates `User.TintColor`; `SetStackInputData` on `InitializeParticle` /
`Color` with a `/Script/NiagaraEditor.NiagaraExt_StackInputData_Linked` value
(`{linkedVariable: {name, type: {classStructOrEnum: /Script/CoreUObject.LinearColor}}}`) binds
it. Every `emitterRef` / `stackInputRef` needs all six fields (`system`, `emitterName`,
`scriptName` such as `ParticleSpawnScript`, `moduleName`, `rendererIndex: -1`,
`inputNameStack: ["Color"]`). Read one input with `GetStackInputData` rather than
`GetEmitterTopology`, whose dump is enormous. Check `GetSystemCompileState` (`bHasErrors`) and
`GetStackIssues`, then `save_assets`. `LiveCodingToolset.CompileLiveCoding` reports Live Coding
disabled in this project, so C++ changes reach PIE only after an editor-target rebuild.

## Blueprints, data assets, textures

- `BlueprintTools.create` (`folder_path`, `asset_name`, `asset_type` = parent class ref) makes a
  Blueprint; `ActorTools.add_component` on the Blueprint asset returns the template
  `/Game/X/BP_Y.BP_Y_C:Comp_GEN_VARIABLE`, which `ObjectTools` writes directly.
  `DataAssetTools.create` makes data assets (`folder_path`, `asset_name`, `asset_type` = class
  ref); `MaterialInstanceTools.create` makes MICs (`parent`); `TextureTools.import_file`
  (`folder_path`, `asset_name`, `source_file`); `SceneTools.load_level` takes `level_path`.
- `BlueprintTools.write_graph_dsl` **replaces** the event it names and leaves the other events
  alone, and a failed write rolls back; `read_graph_dsl` mislabels property getters (an
  `ItemProfile.DisplayName` getter prints as `TypedElementFramework|Testing|GetDisplayName`) —
  check with `get_node_infos`. Ambiguous ids (`Appearance|SetBrushfromTexture` exists on Border
  and Image) resolve to the first class; `Appearance|SetBrushResourceObject` is the Image-only
  spelling. `get_node_type_pins` creates no lasting nodes.
- `TextureTools.import_file` never overwrites. Reimporting means: unhook referencers, delete,
  import, re-hook — deleting also nulls sampler defaults that pointed at it.
- `/Engine/BasicShapes/*` are engine assets: duplicate into `/Game` before enabling Nanite
  (`StaticMeshTools.set_nanite_enabled`) or editing anything.

## Levels

- `SceneTools.create_level_instance` spawns the actor but never loads its sub-level: the
  viewport stays empty, traces return null, and `get_actor_bounds` still reports a plausible box
  (asset-registry bounds, builder brush included). Save and `load_level` the map again;
  `LogLevelInstance: Loaded N levels` is the ready signal. A level made with
  `AssetTools.duplicate` is dirty in memory and `load_level` refuses it until it is saved.
  Setting a LevelInstance's `WorldAsset` through `ObjectTools` does reload it immediately.
- `AssetTools.move` on a map returns false (a "Continue with rename?" dialog is auto-declined)
  and leaves that map open as the current level. Relocate a level by `duplicate` → save →
  repoint every `WorldAsset` → save the map → delete the old file on disk; `AssetTools.delete`
  reports true on a loaded map without removing the file.
- `Config/DefaultEditor.ini` regrows `[/Script/AdvancedPreviewScene.SharedProfiles]` whenever an
  asset editor saves preview-scene settings (`USharedProfiles` is `defaultconfig`;
  `UAssetViewerSettings::Save` writes the three engine profiles). Commit it once instead of
  reverting it.
