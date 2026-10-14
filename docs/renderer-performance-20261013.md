# Performance addition, 13 October 2026

VM119 reaches **648** in the matched one-second glmark2 protocol
(scores 648, 643, 650), after the fully validated 551 build met the 500 target.
Standard-duration confirmation scores **659, 659, 657** (median **659**)
after a normal restart using the installed VM119 defaults. Other VMs' CPU allocations remain unchanged.

The current build passes the complete project-selected Piglit comparison,
focused FBO regressions, host tests, accuracy and mixed X11/OpenGL controls.
The existing unsupported-feature skips remain unchanged.

## Benchmark conditions

- Proxmox host: Intel i5-7500T, four physical CPUs, Intel HD Graphics 630.
- VM119: two vCPUs, CPU weight 1000. Only its vCPU threads are pinned to
  CPUs 2 and 3; its other QEMU threads can use CPUs 0–3. These CPUs remain
  shared with the other workloads.
- Guest: Debian 12, corrected Mesa 22.3.6 SVGA module, extended state enabled.
- Desktop: 1280×800, compositor disabled, hardware cursor disabled.
- Benchmark: glmark2 2023.01, 800×600, vblank disabled, complete default
  selection. All 32 supported scenes render; terrain remains unsupported.
- Matched protocol: 0.5-second-per-scene warmup, three one-second-per-scene
  timed runs. Standard-duration results are recorded separately.
- Vulkan validation is disabled for timing and enabled for correctness QA.
- Other VMs and background workloads remain active. Ambient host load varies;
  the saved logs retain each benchmark's controls and results.

| Candidate | One-second scores | Median |
| --- | --- | ---: |
| Initial correctness workspace | 429, 433, 428 | 429 |
| Direct coherent framebuffer transfer | 482, 483, 477 | 482 |
| FIFO improvements and process-owned framebuffer import | 481, 485, 487 | 485 |
| Retained 500 build with synchronous submission fence | 551, 548, 552 | 551 |
| Compact buffers | 545, 543, 547 | 545 |
| Rounded capacities and partial rename copies | 549, 548, 549 | 549 |
| Image-transfer batching | 551, 551, 551 | 551 |
| Private Mesa 26.2.4, original Xorg presentation | 591, 588, 589 | 589 |
| Same driver with guest GL threading | 592, 588, 593 | 592 |
| Screen readback and DirectPresents | **648, 643, 650** | **648** |

A later retained-build control scores 512, 526, 518 (median 518), showing
shared-host variation relative to its earlier 551 measurement. These are
recorded controls rather than substitutes for the measured candidate runs.

The earlier 500 build's standard-duration diagnostic scores 558. The private
Mesa driver with original presentation scores 597. The final configuration's
standard-duration runs score 659, 659 and 657 (median 659) after a normal
restart using the installed defaults. All 32 supported scenes render in
every run.

## Retained changes

- Import eligible process-owned framebuffer memory through
  VK_EXT_external_memory_host. Require aligned, bounded, coherent host-visible
  memory and successful GPU completion before display notification. Unsupported
  mappings retain the copy path. Replacement and destruction preserve pending
  resources until completion is established.
- Prefer cached coherent transfer memory, cache pipeline and dynamic state,
  use attachment load clears for eligible full clears, and avoid redundant FIFO
  indexing and packet initialization. State caches reset with command-buffer
  recording epochs.
- Reuse a synchronous submission fence with at most 2 ms of polling before the
  mandatory blocking wait. A failed completion never retires pending work.
- Use page-rounded 4 KiB vertex/index buffer capacities. Immutable renaming
  copies only the prefix and suffix outside the incoming update, preserving
  buffers referenced by earlier draws.
- Opt in to batching eligible distinct-image single-box copies and blits.
  Buffer copies, self-copies and multi-box copies retain synchronous handling.
  Readback, presentation and guest fences still require completion.
- Implement primary-screen readback into the primary 32-bit/24-depth VRAM
  framebuffer. Validate the complete source and destination ranges, complete
  pending GPU work, then snapshot the source before writing potentially
  overlapping destination rows.
- Select a private Mesa 26.2.4 Intel Vulkan driver for VM119 and enable Xorg
  DirectPresents after validating the readback path.

DirectPresents initially exposed black pixels because screen-to-GMRFB readback
was unsupported. Accuracy and glmark2 validation alone did not catch this.
After implementing bounded readback, the mixed X11/GL probe passes all front,
window and screen pixel checks across swaps, software overlays, window moves
and a four-color orientation pattern. The original Xorg path's stale window
readback checks also disappear with the corrected DirectPresents path.

The guest probe is retained in
[glx_x11_mix_probe.c](../tools/mesa-guest/glx_x11_mix_probe.c), with build/run
instructions in the [guest tools README](../tools/mesa-guest/README.md).

## Correctness results

| Check | Result |
| --- | --- |
| Host release executables | 19 passed |
| Debug harness | 14 suites passed |
| Python tests | 36 passed |
| Test/acceptance targets | Passed |
| Selected Piglit, SVGA | 141 passed, 5 existing skips |
| Selected Piglit, llvmpipe reference | 146 passed |
| Focused FBO Piglit, SVGA | 13 passed, 16 existing skips |
| Focused FBO Piglit, llvmpipe reference | 29 passed |
| Accuracy and mixed X11/GL pixels, including orientation | Zero failures |
| glmark2 validation | 27 successes, 6 unknown checks, zero failures |
| Complete QA Vulkan/synchronization/renderer log | Zero errors |
| Standard-duration benchmark driver log | Zero renderer/submission/wait/pipeline errors |
| Host and guest GPU kernel interval | No hang/reset/fence timeout |

Both Piglit comparisons complete without missing or unexpected cases and
retain exactly the earlier skip names. The selected hardware run has 149
passing subtests and 160 existing skips; its software reference passes 309
subtests. The focused hardware run passes 154 subtests.

The selected top-level skips are:

- gl-2.0-large-point-fs
- fragment-and-vertex-texturing
- fbo-mrt-alphatest-no-buffer-zero-write
- fbo-blending-snorm
- fs-discard-deep-branch

Host Vulkan tests use the verified Khronos synchronization-validation fixture
and SPIR-V tools. The guest QA process also enables the Khronos layer and
VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT. Pixel regressions
cover framebuffer replacement, recording epochs, buffer growth and immutable
updates, batched image copy/blit ordering, pitched screen readback, overlap,
invalid bounds and unchanged destinations after failed GPU completion.

Piglit uses the private fixture launcher with the two verified Debian 12
fixture corrections and unchanged pixel expectations. The system package and
software reference remain available. See the
[fixture validation report](piglit-fixture-validation-20261010.md).

Guest vmw_msg_ioctl channel errors also occur in the legacy baseline. The
kernel checks above specifically cover GPU hangs, resets and fence timeouts.

## Installed configuration and identity

| Component | SHA-256 |
| --- | --- |
| VM119 adapter | `597b66347fc9e0d00715472a95b43ecd5d7c2dda645036ebbefae4ad9315be1c` |
| Private Intel Vulkan driver | `6199401013d1704e4b030768780954d094e2a7add4e1a0b26b0656f8d741af12` |
| Corrected guest module | `88a1b2409b87b946843133ff28b1c0cf382dd21436b69e9a3507ce8ecaa7381a` |
| Stock QEMU executable | `383e4a30f5ae8f67f8a24fd2ea668f6592a72bd96507dadbc555b5dba5bc2103` |
| Installed KVM wrapper | `4c5d0f1815f6f4e1d6ce271e6a1474e53c19acd04e34e977285d9d9a823a485d` |

VM119 uses `/usr/local/lib/libqemu_svga3d-vm119.so`. Its stock-QEMU startup
branch defaults image batching to 1 and selects
`/usr/local/lib/vlkn-vm119/mesa-26.2.4/intel-vm119.json` when neither Vulkan
ICD override variable is set. Explicit overrides remain available. Other VM
branches and their original global adapter remain unchanged. Native QEMU
experiments are inactive.

The default guest driver directory now contains the corrected module used by
the benchmark. Xorg retains `HWCursor "false"` and adds
`DirectPresents "true"`. The normal restart verifies the expected driver,
environment and display configuration. The final runtime audit confirms
the same adapter, driver and wrapper identities after all three longer runs.

The private Intel driver uses release optimization and Skylake CPU targeting.
Upstream rejects LTO for this build, so LTO is disabled. Its dependencies
resolve against existing host libraries; system Vulkan packages are unchanged.
The official release archive SHA-256 is
`bce5f7fbebb934373b86c999a064d52fb5065878dc57f287f95346648ec832e9`.
[Upstream release](https://docs.mesa3d.org/relnotes/26.2.4.html).

Backups are retained for the original adapter, KVM wrapper, default guest
module, Xorg device configuration and XFCE display configuration.

## Experiments excluded from the final build

| Experiment | Result or reason excluded |
| --- | --- |
| Deferred FIFO completion | Guest fence wait stalled |
| Native asynchronous QEMU IRQ path | Median 524; below retained control |
| Native BQL release during GPU wait | Median 451; below retained control |
| Reusing completed mapped buffers | Median 484 |
| Guest Mesa LTO/Skylake build | Median 520 versus contemporaneous control 518 |
| Timestamp query polling hints | Median 507 versus control 518 |
| Longer fence polling, event polling, narrower barriers, cached scanout commands | No material gain |
| Guest GL threading with private Mesa 26 | Median 592; unnecessary for final configuration |
| HasVK ICD | Could not enumerate the HD 630; never selected for VM119 |

Temporary submission stack tracing was removed. The adapter does not defer
or acknowledge successful guest fences before GPU completion.

## Evidence

Artifacts are under `artifacts/performance-addition-20261013/`:

- `mesa26-screen-readback-direct-scores.json`: matched one-second measurements.
- `frozen600-standard-*`: normal-startup standard-duration confirmation.
- `screen-readback-release-tests/`, `screen-readback-debug-harness.log`,
  `screen-readback-test-acceptance.log`, `image-batch-python-tests.log`.
- `piglit-600/summary.json`, `piglit-fbo-600/summary.json` and complete test logs.
- `frozen600-syncval-*`: accuracy, glmark2 and mixed X11/GL checks.
- `frozen600-complete-qa-*`: exact complete QA driver and kernel intervals.
- `kvm-600-defaults-installed.log`, `default-guest-driver-update.log`.
- `report-before-final-rewrite.md`: detailed experiment history.
