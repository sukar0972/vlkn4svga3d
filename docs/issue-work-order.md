# Issue work order

Snapshot: 2026-10-05, 77 open issues at revision `852de75`.

Fix the command boundary and validation coverage first, then prevent unsafe guest
memory access and partial GPU recording. Query state and resource lifetimes precede
device-loss recovery. Capabilities must match the working subset before expanding
shaders and render states. Presentation optimizations and cleanup come last.

Numbers within each group give the execution order. Combine overlapping issues in
one reviewed change, but close each only when its own acceptance criteria pass.
The device-loss umbrella requires end-to-end evidence in addition to its component
fixes. Guest Piglit and sustained-load claims require live guest runs.

## Validation and wire framing

- [x] [#97](https://github.com/sukar0972/vlkn4svga3d/issues/97) — Medium: CI only runs thin `make test`
- [x] [#83](https://github.com/sukar0972/vlkn4svga3d/issues/83) — High: Core/adapter FIFO 2D framing desync
- [x] [#89](https://github.com/sukar0972/vlkn4svga3d/issues/89) — High: qemu_vmsvga ignores bytesConsumed on execute failure
- [x] [#87](https://github.com/sukar0972/vlkn4svga3d/issues/87) — Medium: Duplicate FIFO parsers: preload vs core/adapter
- [x] [#147](https://github.com/sukar0972/vlkn4svga3d/issues/147) — High: qemu_vmsvga posts guest fence even when flush fails

## Guest-memory bounds and resource limits

- [x] [#84](https://github.com/sukar0972/vlkn4svga3d/issues/84) — High: GMR2 SINGLE_PPN wrap and guest GPA shift overflow
- [x] [#159](https://github.com/sukar0972/vlkn4svga3d/issues/159) — Medium: Render-state and transform maps grow without limit
- [x] [#104](https://github.com/sukar0972/vlkn4svga3d/issues/104) — Medium: Vertex reads can OOB (no robustBufferAccess)
- [x] [#131](https://github.com/sukar0972/vlkn4svga3d/issues/131) — High: Surface redefine destroys old surface before allocate; failure orphans sid
- [x] [#158](https://github.com/sukar0972/vlkn4svga3d/issues/158) — Medium: Pipeline cache unbounded; stencil ref in key (not dynamic)
- [x] [#140](https://github.com/sukar0972/vlkn4svga3d/issues/140) — Medium: Svga3VlknConfig capacity fields dead; DEVCAP vs SVGA3_MAX_* drift

## Submission, query and device-loss recovery

- [x] [#90](https://github.com/sukar0972/vlkn4svga3d/issues/90) — High: SURFACE_COPY records Vk cmds then returns mid-loop
- [x] [#91](https://github.com/sukar0972/vlkn4svga3d/issues/91) — High: Draw records texture barriers then fails late
- [x] [#95](https://github.com/sukar0972/vlkn4svga3d/issues/95) — Medium: endQuery flush failure leaves query FSM inconsistent
- [x] [#94](https://github.com/sukar0972/vlkn4svga3d/issues/94) — Medium: END_QUERY never writes guestResult PENDING
- [x] [#132](https://github.com/sukar0972/vlkn4svga3d/issues/132) — Medium: WAIT_FOR_QUERY returns SUCCESS after ignored writeGuest failure
- [x] [#133](https://github.com/sukar0972/vlkn4svga3d/issues/133) — Medium: waitForQuery accepts unended query and leaves m_queryActive stuck
- [x] [#134](https://github.com/sukar0972/vlkn4svga3d/issues/134) — Medium: Mock GetQueryPoolResults ignores readiness (hides query FSM bugs)
- [x] [#155](https://github.com/sukar0972/vlkn4svga3d/issues/155) — Medium: Query pool create failure fabricates occlusion results
- [ ] [#96](https://github.com/sukar0972/vlkn4svga3d/issues/96) — Medium: Destroy VkImage after failed/ignored flush
- [ ] [#106](https://github.com/sukar0972/vlkn4svga3d/issues/106) — Medium: Failed flush at fence wedges FIFO forever
- [ ] [#113](https://github.com/sukar0972/vlkn4svga3d/issues/113) — High: Host / device-lost recovery incomplete (umbrella)

## Capability, surface-format and feature honesty

- [x] [#98](https://github.com/sukar0972/vlkn4svga3d/issues/98) — Medium: Unknown SVGA format silent B8G8R8A8 fallback
- [x] [#93](https://github.com/sukar0972/vlkn4svga3d/issues/93) — High: YUV (etc.) advertised, mapped to BGRA + wrong bpp
- [x] [#92](https://github.com/sukar0972/vlkn4svga3d/issues/92) — High: MSAA/A2C/supersample caps advertised, pipelines always 1×
- [ ] [#153](https://github.com/sukar0972/vlkn4svga3d/issues/153) — Medium: Depth-stencil format honesty (has_stencil vs Vk vs aspect)
- [x] [#152](https://github.com/sukar0972/vlkn4svga3d/issues/152) — Medium: clear() applies STENCIL aspect without format check
- [ ] [#121](https://github.com/sukar0972/vlkn4svga3d/issues/121) — Medium: Signed bump/normal formats mapped as UNORM (advertised)
- [ ] [#120](https://github.com/sukar0972/vlkn4svga3d/issues/120) — Medium-High: GENERATE_MIPMAPS blits without format blit support (AUTOGEN advertised)
- [x] [#119](https://github.com/sukar0972/vlkn4svga3d/issues/119) — Medium: MAX_FIXED_VERTEXBLEND=4 advertised, unimplemented
- [x] [#138](https://github.com/sukar0972/vlkn4svga3d/issues/138) — Medium: MAX_VERTEX_SHADER_TEXTURES=4 advertised; VS has no sampler interface
- [x] [#139](https://github.com/sukar0972/vlkn4svga3d/issues/139) — Medium: TEXTURE_GRADIENT_SAMPLING=1 while DSX/DSY/TEXLDD fail-closed
- [ ] [#129](https://github.com/sukar0972/vlkn4svga3d/issues/129) — Medium: Fixed-function texture combiner advertised but not implemented
- [x] [#123](https://github.com/sukar0972/vlkn4svga3d/issues/123) — Medium: Wireframe/point fill without fillModeNonSolid
- [ ] [#122](https://github.com/sukar0972/vlkn4svga3d/issues/122) — Medium: Anisotropy enabled without feature gate / limit clamp
- [ ] [#116](https://github.com/sukar0972/vlkn4svga3d/issues/116) — Medium: MIRRORONCE without samplerMirrorClampToEdge
- [x] [#114](https://github.com/sukar0972/vlkn4svga3d/issues/114) — Medium: Dual-source blend factors (SRC1*) silently map to ONE
- [ ] [#162](https://github.com/sukar0972/vlkn4svga3d/issues/162) — Medium: Relative const addressing rejected while SM3 advertised
- [x] [#161](https://github.com/sukar0972/vlkn4svga3d/issues/161) — Medium: ps_1_x accepted but wrong (white output; _x2/_x4/_d2 dropped)
- [x] [#151](https://github.com/sukar0972/vlkn4svga3d/issues/151) — Medium: DEVCAP shader temps=32 but translator only r0–r15
- [x] [#160](https://github.com/sukar0972/vlkn4svga3d/issues/160) — Medium: Geometry instancing (vertex divisors) silently ignored

## Shader and vertex-input correctness

- [x] [#118](https://github.com/sukar0972/vlkn4svga3d/issues/118) — Medium: Dest relative addressing not rejected
- [x] [#137](https://github.com/sukar0972/vlkn4svga3d/issues/137) — Medium: Predicated non-MOV ALU accepted then mis-emitted
- [ ] [#125](https://github.com/sukar0972/vlkn4svga3d/issues/125) — High: ps_2_x shaders read texture coordinates from wrong input (dcl t#/v# collision)
- [ ] [#127](https://github.com/sukar0972/vlkn4svga3d/issues/127) — Medium: SM3 non-COLOR/TEXCOORD I/O lands in wrong slot (overwrites TEXCOORD0)
- [ ] [#107](https://github.com/sukar0972/vlkn4svga3d/issues/107) — Medium: Vertex attribute locations disagree (context vs translator)
- [x] [#130](https://github.com/sukar0972/vlkn4svga3d/issues/130) — Medium: Unknown vertex types silently become float4 (size/OOB risk)
- [x] [#124](https://github.com/sukar0972/vlkn4svga3d/issues/124) — Medium: Indexed draw uses stride for VkIndexType, ignores indexWidth
- [x] [#128](https://github.com/sukar0972/vlkn4svga3d/issues/128) — Medium: RSQ and POW skip abs required by D3D9
- [x] [#126](https://github.com/sukar0972/vlkn4svga3d/issues/126) — Medium: SM2 SINCOS (3-source) rejected; only SM3 form accepted
- [ ] [#110](https://github.com/sukar0972/vlkn4svga3d/issues/110) — Low: Int/bool shader consts stored never uploaded
- [ ] [#88](https://github.com/sukar0972/vlkn4svga3d/issues/88) — Low: D3D9 opcode allowlist gaps

## Render-state and fixed-function correctness

- [x] [#157](https://github.com/sukar0972/vlkn4svga3d/issues/157) — Medium: Draw with no RT: framebuffer vs render-pass attachment mismatch
- [x] [#105](https://github.com/sukar0972/vlkn4svga3d/issues/105) — Medium: SETVIEWPORT wipes depth range
- [x] [#117](https://github.com/sukar0972/vlkn4svga3d/issues/117) — Medium: clear() ignores SCISSORTESTENABLE
- [ ] [#101](https://github.com/sukar0972/vlkn4svga3d/issues/101) — High: Depth bias / polygon offset ignored
- [ ] [#100](https://github.com/sukar0972/vlkn4svga3d/issues/100) — High: Point size and point sprites ignored
- [ ] [#103](https://github.com/sukar0972/vlkn4svga3d/issues/103) — Medium-High: Anisotropic filter maps to NEAREST
- [ ] [#102](https://github.com/sukar0972/vlkn4svga3d/issues/102) — Medium-High: sRGB sampled/written without gamma
- [x] [#143](https://github.com/sukar0972/vlkn4svga3d/issues/143) — Medium: MRT blend attachments diverge without independentBlend gate
- [ ] [#144](https://github.com/sukar0972/vlkn4svga3d/issues/144) — Medium: SVGA3D_RS_CLIPPING ignored (depthClampEnable never set)
- [ ] [#145](https://github.com/sukar0972/vlkn4svga3d/issues/145) — Medium: SVGA3D_RS_LINEWIDTH ignored (always 1.0)
- [ ] [#146](https://github.com/sukar0972/vlkn4svga3d/issues/146) — Medium: SVGA3D_TS_TEXTURE_MIPMAP_LEVEL silently dropped
- [ ] [#156](https://github.com/sukar0972/vlkn4svga3d/issues/156) — Medium: FF texture transforms stored but never applied
- [ ] [#99](https://github.com/sukar0972/vlkn4svga3d/issues/99) — Low/Medium: Fixed-function lights/clip planes stored, never applied

## Presentation, adapter policy and cleanup

- [ ] [#135](https://github.com/sukar0972/vlkn4svga3d/issues/135) — Medium: Present/scanout bpp mismatch truncates/pads with no convert
- [ ] [#149](https://github.com/sukar0972/vlkn4svga3d/issues/149) — Medium: blitSurfaceToScreen ignores destScreenId
- [ ] [#150](https://github.com/sukar0972/vlkn4svga3d/issues/150) — Medium: BLIT_GMRFB_TO_SCREEN only FRAMEBUFFER + 32bpp/24-depth
- [ ] [#109](https://github.com/sukar0972/vlkn4svga3d/issues/109) — Low: Surface-to-screen blit crops instead of scales
- [ ] [#108](https://github.com/sukar0972/vlkn4svga3d/issues/108) — Low: Stretch blit drops face/mip; same-image overlap; no MSAA resolve
- [ ] [#86](https://github.com/sukar0972/vlkn4svga3d/issues/86) — Medium: Partial DMA still barriers all mip levels / array layers
- [ ] [#85](https://github.com/sukar0972/vlkn4svga3d/issues/85) — Medium: Present CPU readback cache is write-only dead API
- [ ] [#141](https://github.com/sukar0972/vlkn4svga3d/issues/141) — Medium: Guest-specific magic IDs (cid 246 / sid 73 / hand) hardcoded in core
- [ ] [#154](https://github.com/sukar0972/vlkn4svga3d/issues/154) — Medium: Preload binary-patch policy not keyed to allowlist entries
- [ ] [#136](https://github.com/sukar0972/vlkn4svga3d/issues/136) — Medium: Preload advertises HWVERSION 0x00020001 as WS65_B1 (actually WS8_B1)
- [ ] [#112](https://github.com/sukar0972/vlkn4svga3d/issues/112) — Low: Wrong capability bit comments; dead collectPendingWindowPresents; SCREEN_TO_GMRFB no-op
- [ ] [#111](https://github.com/sukar0972/vlkn4svga3d/issues/111) — Low: README documents removed SVGA3_VLKN_LEGACY_CLIENT_PRESENT

