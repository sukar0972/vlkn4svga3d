# Renderer performance and retained fixes, 7 October 2026

The candidate retains depth bias, sRGB sampling/output, relative shader
constants, queued-draw ordering, robust guest buffer fetches, and device-loss
handling. It reduces redundant programmable constant uploads, uses direct
render-state lookup, and supplies an optional Vulkan image format list for
mutable linear/sRGB images. It also repairs the advertised hardware version
so this Linux guest can use accelerated direct rendering. Release measurements explicitly disable
Vulkan validation and verify that its layer is absent from the QEMU process.

## Changes and correctness

Identical FLOAT and INT constant updates compare raw bits; BOOL updates compare
their effective value. Unchanged programmable draws reuse an immutable constant
slice within a submission epoch. Completed submissions invalidate the slice
before resetting the ring cursor. Changed constants allocate a fresh slice,
and ring exhaustion still submits and waits before reuse. Fixed-function and
POSITIONT draws continue uploading their derived constants on every draw.

When the device advertises `VK_KHR_image_format_list` and both requested and
device API versions support its Vulkan 1.1 dependency, mutable images list
their linear and sRGB view formats explicitly. Drivers can retain format-specific
compression with both views available. Other devices retain the existing path.
This does not remove gamma support or change image pixels.

Render-state names have a fixed protocol range. A zero-initialized array and
presence bits replace the hash table, retaining the distinction between an
unset state and an explicit zero value. In particular, unset independent
color masks still inherit the primary mask. Setters reject forged state names
before indexing; getters preserve their previous not-found behavior. Existing
pixel tests and new absence/explicit-zero/forged-read checks cover these rules.

The preload now advertises `SVGA3D_HWVERSION_WS8_B1` (`0x20001`). An October 5
cleanup had selected the real `WS65_B1` value (`0x20000`), whereas the earlier
local definition with that name actually meant `0x20001`. The lower version
disabled this guest's Xorg/DRI path. Using the correct enum restores the
working protocol value while retaining capability and preload-policy fixes.

All 14 host harness suites passed with lavapipe for the Vulkan suites. The
buffer-ordering suite includes a new pixel regression across four submission
epochs, with unchanged and changed constants queued before readback. An isolated
negative control removing epoch invalidation fails that regression. Existing
depth-bias, gamma, relative-constant and ordering regressions pass. `make test`
and `make preload-lab-release` pass. Optional `spirv-val` was unavailable.
The final guest replay results are recorded in the follow-up below.

## Release measurements

The combined candidate recorded a median **439**, compared with **442** for
the current release baseline (**−0.68%**). This is a small measured decrease;
the six short samples do not establish its cause or a stable regression. The
candidate does not demonstrate improved whole-suite throughput. The **500
target remains unmet** under the tested background load.

| Combined comparison | Scores in execution order | Median |
| --- | --- | ---: |
| Current release, all correctness fixes | 442, 443; return 442, 441 | 442 |
| Format list and constant reuse | 438, 440 | 439 |

Both builds recorded **27 glmark2 validation successes, six unknowns and zero
failures**. The guest identifies the SVGA3D renderer in every block.

The format-list-only ABBA comparison scored **437, 438, 442, 440** before
(median **439**) and **447, 442, 446, 437** after (median **444**, +1.14%).
The ranges overlap, so this does not establish a reliable whole-suite speedup.
Both builds recorded 27 glmark2 validation successes, six unknowns and zero
failures. Unknowns and unsupported terrain are not correctness passes.

A focused local lavapipe benchmark measured **50.17 → 3.35 µs/draw** median
(93.3% less elapsed time) across ten alternating before/after pairs. Each sample
queues 3,000 programmable draws of a 4×4 triangle with identical constant
updates, includes GPU completion in the timing, and verifies every output pixel
afterward. Reusing constants avoids exhausting the ring and its repeated
submit-and-wait cycles in this workload. This synthetic result measures the
unchanged-constant path; it is not a guest FPS improvement or a hardware result.

The combined comparison uses before/after/before blocks, two timed runs per
block. Both baselines retain all correctness fixes. Each block restarts VM119,
warms up the complete glmark2 selection for 0.5 seconds per scene, then measures
one second per scene at 800×600 with vblank disabled. Adapters use `NDEBUG`,
explicit `SVGA3_VLKN_VALIDATE=0`, and disabled FIFO tracing. VM119 has two vCPUs
pinned to CPUs 2 and 3, CPU weight 1000, and an Intel HD 630 with GPU minimum
and maximum 1100 MHz. VM105 and other background workloads remain active.

Those initial isolated adapters used the common lab hardware-version
adjustment to `0x20001`; unmodified `0x20000` disabled guest direct rendering.
The follow-up candidate incorporates the enum repair in the workspace source
and is built without an external compatibility patch.

## Correction to the earlier depth-bias report

The earlier **298.5 → 301** comparison was validation-enabled. Clearing
`SVGA3_VLKN_VALIDATE` selected the debug default, contrary to the initial report.
A CPU profile confirmed the validation layer was active. Those scores cannot
be compared with historical release scores above 500. The corrected
[depth-bias report](depth-bias-benchmark-20261007.md) preserves that evidence.
An isolated fresh run of the installed older release adapter scored **465**;
it is a diagnostic reference, not a matched comparison with the new candidate.

Historical 556 and 532 scores lack matching background-load records. The
October 4 matched results were around 450, and the 500 target remained unmet.
See the [earlier performance report](renderer-performance-20261004.md).

## Evidence

Source baseline: `0a09f02c11e617712f55be6b045e9d0c07c35981`.
The combined release baseline adapter SHA-256 is
`b1847e0364cc066950b7278c5aa7d3c1b94ffd3b768bec39e224307450934066`;
the candidate is
`a40232e4e45f36294b5f6278a610bf14ccf5a6fcf46ebb3d9a24c27519dac491`.
The unchanged guest driver SHA-256 is
`2c4bb2130da43a1e6f81904ef361538349025e15a287200170cccb0dfb0de43f`.
Raw evidence is retained under `artifacts/performance-recovery-20261007/`
(excluded from Git): `release-comparison/`, `combined-comparison/`, host harness
and mock logs, release-target build log, and `constant-reuse-negative/`.
`constant-draw-benchmark/` retains the focused benchmark source and all samples.
Each comparison records exact adapter hashes, source patches, process maps,
renderer identities, per-run host conditions, validation and timing logs,
original affinities, and restoration status.

Both comparisons restored the original installed adapter and vCPU affinities.
The restored SHA-256 is
`1df1f5737edc40689bedde7e46b2980a8897b09c9bd604a1764db1347e77a1b8`.
VM119 and VM105 remain running, the SVGA3D graphics-health probe passes, and
the desktop remains at 1920×1080. The candidate changes remain in the workspace;
they are not installed as the lab's default adapter.


## Follow-up: exact historical binary and retained candidate

The adapter recorded at **558 / 556 / 555** on October 3 is still available.
Its SHA-256 is `9270408e8e3cf467481a6163652ebde3ca6d70ed11376a98b4d8c69a4bbcc46e`.
A historical/current/candidate/current/historical comparison now records:

| Build | Timed scores in execution order | Median |
| --- | --- | ---: |
| Exact historical 556 adapter | 466, 470; return 465, 464 | **465.5** |
| Current correctness baseline, format lists and constant reuse | 438, 441; return 441, 439 | **440** |
| Retained candidate, additionally direct state lookup and enum repair | 445, 439, 441, 438 | **440** |

The historical binary itself is **16.3% below its October 3 median** under
today's conditions. Thus the cross-date drop from 556 cannot be attributed
entirely to renderer changes. Background-load records for October 3 are
incomplete; this comparison does not isolate CPU contention from every other
change outside the adapter. Earlier controlled experiments demonstrated that
CPU shares, vCPU affinity, other VMs and GPU clock settings materially affect
this lab's score.

Under today's matched profile, the corrected candidate is **5.48% below**
the historical binary. That remaining gap is real in these samples. It spans
many rendering and guest-contract changes; this experiment does not attribute
it to a single fix. The historical adapter's recorded Piglit result was
**64 pass / 70 fail / 5 skip / 7 timeout**, including blending, scissor,
two-sided stencil, back-color and framebuffer failures. Returning to that
adapter would restore known rendering defects. Its 27 glmark2 validator
successes did not establish broader accuracy.

Direct state lookup produces **no measured whole-suite median change** here.
It does reduce a focused draw workload from **3.327 to 2.749 microseconds per
draw**, or **17.38%**, across ten alternating pairs of 3,000 draws with GPU
completion included and every output pixel checked. Both builds retain full
buffer robustness, constant reuse and image format lists. This is a local
lavapipe draw-cost improvement, not a guest FPS improvement. The **500-plus
whole-suite target remains unmet** under the tested background workload.

The final comparison uses the same 800x600, one-second scenes, explicit
validation-off release policy, fresh boots, warmups, vCPU affinity, GPU limits,
CPU weight and running background workloads described above. Reference builds
have two blocks with two timed runs each; the candidate has four runs in one
block. The limited sample count and single candidate boot restrict precision.
All three builds recorded **27 glmark2 validation successes, six unknowns,
and zero failures**. The retained candidate was built directly from the
workspace, without an external hardware-version adjustment.

### Experiments not retained

An optional `VK_EXT_pipeline_robustness` path omitted duplicate bounds checks
on translator-controlled uniform buffers while retaining guest vertex/index
robustness. Its ABBA scores were **439, 436; return 435, 440** before (median
437.5) versus **440, 442, 438, 444** after (median 441, **+0.8%**). Ranges
overlap. It was removed; the retained candidate keeps full buffer robustness.

A native Intel submission probe compared queue-idle waits with submission
fences. Twenty alternating pairs of 300 synchronous 4KiB copies measured
**65.126 versus 62.524 microseconds per submission** (about 4% in this tiny
workload), with output bytes checked. Separate validation-enabled invocations
passed without validation messages. The approximately 2.6-microsecond saving
per submission does not explain the full-suite gap; no completion-path rewrite
was retained.

The first diagnostic comparison used another retained PR18-era binary,
`9e4746581e89e052344bfb94fb06af3e15d7c8557fdb350ca3960f5e0f42ba26`,
which scored **456, 470, 465, 462** versus **442, 433** for the current release.
Its identity differs from the recorded 556 adapter. An initial progress update
incorrectly called it the exact binary; the mismatch was corrected and the
exact comparison above supersedes that inference. Flat CPU profiles from this
diagnostic show substantial copying and driver-submission work, and repeated
render-state hash lookups. They do not quantify the causes of the historical
score gap or measure GPU shader cost.

A queued-buffer copy experiment preserved only bytes outside the next DMA
update, avoiding a copy of bytes immediately overwritten. Ten alternating
lavapipe pairs of 1,000 draws and 256KiB updates, including GPU completion,
verified every output pixel and all readback bytes. Full updates improved
**242.757 to 230.133 µs/update (5.20%)**, and middle-half updates improved
**236.943 to 230.650 µs/update (2.66%)**. The queued-draw pixel regressions also
passed. With the corrected guest driver, however, the complete guest suite
scored **440, 433; return 439, 437** before (median **438**) versus
**440, 434, 432, 431** after (median **433**, **−1.14%**). Both validators
recorded 27 successes, six unknowns, and zero failures. The ranges overlap,
but there is no guest gain to justify retaining this experiment. It remains
only in ignored artifacts; the workspace upload path is unchanged.

### Final guest correctness and evidence

The retained adapter is byte-identical throughout the guest-driver repairs:
`26d2ca757188ff392e77e0fee9496114f4e6936674d58586be6fa4430f0e53df`.
The final guest driver is
`88a1b2409b87b946843133ff28b1c0cf382dd21436b69e9a3507ce8ecaa7381a`.
It completed **146 cases: 140 pass, one raw fail, five skip, zero timeout**,
with **zero regressions, recoveries, Vulkan validation errors or warnings,
and kernel GPU hangs**. The original adapter and vCPU affinity were restored,
graphics health verified, and the default guest module remained unchanged.

The one raw failure is the dated unsized depth/stencil cube-storage API
expectation (`GL_INVALID_ENUM` versus `GL_INVALID_VALUE`). It recorded zero
failed pixel probes; the separate corrected-expectation control passed on
this exact pair. The unmodified result is retained. These are selected cases
in separate processes, not full OpenGL conformance or continuous-session
stability validation.

Disabling unsupported floating `Z_DF24` made Mesa choose legacy `Z_D24X8`.
Mesa assumed those formats automatically perform hardware shadow comparison
and suppressed shader comparisons; this adapter returns raw depth. The final
opt-in patch retains the requested shader comparisons. It also permits legacy
raw-depth copy blits, which Mesa otherwise blocks under the same comparison-only
assumption. Ordinary copied depth sampling and all shadow variants now pass.
The original depth formats and non-opt-in VMware behavior are preserved.
All nine focused depth/shadow/framebuffer cases pass with no validation
messages, hangs or recoveries. The complete Mesa patch applies cleanly to
recorded pristine sources. All 14 rebuilt host harness suites also pass;
new checks clear and sample D24S8_INT, D24X8 and D24S8.

The final matched driver benchmark kept this adapter fixed, explicitly disabled
Vulkan validation, and verified the layer was absent. Old/new/new/old blocks
with two complete timed runs each scored **436, 434, 443, 429** before
(median **435**, range 429–443) versus **431, 431, 435, 434** after
(median **432.5**, range 431–435): **−0.57%**, with overlapping ranges.
Both validators recorded 27 successes, six unknowns and zero failures.
There is no demonstrated whole-suite gain or stable small regression.
The three buffer scene medians fell 6.92%, 4.19% and 2.63%; these outliers
need further measurement and do not establish that the depth guards caused
the changes. All 32 scenes are in `raw-depth-blit-driver-benchmark/scene-comparison.json`.
The original adapter and affinity were restored, and the default guest module
remained unchanged. **500+ has not been recovered.**

Superseded repairs remain recorded with their exact identities:

| Guest module | Correctness result | Matched driver benchmark |
| --- | --- | --- |
| Original `2c4bb213` | Replay stopped at case 92: 88 pass / 3 skip / 1 new shadow fail; original adapter passes all 58,212 checks on that case | Baseline in each comparison below |
| Integer-format fallback `467df04e` | Full 146: 140 pass / 1 known API fail / 5 skip; no new regressions | 443, 438, 439, 444 (median 441) → 435, 428, 434, 428 (median 431), −2.27%, non-overlapping ranges; removed |
| Shader-comparison-only `d6ae256c` | All shadow variants pass; replay stops at case 112: 108 pass / 3 skip / 1 new `texdepth` fail | 436, 428, 438, 434 (median 435) → 438, 436, 438, 429 (median 437), +0.46%, overlapping ranges; superseded |
| Final comparisons and depth blits `88a1b240` | Full 146: 140 pass / 1 known API fail / 5 skip; no new regressions | 435 → 432.5, −0.57%, overlapping ranges |

The first comparison-only controller stopped during guest startup before any
cases ran; it was restarted with the original backup preserved. That replay
later caught the `texdepth` regression and restored the lab. The final replacement
replay ran from case one without interruption. Neither superseded driver
benchmark is evidence for the final module's performance.

Raw evidence is in `artifacts/performance-preserve-20261007/`:
`full-validation-raw-depth-blit/`, `raw-depth-blit-focused/`,
`raw-depth-blit-driver-benchmark/`, and `raw-depth-blit-fix/patch-check/`.
Earlier experiments remain in `full-validation/`, `full-validation-repaired/`,
`full-validation-raw-depth/`, `depth-fallback-benchmark/`,
`raw-depth-driver-benchmark/`, `final-matched-comparison/`,
`draw-cpu-comparison/`, `uniform-robustness-comparison/`, and `submission-probe-*`.
Final evidence retains the source patch and manifest, loaded hashes, renderer
identities, validation settings, per-case pixel checks, per-scene measurements
and restoration status. The historical adapter comparison and rejected
optimization experiments remain separate measurements.
