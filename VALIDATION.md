# Validation

## Piglit fixture controls — 2026-10-10

The two versioned Piglit fixture repairs passed two fresh-overlay controls of
the pinned release adapter and corrected guest Mesa on Oracle, with Vulkan
validation enabled. Each control retained all 146 selected cases: SVGA recorded
141 passes and five existing skips; guest llvmpipe recorded 146 passes. No
failures, crashes or timeouts occurred. Conditions matched and cleanup passed.
The independent Debian 12 fixture build and 36 Python tests also passed.

See the [fixture report](docs/piglit-fixture-validation-20261010.md) for hashes
and evidence. This validates the focused selection with repaired fixtures.
It does not validate new source builds or performance, and the GitHub graphics
gate remains unactivated. Nested-worker startup remains an infrastructure
blocker.

## Guest accuracy — 2026-10-07

The retained workspace on `0a09f02`, including depth bias, gamma, relative
constants, robust buffer fetches and device-loss handling, completed the selected
146-case replay with release adapter `26d2ca75` and corrected guest Mesa
`88a1b240`. This exact pair recorded:

| Path | Pass | Fail | Skip | Timeout |
| --- | ---: | ---: | ---: | ---: |
| Current SVGA3D-to-Vulkan workspace | 140 | 1 | 5 | 0 |

There were **zero new regressions, zero recoveries, zero Vulkan validation
errors or warnings, and zero kernel GPU hangs**. The raw failure is the dated
unsized depth/stencil cube-storage API expectation described below; it has no
failed pixel probes, and the separate corrected-expectation control passes.
The original adapter and vCPU affinity were restored after testing; the default
guest driver remained unchanged.

The replay exposed two integration errors after unsupported floating `Z_DF24`
was disabled: Mesa omitted shader shadow comparisons for legacy depth formats,
and its comparison-only assumption blocked depth-copy blits. The final opt-in
patch retains shader comparisons and permits raw-depth blits without changing
the depth formats. All nine focused depth/shadow/framebuffer cases and the full
replay passed their pixel checks. **The updated guest Mesa patch is required**;
the old driver fails the 1D shadow case with this adapter. A comparison-only
repair passed shadows but failed `texdepth`; an earlier integer-format fallback
passed correctness but lost 2.27% in its matched benchmark. Both were superseded.

The final matched driver benchmark recorded **435 before → 432.5 after**
(−0.57%, overlapping ranges). It establishes neither a reliable speedup nor
a stable small regression; **500+ remains unmet** under the tested background load.

`make all`, `make test`, the release build and all 14 rebuilt host harness
suites passed. New packed-depth pixel checks cover D24S8_INT, D24X8 and D24S8.
Optional `spirv-val` was unavailable. The [retained-fixes report](docs/renderer-performance-20261007.md)
records full hashes, benchmarks, rejected experiments and raw evidence under
`artifacts/performance-preserve-20261007/`, including the final
`full-validation-raw-depth-blit/` and `raw-depth-blit-driver-benchmark/` runs.

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

The October 6 fixes for issues #100, #101, #102, #113 and #162 passed
`make all`, `make test`, and all 14 host harness suites. The translator suite
used `spirv-val`; the renderer suites used lavapipe. Pixel regressions cover
D16/D24 depth bias, sRGB sampling/output, opaque texture alpha, relative
constants through a0/aL with runtime and DEF values, out-of-range indices,
and point coverage clamped to the advertised size of one. Device-loss
injection covers begin, end, submit, queue wait and device wait; wrapped FIFO
checks show that 3D is dropped while updates, screen objects and fences drain.

Logs are in `artifacts/open-issues-20261006/`. The October 7 selected guest
replay above now covers these changes. Neither replay reproduced the earlier
GPU hangs or validates real GPU-fault recovery or continuous-run stability. Real GPU-fault reproduction, device recreation and sustained-load
validation remain deferred. On device loss, Vulkan allocations are retained
until process exit so teardown cannot free resources whose completion is
unknown.

For the tested accuracy build, all 14 host harness suites passed: 11 configured for lavapipe and three ICD-free suites. Mock/preload checks and renderer-source CI also passed. Coverage includes attachment-copy pixels, point size, and occlusion queries. The QEMU integration executable exercises an in-process device harness; it does not boot a guest.

The September 25 publication snapshot also passed `make all`, `make test`, and `make acceptance`; compiler warnings remained. Its transcripts are in `validation/`. The [testing guide](docs/testing.md) explains the current targets and dependencies.

## Depth-bias performance — 2026-10-07

A matched before/after/after/before glmark2 comparison on VM119 recorded medians
of **298.5 before** and **301 after** depth-bias support (+0.84%). Four timed runs
per build had overlapping ranges; the comparison does not establish a reliable
speedup or a small regression. Vulkan validation was enabled by the debug-build
default, despite the initial report saying otherwise; these are not release
throughput measurements. Both builds reported 27 glmark2 validation successes,
six unknowns, and zero failures.

Both isolated adapters retained the installed lab's hardware-version value
`0x20001`: the unmodified current value `0x20000` disables SVGA direct rendering
in this guest. This measures the depth-bias difference under a common
compatibility control, not an unmodified production deployment. The
[benchmark report](docs/depth-bias-benchmark-20261007.md) records conditions,
source differences, hashes, and evidence.

## Release performance and constant reuse — 2026-10-07

With all correctness fixes retained, a release before/after/before comparison
recorded glmark2 medians of **442 before** and **439 after** optional image format
lists and submission-epoch-safe programmable constant reuse (−0.68%). This is
a small measured decrease, not evidence of a whole-suite speedup. The 500
target remains unmet. Both builds recorded 27 glmark2 validation successes,
six unknowns and zero failures. The common lab hardware-version adjustment
described above was required in both builds.

A focused local lavapipe workload of unchanged programmable constants improved
from **50.17 to 3.35 µs/draw** across ten alternating pairs, with all pixels
verified. This synthetic result is not guest FPS. All 14 host harness suites,
`make test`, and the separate release build passed; the new constant-epoch
pixel regression fails when its safety invalidation is removed. Optional
`spirv-val` was unavailable. That initial comparison did not repeat Piglit;
the later October 7 replay above validates the retained workspace with the
corrected guest driver.
See the [release report](docs/renderer-performance-20261007.md) for conditions,
hashes and evidence, including the correction to earlier validation-enabled
timings.

The [benchmark history audit](docs/benchmark-history-audit-20261007.md) reviews
the available Codex threads, lists the earlier 556-era measurements and confirmed
regressions, and identifies the unmeasured October 5 integration changes. The
remaining throughput gap has not been isolated to a commit.

## Guest performance — 2026-10-04

The final accuracy build ran the full 33-scene glmark2 suite at 800×600 with validation disabled, vblank off, a 0.5-second warm-up, and three one-second-per-scene runs. With a 1100 MHz GPU minimum, CPU weight 1000, and separate vCPU affinity, median scores were **443 with VM105 running** and **463 with it briefly paused**. A Jellyfin transcode remained active in both conditions.

Glmark validation reported **27 Success, 6 Unknown, and zero Failure**; unknown results and unsupported terrain are not correctness passes. The 500 target remained unmet. Historical scores of 556 and 532 lack matching background-load records and do not establish a performance change against this build. Full profiles and evidence are in the [detailed report](docs/piglit-accuracy-20261004.md#performance-comparison).

## Limits and earlier results

The guest exposes OpenGL 2.1 / GLSL 1.20. Selected-case success does not establish complete conformance, modern OpenGL support, broad guest or hardware compatibility, or uninterrupted multi-hour stability. DX rendering remains unsupported. Modern Minecraft required guest llvmpipe; accelerated modern-Minecraft support was not established.

Earlier October 3–4 replays recorded 64 passes / 70 failures / 5 skips / 7 timeouts before PR #26, then 85 / 49 / 5 / 7 after it. Five GPU hangs required recovery in those runs. The October 4 accuracy replay above supersedes those results for its tested build and configuration. Earlier logs remain in `artifacts/pr-review-20261003/` and `artifacts/piglit-accuracy-final/`; current raw evidence is in `artifacts/full-accuracy/`. These artifact directories are excluded from Git.
