# Testing

Run these commands from the repository root.

## Tests

Basic reference and mock-engine checks:

```sh
make test
```

These checks do not prove correct GPU rendering. Run the broader suite separately:

```sh
vulkaninfo --summary
make acceptance
```

The acceptance runner stops on the first failure and writes evidence under `artifacts/`. Missing Vulkan drivers or validation layers may prevent these tests from running. Read the actual logs; successful assertions do not establish full protocol coverage.

Required packages for `make acceptance` (Debian/Ubuntu names; other distros equivalent):
- a Vulkan ICD — e.g. `mesa-vulkan-drivers` (provides lavapipe for headless runs) plus `libvulkan1`
- `vulkan-validationlayers` — needed by the suites that enable `VK_LAYER_KHRONOS_validation`
- `spirv-tools` — optional; enables the `spirv-val` step in shader-translation suites (they report SKIP, not PASS, when it is absent)
- `vulkan-tools` — optional; provides `vulkaninfo` for the pre-flight check above

Individual targets:

| Command | Purpose |
| --- | --- |
| `make test-oracle` | VirtualBox reference implementation with mock D3D9 |
| `make test-vlkn` | Mock engine checks |
| `make test-real-vulkan` | Driver and validation checks |
| `make test-shader-translation` | Shader translation checks |
| `make test-shader` | Shader execution checks |
| `make test-guest-mem` | Guest-memory handling |
| `make test-verified-rendering` | Rendering comparisons |
| `make test-presentation` | Framebuffer presentation |
| `make test-qemu` | Host-side device integration harness |

**`test-qemu` does not boot an actual QEMU guest.** Passing it is not proof of working Linux, Windows, or QNX graphics. Test names and success banners inherited from development should not be read as completeness claims.

## Unsupported capabilities

The core and lab preload report zero for MSAA, alpha-to-coverage, supersampling,
fixed-function vertex blending, vertex texture sampling and gradient sampling.
Multisample surfaces, non-disabled vertex blend state, VS texture instructions
and instancing divisors are rejected. Unknown and unmapped surface formats,
including packed/planar YUV, fail definition instead of being replaced with BGRA.
Stencil clears are ignored on attachments with no stencil aspect.

## Depth bias

`DEPTHBIAS` supplies a normalized offset on D16 and D24 UNORM attachments;
`SLOPESCALEDEPTHBIAS` supplies the slope factor. Both values are recorded for
each draw. NaN and infinity are rejected without changing the stored state.
Draws reject constant offsets whose Vulkan conversion overflows and nonzero
normalized offsets on floating-point depth attachments. Bias does not prevent
color rendering when no depth attachment is bound.

The buffer-ordering suite checks stored depth, coplanar pixels, queued draws
with different offsets, both slope-factor signs, and invalid inputs with Vulkan
validation enabled through teardown.

## Gamma, relative constants and points

Texture `TS_GAMMA` and render-target `OUTPUTGAMMA` accept float values 1.0
and 2.2. Gamma 2.2 selects compatible sRGB image views for BGRA/RGBA and
BC1/BC2/BC3 formats; alpha swizzles and mip selection are preserved. Render
passes and descriptors retain the selected view, so changing gamma does not
rewrite a view used by queued draws. Formats without an sRGB twin keep their
native view.

Shader constant sources support `c[a0.component + offset]` in vertex shaders
and `c[aL + offset]` inside vertex/pixel loops. MOVA rounds the address value;
relative loads honor shader DEF constants and return zero for indices outside
c0–c255 without accessing the remaining uniform bank. Malformed extension
tokens and relative destinations are rejected. The translator suite validates
SPIR-V, and buffer-ordering checks rendered values.

The point-size cap is one. Point-size states accept only 1.0, shader PSIZE
outputs clamp to one, and enabling point sprites fails explicitly. The
verified-rendering suite checks one covered sample and unchanged position.
Larger points and sprites remain unsupported.

## Device loss

A Vulkan `VK_ERROR_DEVICE_LOST` result disables the backend for its remaining
lifetime. Later 3D FIFO packets are framed and dropped with limited logging;
2D updates, screen objects and fences continue. Direct 3D API calls return
device-lost, and reset cannot revive the failed Vulkan device. Destroy and
recreate the device to start a fresh backend. Driver allocations are retained
until process exit on loss because submission completion cannot be established.
Other submission errors remain retryable.

`test_fifo_framing` checks loss during submission followed by wrapped 3D,
UPDATE, screen-object and fence packets. `test_atomic_commands` checks loss
during command-buffer begin/end and queue/device waits, with no subsequent
reset, completion-serial advance or submit. `test_preload_fifo` verifies that
the preload path drains fences and still presents 2D after a renderer failure.
These deterministic injections do not establish recovery from a physical GPU
hang; device recreation and guest fault replay remain deferred.

## Query failure policy

Queries require a successfully created Vulkan query pool. BEGIN, END and WAIT
fail when the pool is unavailable; sample counts are never invented. WAIT requires
an ended query. A failed END leaves the query inactive and failed until the next
BEGIN resets the slot. END publishes PENDING, successful WAIT publishes SUCCEEDED,
and failed END/WAIT publishes FAILED when a guest result pointer is provided.
Invalid guest result pointers return an error. `test_query_state`, included in
`make test`, covers this policy and mock Vulkan readiness/stride handling.

## Continuous integration

Every push and pull request runs the oracle/mock, preload FIFO/fence, DX,
ICD-free translator (with `spirv-val`), malformed-input, guest-memory,
presentation and host QEMU integration suites. The last three use Mesa lavapipe
with the Vulkan validation layer installed. Any suite failure fails the job.
Shared FIFO framing regressions run in `make test`, including wrapped packets,
unknown commands, semantic errors and failed fences. Core 2D execution implements
UPDATE/UPDATE_VERBOSE notifications, RECT_COPY notifications, GMR2 and fences.
Screen/GMRFB, cursor, annotation, RECT_FILL and FRONT_ROP_FILL commands are
framed and skipped by the core; the lab preload retains its screen/GMRFB and
VRAM rectangle implementations. These execution differences remain intentional.

Real-driver, buffer-ordering, shader-execution and verified-rendering suites
remain available locally through the harness loop. Piglit requires the live VM;
CI checks its reporting logic only.

## Performance changes and correctness contracts

The goal is higher end-to-end throughput with unchanged guest-visible accuracy.
Tests should assert pixel values, command ordering, memory visibility at completion,
error behavior, and preservation of untouched regions. They should not require a
particular number of copies, queue submissions, GPU waits, or notification calls.
Microbenchmarks and profiles explain costs; glmark2 measures the complete path.
An isolated microbenchmark improvement does not establish an FPS improvement.

The presentation harness observes its registered CPU framebuffer after an explicit
`svga3_vlkn_device_wait_idle` completion point, once per submitted FIFO stream.
It checks that the union of valid display notifications covers the presented
regions, allowing splitting, reordering, and coalescing. Over-notification may
request repainting unchanged pixels; those framebuffer pixels must still be
preserved exactly. Negative controls reject missing coverage and out-of-bounds
notifications. Two overlapping PRESENT commands in one stream verify that the
later write wins and all surrounding pixels remain unchanged.

This test structure does not implement asynchronous presentation or GPU scanout.
The current wait-idle API drains the Vulkan backend; a future presentation queue
must also be drained at the observation boundary before reusing the framebuffer
or callback state. A GPU-backed display adapter will need its own integration
coverage. Guest fence ordering and CPU readback visibility must remain correct
without adding artificial waits to the guest path to make tests pass. Keep the
buffer-ordering regressions, exact copy comparisons, and Piglit expectations;
change an implementation-specific assertion only with equivalent behavioral
coverage. Unsupported skips and unknown validation results are not accuracy
passes, and known upstream expectation mismatches remain recorded separately.

The optional real-Vulkan presentation benchmark covers full 800×600 frames,
64×64 updates, overlapping clips, fully offscreen updates, and identical shader
redefinitions. It verifies every framebuffer pixel after completion and reports
JSON lines with time, transfer bytes, CPU copy calls, submissions, and queue wait
time. Run `make benchmark-presentation BENCHMARK_ITERATIONS=200`; set
`BENCHMARK_VALIDATE=1` for a validation run. Alternate baseline and candidate
executables on the same device for comparisons. These timings are not glmark2
scores or predictions of guest FPS. Performance counters are disabled by default;
`SVGA3_VLKN_PERF_COUNTERS` enables them at backend startup and logs totals on
shutdown. Queue timing covers the submission queue wait, not every device idle.

See [the presentation efficiency report](presentation-efficiency-20261005.md)
for the audit, measurement conditions, and accuracy results.

## Piglit guest OpenGL tests

[Piglit](https://piglit.freedesktop.org/) adds external OpenGL correctness tests through the guest's Mesa SVGA driver and the QEMU renderer. Install it **inside the Debian guest** with an existing Xorg session:

```sh
sudo apt install piglit mesa-utils
```

From this repository on the development machine, run:

```sh
make test-piglit PIGLIT_ARGS="--ssh user@guest"
```

The default compares the same focused selection from Piglit's `quick` profile using SVGA3D, then guest CPU rendering via llvmpipe. It covers legacy texture operations, blending, scissor/depth/stencil, framebuffer objects, and selected GLSL 1.10/1.20 execution tests. Tests run serially on display `:0`, with a 60-second per-test timeout and a 30-minute limit per renderer. The runner checks `glxinfo` renderer identity before each run, forces GLX, and clears software-rendering and version overrides. The software choice applies only to the test subprocesses.

The current lab uses `svga3d@10.0.0.144`, display `:0`, in Proxmox VM119 on `10.0.0.200`. QEMU loads the host's `/usr/local/lib/libqemu_svga3d.so`. Piglit belongs in the guest: running it on the Proxmox host would bypass the guest SVGA path. Ensure the intended library is already loaded before testing; the runner does not deploy or restart QEMU. Keep the VM idle for consistent results.

The initial VM runs reproduced Intel GPU hangs while `copyteximage 2d`, `texsubimage`, and framebuffer format tests were executing, followed by Vulkan device loss and stalled tests. The smoke runner schedules copyteximage/texsubimage cases last, preserving them in the test list so earlier cases can finish. `--last REGEX` overrides this ordering. This ordering changes no expected outcomes and does not suppress failing cases. If device loss occurs, stop the accelerated run, retain the logs, and recover before further accelerated testing. Unexecuted cases are not rendering failures or passes.

Other runs:

```sh
# Only the guest SVGA path:
make test-piglit PIGLIT_ARGS="--ssh user@guest --renderer svga"
# Broader upstream profile, including unsupported API features (many expected skips):
make test-piglit PIGLIT_ARGS="--ssh user@guest --suite quick --run-timeout 14400"
# Investigate a particular feature; --include replaces the default selection:
make test-piglit PIGLIT_ARGS="--ssh user@guest --include arb_depth_texture"
# Run directly inside the guest after copying this repository:
make test-piglit
```

Use `--piglit /path/to/piglit` for a built upstream checkout, `--display` for a different X session, and `--exclude` to make a deliberately narrowed run. Exact selection and commands are saved. Debian's package is a dated snapshot; compare runs using the same Piglit version and selection.

Each run creates a new `artifacts/piglit-*` directory with renderer/system identity, package versions, command lines, logs, raw compressed JSON, `summary.json`, and an HTML report under `html/index.html`. The JSON summary counts top-level cases and subtests separately; upstream console totals may differ. SSH runs also retain a temporary guest result directory, printed at startup. Copying evidence back happens even if the runner fails. Inspect raw test output for expected/observed pixels and API errors.

Exit status is nonzero for test failures, crashes, timeouts, warnings, incomplete/missing results, mismatched comparison test lists, or runs without any passing cases. A nonzero Piglit runner exit retains partial evidence and still attempts the software reference. The summary marks interrupted runs as incomplete and planned cases that never started as `notrun`. Skips remain separate from passes. `summary.json` distinguishes SVGA problems, skips, and unverified cases that pass on llvmpipe; skips can reflect differing advertised capabilities. Such differences identify investigation targets, not their cause. This is independent of `make acceptance` and `make harness-loop`; CI checks the result-reporting logic, while guest tests require the live VM. Piglit does not certify VM stability, desktop scanout, or full API conformance.

The [recorded VM119 comparison](piglit-accuracy-20261004.md) documents the corrected guest driver, raw accuracy results, and benchmark conditions. Slow texture probes on a contended VM119 needed `--test-timeout 600`; retain raw timeout results when comparing different limits.

## Harness loop

`make harness-loop` runs the tight build/test loop used for iterative translator and driver work:

```sh
make harness-loop                      # one full build + all suites
make harness-loop HARNESS_ICD=/path/to/icd.json
SPIRV_TOOLS_DIR=/path/to/spirv-tools/build/tools make harness-loop
scripts/harness_loop.sh --iterations 5 # repeat; stops on pass->fail regressions
scripts/harness_loop.sh --fail-fast --skip-build
```

The loop requires a real Vulkan ICD (`HARNESS_ICD`, else a hard failure — never a silent skip), enables `spirv-val` when `SPIRV_TOOLS_DIR` provides it, builds everything, then runs the ICD-free translator and preload FIFO suites followed by all lavapipe-backed suites. Logs and a `results.tsv` are written per iteration under `.harness-loop/`. Iteration stops on the first pass→fail regression between runs.

What each suite proves:

| Suite | Proves |
| --- | --- |
| `test_translator_novulkan` | D3D9→SPIR-V translation without a Vulkan ICD; optional `spirv-val` (prints SKIP, not PASS, when unavailable); SPIR-V value-flow assertions that shader inputs actually reach the declared outputs (not just decorations) |
| `test_preload_fifo` | Actual preload FIFO walker on a synthetic QEMU state, without patching a process: batches exceeding 8192 commands, ring wrap, producer notification races, incomplete packets and final fences |
| `test_preload_fence` | Build-ID allowlist, refusal diagnostics, validation policy and guest-profile gates |
| `test_dx_path` | Literal VMware wire IDs, bounded DX context lifecycle, malformed payloads, reset and legacy-context isolation; no DX rendering |
| `test_buffer_ordering` | Strict Vulkan validation through teardown; queued buffers/constants, compressed FIFO/GMR transfers, sampler retirement, mip/image ordering, depth sampling and shader pixel regressions |
| `test_shader_translation` | Translation plus real Vulkan shader-module creation; malformed/unsupported bytecode rejected; `_SAT` handling |
| `test_real_vulkan` | Real device init and bit-exact buffer upload/download; no mock fallback |
| `test_svga3_vlkn` | Engine/unit suite incl. failure-injection of Vulkan dispatch (flush-error propagation) |
| `test_shader_execution` | Translated shaders execute under real Vulkan |
| `test_guest_memory` | Guest memory, GMR/translation, transfer paths |
| `test_malformed_inputs` | Malformed FIFO/input and resource-accounting rejection |
| `test_verified_rendering` | Analytical pixel/rendering scenes (repository-local; does not prove external GLES rendering) |
| `test_presentation` | Presentation/framebuffer paths |
| `test_qemu_integration` | Host-side integration only; no QEMU guest boots |

## QEMU integration limitations

`src/qemu_svga3d_preload.cpp` is an experimental, build-specific hook with guest-specific workarounds. Do not install it globally or preload it into an arbitrary QEMU build. It requires separate review of binary offsets, device layouts, capabilities, and guest-memory handling. This repository does not provide a supported Proxmox deployment procedure. Use disposable, isolated VMs for integration work.


The [renderer performance report](renderer-performance-20261004.md) records the renderer optimization, matched benchmark conditions, and retained Piglit/glmark2 accuracy.
