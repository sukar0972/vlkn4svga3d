# VM119 Piglit accuracy and performance, 4 October 2026

The corrected SVGA path passes every pixel check in the recorded selection. The raw 146-case replay has one API error-expectation failure, five unsupported cases, and no hangs. Performance depends strongly on host CPU contention and the GPU clock profile; the measurements below keep those conditions explicit.

## Verified build and environment

Backend source: `384081f569dd237b26ad8056e368b4c3de5ebda9`. Subsequent report/session-configuration commits do not change the tested renderer.

| Component | SHA-256 |
| --- | --- |
| Loaded host adapter, `/usr/local/lib/libqemu_svga3d.so` | `dbba46d242f38cee8640e623d9e66658d7c7734194dc1c22ae254db4c953c857` |
| Guest custom `vmwgfx_dri.so` | `2c4bb2130da43a1e6f81904ef361538349025e15a287200170cccb0dfb0de43f` |
| Unchanged system software module | `f726189443dcc525759de95162b437223cf76403d492c87288cadb7e28923d8f` |

Proxmox is `10.0.0.200`; Debian 12 VM119 is `10.0.0.144`, with two host-type vCPUs, 4 GiB RAM, and 128 MiB VMware graphics memory. Mesa 22.3.6 exposes OpenGL 2.1 / GLSL 1.20. Actual QEMU mappings, driver loader output, hashes, and session environment are retained locally under `artifacts/full-accuracy/`.

The guest desktop now selects the corrected driver through `~/.xsessionrc`, with `SVGA_VLKN_EXTENDED_STATE=1` and `LIBGL_DRIVERS_PATH=$HOME/vlkn-mesa/dri:/usr/lib/x86_64-linux-gnu/dri`. The active XFCE session exports were verified after reboot. The system DRI directory remains available for software rendering; software comparisons clear both variables. See [guest build and setup](../tools/mesa-guest/README.md).

## Accuracy results

The replay uses the exact 146 previously recorded binaries and arguments, GLX, separate processes, per-case graphics health probes, and Vulkan validation. The final timeout is 600 seconds: a healthy, probe-heavy cube texture case takes about 256 seconds under contention. Raw results retain failures and skips.

| Recorded 146-case selection | Pass | Fail | Skip | Timeout |
| --- | ---: | ---: | ---: | ---: |
| Initial hardware baseline | 64 | 70 | 5 | 7 |
| PR26 hardware | 85 | 49 | 5 | 7 |
| Intermediate accuracy build `1432f95` | 118 | 23 | 5 | 0 |
| Final hardware `384081f` | 140 | 1 | 5 | 0 |
| Fresh unchanged llvmpipe reference | 146 | 0 | 0 | 0 |

The final replay required zero recoveries and retained all 85 passes from PR26 and all 118 from the intermediate build. All eight shadow sampler variants, projected samplers, packed depth/stencil copies, blending, and color-mask pixel checks pass. Vulkan validation reported **zero errors and zero warnings** throughout the final replay and expanded framebuffer comparison.

The remaining raw failure is `spec@ext_packed_depth_stencil@depth_stencil texture`: the dated test expects `GL_INVALID_VALUE` for invalid unsized cube storage, while Mesa returns the also-applicable `GL_INVALID_ENUM`. It reproduces on the unchanged software driver when matched to GL 2.1 without `GL_EXT_gpu_shader4`. [ARB_texture_storage](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_texture_storage.txt) specifies `INVALID_ENUM` for unsized internal formats; [OpenGL error semantics](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf) permit any applicable error when several conditions hold. The optional [Piglit expectation patch](../patches/piglit-20220119-depth-stencil-error.patch) changes only this call and passes on both hardware and matched software. It waives no pixel checks. Unmodified results remain the primary counts above.

An additional paired 29-case framebuffer selection has **13 hardware passes, 16 skips, and zero failures**, with **154 passing hardware subtests**. Software passes all 29 top-level cases, with 249 passing and four skipped subtests. Previously failing RGB12/RGB16 and RGB16F/RGB32F blending checks now pass. Unsupported extensions remain visible as skips.

Independent guest controls pass asymmetric depth/stencil values through all four framebuffer orientations, an overlapping packed copy, and unchanged texture state after rejected storage. They pass on SVGA and on both native and matched-GL-2.1 llvmpipe. All 14 host harness suites (11 configured for lavapipe and three ICD-free suites) and mock/preload checks pass, including repeated sampled-attachment snapshots, a three-pixel point's nine covered pixels, and an exact nine-sample occlusion query. CI passes for the tested renderer commit.

These results cover the selected legacy API and pixel cases. They do not establish modern OpenGL support, complete API conformance, or uninterrupted multi-hour application stability.

## Performance comparison

Release adapter, Vulkan validation disabled; full 33-scene glmark2 2023.01 suite, 800×600, vblank off, 0.5-second warm-up per scene, then three runs at one second per scene. Unsupported terrain stays in the suite. VM105's QMP state is checked before and after every run. Brief pauses use an automatic four-minute resume timer and a final resume check.

| GPU minimum | VM119 CPU weight | VM105 | Scores | Median |
| --- | ---: | --- | --- | ---: |
| 350 MHz (original minimum) | 100 | Running | 176 / 180 / 173 | 176 |
| 350 MHz | 100 | Briefly paused | 267 / 269 / 267 | 267 |
| 1100 MHz | 100 | Running | 230 / 231 / 231 | 231 |
| 1100 MHz | 100 | Briefly paused | 465 / 455 / 466 | 465 |
| 1100 MHz | 1000 (temporary) | Running | 363 / 354 / 363 | 363 |
| 1100 MHz | 1000 (temporary) | Briefly paused | 478 / 471 / 465 | 471 |

The same protocol with VM119 vCPU 0 pinned to physical core 2 and vCPU 1 to core 3, CPU weight 1000, and the 1100 MHz GPU minimum produced:

| VM105 | Scores | Median |
| --- | --- | ---: |
| Running | 443 / 450 / 435 | 443 |
| Briefly paused | 462 / 463 / 472 | 463 |

The earlier single-run affinity probe scored 433 without a separate warm-up. Experimental CPU weights and affinity were restored after each experiment. The **500 target remains unmet** in these final-build comparisons. Pinning improves performance under contention but does not improve the paused result. A single exploratory run at CPU weight 10000 scored 452 with VM105 running and no separate warm-up, so that extreme share was not retained. All benchmark comparisons have completed, and VM105 is confirmed running.

The host also has an active Jellyfin transcode using approximately one CPU core. It was left running. VM105 uses approximately two cores while running. Consequently the paused comparison still contains substantial unrelated host load. A historical median of 556 and an intermediate-build median of 532 did not fully record this background load; they are not controlled before/after comparisons against the final build.

Final glmark validation: **27 Success, 6 Unknown, zero Failure**. Unknown cases and unsupported terrain are not correctness passes. The deployed profile retains CPU weight 1000 and separate vCPU affinity (cores 2 and 3), matching the repeated 443-median running-server comparison. Proxmox VM119 now records `cpuunits: 1000`. Per-vCPU affinity and the 1100 MHz GPU minimum are runtime settings; affinity must be reapplied after QEMU restarts, and no persistent clock service is installed. VM105 remains running; its configuration was not changed. The remaining shortfall requires further investigation under matched background load before claiming the original 500 performance has been restored.

## Local evidence

Raw evidence is retained in the workspace rather than committed as large test artifacts:

- `artifacts/full-accuracy/full-replay-384081f/`: exact hardware plan, logs, raw outcomes, loader identity, independent controls, and optional corrected API case.
- `artifacts/full-accuracy/software-replay-20b8bb3/`: unchanged system software reference.
- `artifacts/full-accuracy/final-framebuffer-comparison-384081f/`: paired framebuffer results and HTML report.
- `artifacts/full-accuracy/final-verification-384081f/`: full-replay/expanded validation captures and original-clock benchmark.
- `artifacts/full-accuracy/clock1100-performance-384081f/`: higher-clock benchmark and resume evidence.
- `artifacts/full-accuracy/priority1100-performance-384081f/`: temporary CPU-share comparison.
- `artifacts/full-accuracy/affinity1100-performance-384081f/`: repeated vCPU-affinity comparison.
- `artifacts/full-accuracy/priority10000-probe-384081f/`: single higher-share probe.
- `artifacts/full-accuracy/final-production-profile.txt`: deployed CPU/GPU profile, loaded adapter, validation mode, and both VM running states.
- `artifacts/full-accuracy/desktop-driver-environment.txt`: actual active desktop environment.
- `artifacts/full-accuracy/feedback-copy-*.log`: analytical host suites and build checks.
