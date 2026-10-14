# Oracle framebuffer-alpha test diagnosis — 2026-10-09

The extra `fbo-blending-format-quirks` failure is caused by an uninitialized
attachment in the packaged Piglit fixture. It is not evidence of a renderer
blending regression. The renderer and corrected guest Mesa module were unchanged
for these controls.

The Debian Piglit source used by both guests is
`0~git20220119-124bca3c9-1`. In `tests/fbo/fbo-blending-format-quirks.c`,
`test_formats()` creates two textures with `glTexImage2D(..., NULL)`. Its
initialization loop runs twice but always selects `draw_bufs[0]` and uses
`expect[0][3]`. It never clears attachment 1. The alpha-swizzle variant then
uses `GL_DST_ALPHA` and expects attachment 1 to contain zero. Texture allocation
with a null data pointer does not initialize its contents; see the
[Khronos OpenGL reference](https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/glTexImage2D.xml).
The source archive is retained locally from
[Debian](https://deb.debian.org/debian/pool/main/p/piglit/piglit_0~git20220119-124bca3c9.orig.tar.xz).

A diagnostic interposer calls the original `piglit_draw_rect()` helper in the
unmodified installed binary. It observes attachment 1 immediately before the
draw, or explicitly clears it before the second case. The adapter, Mesa module,
blend factors and expected probes stay unchanged. A zero-clear control tests
the intended input; a 0.75-clear control tests whether the same failure appears
when destination alpha is deliberately nonzero.

| Path | Original isolated run | Explicit zero | Explicit 0.75 |
| --- | --- | --- | --- |
| VM119 SVGA | Pass | Pass | Fail, alpha 0.749020 |
| VM119 guest llvmpipe | Pass | Pass | Fail, alpha 0.749020 |
| Oracle SVGA with host lavapipe | Pass | Pass | Fail, alpha 0.749020 |
| Oracle guest llvmpipe | Pass | Pass | Fail, alpha 0.749020 |

On Oracle SVGA, the observation-only run failed with alpha 0.125490. The
pre-draw observation was also 0.125490. The zero-clear run observed that same
initial alpha, cleared it, and passed all four test variants. The previous full
replays observed 0.741176; an isolated original run passed. These differing
values are consistent with reuse of undefined allocation contents, rather than
a deterministic incorrect blend equation. Guest llvmpipe and VM119 happened
to start with zero alpha in the observation controls.

The proposed fixture repair indexes both the draw buffer and expected alpha
with `i` in the initialization loop. A reviewable diff is retained as
`artifacts/oracle-alpha-diagnosis-20261009/fbo-blending-init.patch`. This
investigation validates the diagnostic zero-clear control; it has not rebuilt
that proposed upstream fixture patch or substituted it into the graphics gate.
The raw gate results remain 139 passes, two failures and five skips on SVGA.
The existing depth/stencil API-expectation mismatch is a separate documented
fixture issue. No failures were waived and no GitHub enforcement was activated.

Raw logs, commands, interposer source/binary and downloaded source are in
`artifacts/oracle-alpha-diagnosis-20261009/`. Oracle controls used the verified
adapter `26d2ca757188ff392e77e0fee9496114f4e6936674d58586be6fa4430f0e53df`
and guest Mesa module
`88a1b2409b87b946843133ff28b1c0cf382dd21436b69e9a3507ce8ecaa7381a`.
The development guest was temporarily stopped under the operator's existing
authorization for the targeted Oracle runs. Cleanup and restoration records
are retained with the Oracle evidence.

The [October 10 fixture validation](piglit-fixture-validation-20261010.md)
records the subsequent rebuild of both versioned repairs and their installation
in disposable guest overlays. The results above retain the original October 9
diagnosis and unmodified-test controls.
