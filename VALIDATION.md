# Validation

## Guest accuracy — 2026-10-04

[PR #27](https://github.com/sukar0972/vlkn4svga3d/pull/27) completed the selected 146-case Piglit replay on Debian 12 VM119 with patched Mesa 22.3.6. The tested renderer source was `384081f569dd237b26ad8056e368b4c3de5ebda9`; these results do not validate later renderer changes.

| Path | Pass | Fail | Skip | Timeout |
| --- | ---: | ---: | ---: | ---: |
| SVGA3D-to-Vulkan | 140 | 1 | 5 | 0 |
| Unchanged guest llvmpipe | 146 | 0 | 0 | 0 |

Counts are top-level cases. All recorded pixel checks passed; the five skips reflect unsupported capabilities. The one raw failure is an older Piglit API error expectation: invalid unsized cube storage returns `GL_INVALID_ENUM` where the test expects `GL_INVALID_VALUE`. It also fails on software with matching GL 2.1 capabilities. An optional expectation patch passes on both paths; the table retains the unmodified result.

The replay used GLX, separate processes, per-case graphics health probes, Vulkan validation, and a 600-second timeout. It needed **zero recoveries** and reported **zero Vulkan validation errors or warnings**. The custom guest driver used `SVGA_VLKN_EXTENDED_STATE=1`; the software reference cleared the custom driver options.

An additional 29-case framebuffer comparison recorded **13 hardware passes, 16 skips, and zero failures**, including 154 passing hardware subtests. Independent controls passed depth/stencil copies in all four framebuffer orientations, overlapping copies, and unchanged texture state after rejected storage.

See the [full accuracy and performance report](docs/piglit-accuracy-20261004.md) for build hashes, methods, the remaining API failure, and raw evidence locations. See [guest Mesa setup](tools/mesa-guest/README.md) to reproduce the driver configuration.

## Host checks

For the tested accuracy build, all 14 host harness suites passed: 11 configured for lavapipe and three ICD-free suites. Mock/preload checks and renderer-source CI also passed. Coverage includes attachment-copy pixels, point size, and occlusion queries. The QEMU integration executable exercises an in-process device harness; it does not boot a guest.

The September 25 publication snapshot also passed `make all`, `make test`, and `make acceptance`; compiler warnings remained. Its transcripts are in `validation/`. The [testing guide](docs/testing.md) explains the current targets and dependencies.

## Guest performance — 2026-10-04

The final accuracy build ran the full 33-scene glmark2 suite at 800×600 with validation disabled, vblank off, a 0.5-second warm-up, and three one-second-per-scene runs. With a 1100 MHz GPU minimum, CPU weight 1000, and separate vCPU affinity, median scores were **443 with VM105 running** and **463 with it briefly paused**. A Jellyfin transcode remained active in both conditions.

Glmark validation reported **27 Success, 6 Unknown, and zero Failure**; unknown results and unsupported terrain are not correctness passes. The 500 target remained unmet. Historical scores of 556 and 532 lack matching background-load records and do not establish a performance change against this build. Full profiles and evidence are in the [detailed report](docs/piglit-accuracy-20261004.md#performance-comparison).

## Limits and earlier results

The guest exposes OpenGL 2.1 / GLSL 1.20. Selected-case success does not establish complete conformance, modern OpenGL support, broad guest or hardware compatibility, or uninterrupted multi-hour stability. DX rendering remains unsupported. Modern Minecraft required guest llvmpipe; accelerated modern-Minecraft support was not established.

Earlier October 3–4 replays recorded 64 passes / 70 failures / 5 skips / 7 timeouts before PR #26, then 85 / 49 / 5 / 7 after it. Five GPU hangs required recovery in those runs. The October 4 accuracy replay above supersedes those results for its tested build and configuration. Earlier logs remain in `artifacts/pr-review-20261003/` and `artifacts/piglit-accuracy-final/`; current raw evidence is in `artifacts/full-accuracy/`. These artifact directories are excluded from Git.
