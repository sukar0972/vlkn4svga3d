# Validation

## Piglit accuracy fixes — 2026-10-03; continuation 2026-10-04

PR #26 fixes four rendering errors exposed by Piglit:

- Packed guest blend color was never supplied to Vulkan. Blend constants now record per draw, including queued draws with different colors and separate alpha factors.
- SVGA's clockwise front-face convention was reversed. Front-facing shader values and two-sided stencil selection now use that convention while the culled triangle orientation stays consistent.
- COLOR2/COLOR3 back-face interpolants were aliased to COLOR0. They now have independent shader locations without displacing texture coordinates.
- Packed D24 transfers omitted stencil and used the wrong depth bit layout. Depth and stencil now copy through separate Vulkan planes; D24X8 converts packed normalized depth to/from the Vulkan float format.

Wire values and guest layouts were checked against the [Mesa 22.3.6 source](https://archive.mesa3d.org/older-versions/22.x/mesa-22.3.6.tar.xz). Real-Vulkan pixel tests reproduced the missing blend constants, aliased back colors and packed readback failures before the fixes. New tests cover both windings, stencil operations, changing blend constants, color/alpha factors, pitched partial depth transfers and direct staging readback. All 14 host suites passed with lavapipe and validation, as did `make test` and all seven reporting tests.

The replay completed 141 of the same 146 cases before the coordinator stopped. On October 4, both Proxmox `10.0.0.200` and VM119 `10.0.0.144` were unreachable; the local gateway remained reachable. The remaining five cases and matched benchmark are pending. PR #26 remains a draft.

| Outcome | Before, matched 141 cases | Accuracy fixes, 141 cases |
| --- | ---: | ---: |
| Pass | 63 | 82 |
| Fail | 67 | 48 |
| Skip | 5 | 5 |
| Timeout | 6 | 6 |

Nineteen former failures now pass: thirteen enabled two-sided vertex color combinations, two front-facing shader checks, two packed depth/stencil readbacks, stencil drawpixels and two-sided stencil. All 63 previous passes in this subset still pass. The separate `gl-1.0-blend-func` test remains failing, but failed probes fell from 1,043 to 388. The unchecked cases are `texsubimage`, `texsubimage-unpack`, `texsubimage-depth-formats`, `fbo-copyteximage-simple` and packed depth/stencil `texsubimage`. These partial counts must not be compared as a completed 146-case result.

Each exact Piglit binary and argument list was replayed directly with GLX, the runner's clean environment and a 60-second timeout. A small graphics health probe followed each case unless a kernel GPU hang had already been detected. Four GPU hangs required recovery through the completed subset; two cube mip-level timeouts retained graphics health. GPU hangs remain unresolved, and separate invocations with recovery do not establish continuous-run stability. The preceding llvmpipe reference passed all 146 cases and 309 subtests; a fresh reference and benchmark have not yet run for these fixes.

The installed release adapter was `6df0d2d780520849238dcc39bba8fa04a4087aa41415b524debe34d5a474ed11`, built from runtime source `5b6f7f6e9de33169b87e48d9713e647aff2b35fc`. Evidence is preserved in `artifacts/piglit-accuracy-final/`, including raw per-case outputs, commands, health probes, renderer identities and recovery logs. Host logs are `artifacts/piglit-accuracy-harness.log`, `piglit-accuracy-mock.log` and `piglit-accuracy-python.log`. These artifacts are excluded from Git. Current VM health cannot be verified while the host is unreachable.

## PR review and matched VM benchmarks — 2026-10-03

Reviewed PRs #18, #22, #23, #24 and #25 on the combined source. Fixed staging-wrap command ordering and context initialization, corrected VMware DX opcodes and isolated context bookkeeping, tested copied host-adapter tables and RAM boundaries, removed stale QEMU members from the core archive, and tested the explicit preload build fence. DX rendering remains unsupported.

A fresh combined build passed all 14 host harness suites on lavapipe with SPIRV-Tools, `make test` (including 3,635 mock assertions), and all seven Piglit reporting tests. The final release adapter matched the binary installed in VM119: `9270408e8e3cf467481a6163652ebde3ca6d70ed11376a98b4d8c69a4bbcc46e`.

Matched glmark2 checks used an 800×600 window, `vblank_mode=0`, a 0.5-second warmup per scene, and three runs with one-second scenes. The baseline scores were 549/556/556; final scores were 558/556/555. Both medians were **556**. Each validation run reported 27 successes and six unknown results; terrain lacked vertex texture fetch support. These short runs show no measured performance regression, but unknown results are not pixel correctness passes and this is not a sustained-load benchmark.

Build, test and benchmark logs are in `artifacts/pr-review-20261003/`, including `final-harness.log`, `final-mock.log`, `final-python.log` and `vm-bench/`. These artifacts are excluded from Git.

## Final isolated Piglit comparison — 2026-10-03

The combined renderer was tested against the same 146-case quick-profile selection, GLX backend and 60-second per-case timeout as the baseline below. The guest package versions stayed unchanged. The installed renderer hash matched the release adapter above throughout all six execution epochs. Each hardware case ran in a separate Piglit invocation followed by a small glmark2 health probe unless a new kernel GPU hang had already been detected. A lost device triggered VM recovery before the next case.

| Outcome | SVGA3D | Guest llvmpipe |
| --- | ---: | ---: |
| Pass | 64 | 146 |
| Fail | 70 | 0 |
| Skip | 5 | 0 |
| Timeout | 7 | 0 |
| Not run | 0 | 0 |

These are top-level cases. The llvmpipe reference also reported 309 passing subtests. All 53 cases that passed in the earlier baseline still passed. The first 112 hardware outcomes matched that baseline; newly reached cases expose additional failures and are not evidence of new regressions.

Five cases reproduced kernel i915 GPU hangs and required VM recovery: `fbo-alphatest-formats`, `fbo-blending-formats`, `fbo-colormask-formats`, `copyteximage 2d` and `texsubimage`. The other two timeouts were cube mip-level selection checks whose subsequent health probes passed. Hardware correctness and graphics stability remain incomplete. Separate invocations and recovery epochs allow independent coverage of the whole plan; they do not establish continuous-run stability.

The evidence coordinator initially stopped after recovery because the guest cleared its temporary directory on reboot. It was corrected to recreate that directory and resume from preserved local results. The manifest retains this interruption; completed raw Piglit results were not rewritten or rerun.

Evidence is in `artifacts/pr-review-20261003/piglit-final-isolated/` (filterable HTML report, exact plan, raw per-case results, health probes, renderer identity and kernel logs) and `piglit-final-reference/` (complete llvmpipe reference). VM119 was left running with graphics health restored; Minecraft remained closed after its world was saved.

## Piglit guest baseline — 2026-10-03

Added `make test-piglit`, an SSH/local runner, JSON/HTML evidence, and CI checks for outcome reporting. Seven reporting tests passed. A live interruption check confirmed that an interrupted SVGA run still attempts the llvmpipe reference, retains incomplete results, and exits unsuccessfully.

The VM setup was recovered from T3 thread `13096e80-1a96-4c91-81d2-6975166246af`: Proxmox `10.0.0.200`, VM119, Debian guest `svga3d@10.0.0.144`, Xorg `:0`. The loaded host library hash was `373096a13e9ff1b03ef54b753970374ab79902b77e1abe789dadd320714fe1a5`. The guest used Mesa 22.3.6 and Debian Piglit `0~git20220119-124bca3c9-1`.

Both paths used the same 146-case selection and 60-second per-test timeout. Counts below are top-level cases, not upstream subtest totals:

| Outcome | SVGA3D | Guest llvmpipe |
| --- | ---: | ---: |
| Pass | 53 | 146 |
| Fail | 54 | 0 |
| Skip | 3 | 0 |
| Timeout before device loss | 2 | 0 |
| Timeout/incomplete after device loss | 4 | 0 |
| Not run after interruption | 30 | 0 |

**The accelerated run did not complete.** Before device loss, 112 cases completed with the first four outcomes above. Pixel/API failures include blending, stencil/scissor, framebuffer, and mip-selection checks. Three skips reflect capability differences and are not correctness passes. Subsequent timeouts cannot be treated as independent rendering failures.

Clean restarts reproduced Intel i915 GPU hangs while `copyteximage 2d`, `texsubimage`, and framebuffer format cases were executing. QEMU then reported `VK_ERROR_DEVICE_LOST`; a small glmark2 probe also stalled. Kernel and renderer logs are retained. No renderer fixes are claimed by this testing change. Minecraft was closed normally and its log confirmed that all dimensions finished saving before VM recovery.

Evidence is in `artifacts/piglit-vm119-comparison-20261003/`, with interrupted exploratory runs in the other `artifacts/piglit-*` directories. The combined comparison records interrupted/unrun cases separately, and includes raw Piglit checkpoints, the exact test list, renderer identity, package versions, kernel logs, and an HTML report. These artifacts are excluded from Git.

## Renderer and scanout changes — 2026-10-03

`make -j4 all`, all 12 harness suites, and `make -j4 test` passed on the final source changes. The harness used `/usr/share/vulkan/icd.d/lvp_icd.json` and SPIRV-Tools v2025.1. The no-ICD translator tests ran `spirv-val` successfully rather than skipping it. Rendering tests check Vulkan validation through resource and device teardown.

Added regressions cover alpha comparisons and discarded-fragment depth/stencil behavior, buffer overwrite ordering and retirement accounting, cached framebuffer invalidation, and screen backing-store offsets, bounds, padded pitch, define/destroy packets, and display notifications.

Earlier live VM119 checks completed three glmark2 runs with scores 545, 533, and 544. Checkable glmark2 scenes reported validation success; scenes reporting `Unknown` are not correctness passes. In the matched Minecraft 1.8.9 scene, five F3 readings averaged 37.6 FPS for the current renderer versus 31.4 for the saved old binary. The old binary also showed rendering corruption, its source identity is unknown, and the comparison was not repeated. These readings do not establish equal rendering work or isolate optimization effects.

Fabulously Optimized 8.0.1 / Minecraft 1.21.4 failed to create a GLX core context on the guest SVGA driver, which reports OpenGL 2.1. It subsequently launched using guest CPU rendering (`llvmpipe`) via an instance-only Prism wrapper. That workaround is separate from these source changes and does not establish accelerated modern-Minecraft support.

Local build/test evidence is in `artifacts/pr-merge-20261003/` and `.harness-loop/iter-01/`; live evidence is in `artifacts/glmark2-ab-20261003/` and `artifacts/minecraft-ab-20261003/`. These directories are excluded from Git.

## Publication checks — 2026-09-25

A separate source snapshot was built without using the development directory's existing object files.

| Check | Result |
| --- | --- |
| `make -j4 all` | Passed; compiler warnings remain |
| `make test` | Passed, including 3,407 mock-engine assertions |
| `make acceptance` | Passed all stages of the existing runner |

The real-Vulkan driver check identified Mesa llvmpipe (LLVM 20.1.2, 256 bits), Vulkan API 1.4.318, software rendering, mock fallback disabled, and validation layers enabled. That check reported zero validation errors and warnings.

These results describe the current tests, not an independent certification of their coverage. The QEMU integration executable is an in-process device harness, not a booted guest. No real VM was booted or modified during publication. The project owner reported entering a Minecraft world on September 24. Recorded screenshots inspected afterward show terrain and a textured hand. A live September 25 check identified the Intel HD Graphics 630 Vulkan driver and nonzero GPU engine activity. Full visual correctness and cold-boot reliability remain unverified. The fresh test runs above use local software Vulkan, not the live guest. Hardware-GPU performance and broad guest compatibility remain unverified.

Build and test transcripts from this snapshot are in `validation/`.
