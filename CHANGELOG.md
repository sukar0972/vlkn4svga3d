# Changes

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
