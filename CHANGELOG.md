# Changes

## Source layout cleanup

Moved private renderer headers to `include/internal/`, mock headers to `shim/include/mock/`, and reference helper headers to `tools/include/`. Updated build dependencies and include paths, documented the layout, and added `.editorconfig`. No rendering behavior changes are intended. Build, basic tests, and the acceptance suite passed after the reorganization.

## 2026-09-25 — Shader and texture integration updates

- Added shader destination saturation (`_SAT`) and vertex color output clamping.
- Added saturation translation and execution tests.
- Added fallback vertex/index buffers and white textures for missing bindings. These fallback semantics still need broader compatibility review.
- Updated mip-level image views, texture binding, surface transfer classification, and resource teardown.
- Added further draw diagnostics.

Recorded live screenshots show Minecraft terrain and the textured hand. The hand fix also depended on changes to the separate GL4ES project; those changes are not part of this renderer repository. This is not a claim that the renderer alone fixes the entire guest graphics stack.

A 15-second live sample on September 25 measured the vCPU thread at 90.45% CPU, QEMU main thread at 4.87%, and the Intel GPU render engine at 0.879% busy. This suggests a CPU-side bottleneck, but does not isolate Java execution from emulation or graphics synchronization overhead. It is not an FPS benchmark.

## 2026-09-24 — PlayBook integration development snapshot

The project owner reports Minecraft now reaches an in-game world in the PlayBook guest. This report supersedes the earlier black-screen observation as the latest user-visible result. It is not a claim of complete SVGA3D support or independent verification of the active renderer.

This snapshot imports development changes in:

- Texture/DMA transfer handling, including compressed and linear transfers.
- Surface redefinition and invalidation of dependent context resources.
- Framebuffer and texture binding caches and render-pass lifecycle handling.
- Fixed-function texture rendering and culling adjustments.
- QEMU rectangle fill/copy handling and framebuffer-to-surface synchronization.
- Preload linking with the full static library and static C++ runtime.
- The duplicate-surface test, now expecting replacement rather than rejection.

Some integration paths remain guest-specific. Correctness across guests, hardware drivers, and protocol features is still unverified. See VALIDATION.md for the actual results of the snapshot checks.
