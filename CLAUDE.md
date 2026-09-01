# MintChoco

C++ follows the Epic Coding Standard (tabs, PascalCase, U/A/F/E prefixes);
personal styles do not apply here.

## Unreal MCP pitfalls

Each of these cost real debugging time once.

- `UInputMappingContext.Mappings` is deprecated and invisible to the editor UI.
  Read and write `DefaultKeyMappings.mappings` instead.
- `ObjectTools.set_properties` on an array property: append exactly one element per
  call, and pass the existing elements back verbatim as read.
- `ObjectTools.reset_properties` resets to the C++ default, not the Blueprint
  default. To clear a level-instance override, set the value explicitly.
- Editing a BP CDO does not reach level instances that hold a serialized override.
  Verify on the instance (`Lvl:PersistentLevel.<Actor>.<Component>`), never only on
  the CDO.
- `TextureTools.import_file` never overwrites. Reimporting means: unhook referencers,
  delete, import, re-hook — deleting also nulls sampler defaults that pointed at it.
- After a UPROPERTY or struct-layout change, hot reload re-instances classes
  unreliably; restart the editor before trusting PIE. The MCP server dies with the
  editor, and a hung editor process can keep holding port 8000.
- Inside `ProgrammaticToolset` scripts, `try/except` does not reliably catch
  `execute_tool` failures — split risky calls into separate script runs.
- `MaterialTools.recompile` propagates **parameter default** changes to everything
  on screen, but **structural graph changes** (new wires, new outputs, Custom code)
  do not reach materials already rendering in the session — only a freshly created
  MaterialInstanceConstant compiles the current in-memory graph. Verify structural
  edits through a fresh MIC, or save and restart the editor; a PIE-created MID
  renders whatever shader its parent material loaded at editor start.

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
| Paint mask/roughness respond but the relief normal stays flat on some faces | The height gradient is computed in UV1 (unwrap atlas) space but MP_Normal is applied in the mesh's UV0-derived tangent frame; per-face island orientation makes the result wrong or invisible. Judge normals face by face (front face can look dead while a side face wobbles), and prefer building a world-space normal from position-map-derived axes over trusting mesh tangents. |
