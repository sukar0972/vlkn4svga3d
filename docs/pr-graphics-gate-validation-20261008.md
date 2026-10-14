# PR graphics gate validation — 2026-10-08

The gate builds the exact base and candidate with common release flags in fresh
output directories. It requires successful focused Piglit runs on SVGA and the
guest llvmpipe reference before comparing three interleaved glmark2 samples per
build. Candidate median score must be at least the base median. Missing evidence,
new skips, changed selections/conditions and failed cleanup fail the check.

Local verification passed 32 tests: 16 gate tests, six Oracle hook tests and ten
Piglit reporting tests. Coverage includes the complete eight-stage sequence,
cleanup after preparation/test/benchmark failures, invalid identities/settings,
changed conditions and warmup identities, score regressions, missing planned
cases, reused PIDs, guest ownership, validation failures and timeout descendants.
Both workflow files parsed as YAML; `git diff --check` passed. The existing
`make test` suite and release adapter build also passed during this work.

## Oracle setup

Trusted manual testing used the prepared Oracle host with lavapipe, one guest
vCPU, 512 MiB guest RAM, and `LP_NUM_THREADS=1`. An independent runtime at
`/opt/vlkn-ci/qemu-gate-root` and localhost SSH port 2223 kept the development
guest's runtime and port 2222 unchanged. The host has only 956 MiB RAM; both
guests remained running, causing heavy swapping and slow test execution.

The administrator hook creates a fresh overlay, clears its shader cache, stages
the supplied adapter and starts QEMU as `graphics-ci` with a cleared environment.
It checks the mapped library inode/hash, lavapipe, actual validation-layer
presence, SSH/Xorg and SVGA identity. Conditions include image/QEMU/ICD/seed/hook
hashes, guest packages and the Mesa module hash. Cleanup verifies the owning
stage and process start time, stops QEMU, saves logs and removes the overlay.

Two clean source snapshots were made from the current workspace, whose parent
is `0a09f02`. This is a base-versus-base infrastructure check of that workspace,
not a comparison against the unmodified default branch. Both release builds
produced adapter SHA-256
`26d2ca757188ff392e77e0fee9496114f4e6936674d58586be6fa4430f0e53df`.
Source manifests and snapshot revisions are retained with the evidence.

## Old-driver control

The first complete replay used the base image's original Mesa module
`2c4bb2130da43a1e6f81904ef361538349025e15a287200170cccb0dfb0de43f`.
With Vulkan validation actually enabled, it completed all 146 planned cases:

| Renderer | Pass | Fail | Skip | Timeout |
| --- | ---: | ---: | ---: | ---: |
| SVGA | 130 | 11 | 5 | 0 |
| Guest llvmpipe | 146 | 0 | 0 | 0 |

Failures were eight ordinary/projected 1D/2D shadow cases, `texdepth`,
`fbo-blending-format-quirks`, and the dated `depth_stencil texture` API expectation.
The framebuffer test expected alpha zero at `(128,128)` on
`GL_COLOR_ATTACHMENT1`, but observed `0.741176`. The API case returned
`GL_INVALID_ENUM` where the fixture expected `GL_INVALID_VALUE` for unsized
depth/stencil texture storage in its compatibility context.

The gate returned failure, recorded no performance samples, and reported
`cleaned_up: true`. Cleanup found no Vulkan validation errors or warnings.
The development guest remained running. This demonstrates rejection of a
real failing baseline; it does not qualify the gate for merge enforcement.

## Setup corrections

Live preflight found and fixed two launch/setup problems. QEMU retained a
captured stderr pipe after daemonizing, causing the hook launcher to wait;
launch output now goes directly to a file. The base image lacked `xdpyinfo`, so
readiness now uses its installed `glxinfo` after checking the Xorg service.
The initial build attempt also exposed a missing host `make` dependency.

The hook can pin a reviewed host Mesa module into each fresh overlay and verify
its guest hash. The corrected module is
`88a1b2409b87b946843133ff28b1c0cf382dd21436b69e9a3507ce8ecaa7381a`,
matching the retained renderer's documented depth/shadow fixes. The shared base
disk is unchanged. Its first prepare passed all identity checks, but Piglit
enumeration exceeded the existing 30-second setup timeout on this host. That
failed attempt and successful cleanup were preserved. The gate now allows
120 seconds for setup; per-test and renderer-run limits remain 600 and 3600
seconds. The standalone Piglit runner's setup default remains 30 seconds.

## Corrected-driver replay

The final replay used the corrected module, snapshot revision
`48b46871f76a310041fd6b65d7c9e3171b7fdf54` in both checkouts, and the same
adapter hash recorded above. The hook verified the actual validation-layer
mapping with validation enabled. Both renderers completed exactly all 146
planned cases, with no missing/unexpected cases or timeouts:

| Renderer | Pass | Fail | Skip | Timeout |
| --- | ---: | ---: | ---: | ---: |
| SVGA | 139 | 2 | 5 | 0 |
| Guest llvmpipe | 146 | 0 | 0 | 0 |

All eight shadow-texture failures and `texdepth` from the old-driver control
passed. Two failures remain, and both pass on llvmpipe:

- `fbo-blending-format-quirks` still observes alpha `0.741176` where zero is
  expected on `GL_COLOR_ATTACHMENT1`. This pixel-probe failure was unresolved
  at replay time; the subsequent diagnosis below identifies uninitialized test
  input rather than an incorrect renderer blend result.
- `depth_stencil texture` still returns `GL_INVALID_ENUM` instead of the
  fixture's expected `GL_INVALID_VALUE` for unsized depth/stencil storage.
  Its dated compatibility-context expectation is recorded, not waived.

SVGA subtests reported 149 passes and 160 skips; llvmpipe reported 309 passes.
The five top-level unsupported SVGA skips remain visible in the raw summary.
No Vulkan validation errors or warnings were found during cleanup.

The strict gate returned failure at base accuracy, recorded `cleaned_up: true`,
and collected no candidate accuracy or performance samples. It correctly
refused to advance past a failing baseline. A successful full eight-stage
base-versus-base run remains required before merge enforcement can be enabled.

## Validation-off collector smoke

After the failed gate had cleaned up, a separate fresh guest ran the gate's
actual glmark2 environment, command builder and log parser with validation set
to zero. Process maps confirmed that the validation layer was absent and the
same adapter was loaded. The machine/module/image conditions exactly matched
the corrected accuracy run.

glmark2 2023.01 completed all 32 default scenes at 800×600 with vblank disabled.
Warmup (0.5 seconds per scene) scored 17; the timed run (one second per scene)
scored 32. Renderer, version and scene list matched between phases. Cleanup
succeeded with no Vulkan validation messages, and the disposable runtime's
guest directory was removed. The original development guest, PID 8462, remained
running throughout.

This verifies the real validation-off collector path only. One sample has no
candidate comparison or three-sample median and is not a successful performance
gate. The full gate's performance samples remain empty.

Evidence for the completed control is in
`artifacts/oracle-ci-preflight/gate-control/`. Remote run evidence and source
snapshots are in `/home/ubuntu/vlkn-ci/gate-validation-20261008/`. The supplied
historical hook source matches the control's recorded SHA-256
`4d75c826be481ef6e90f9545f24ad7173af22b5b60a4de8d315c91e169c8c94f`.
The corrected replay, failed 30-second setup attempt and collector smoke were
downloaded to `artifacts/oracle-ci-preflight/gate-final/`, including raw results,
process maps/settings, logs, source manifest, and installed hook/configuration.
The installed hook matches the recorded SHA-256
`44de730855dffb56bdc0632e172f7ae03bf8979fe6204f8630736997b7f0640d`.

## Single-guest replay — 2026-10-09

After the operator authorized a temporary shutdown, the development guest was
powered off gracefully and its disk retained. The gate then rebuilt both clean
snapshots and repeated the full accuracy selection with only its disposable
guest running. Adapter hashes and all pinned conditions matched the previous
corrected-driver replay except the recorded background workload. Some swap was
still used on the 956 MiB host; this was not a claim of zero memory pressure.

The results were unchanged: SVGA **139 pass, two fail, five skip**; guest
llvmpipe **146 pass**. Both completed exactly the 146 planned cases with no
timeouts or missing/unexpected results. The framebuffer test again observed
alpha `0.741176` instead of zero, and the unsized depth/stencil storage test
again returned `GL_INVALID_ENUM` instead of `GL_INVALID_VALUE`. Stopping the
development guest did not remove either failure.

The gate returned exit status 1 at base accuracy, skipped candidate accuracy
and performance, and completed cleanup without Vulkan validation errors or
warnings. The temporary guest directory was removed. The administrator hook's
original configuration was restored byte for byte.

The development guest restarted using its existing disk and unchanged adapter,
with validation enabled as observed immediately before shutdown. Its new PID
was 29738; SSH, `graphics-xorg.service`, and SVGA3D `glxinfo` were verified after
restart. Lifecycle records, raw results, logs and both release builds are saved
in `artifacts/oracle-ci-preflight/gate-single-guest/` and the remote
`/home/ubuntu/vlkn-ci/gate-validation-20261008/single-guest-replay/` directory.

## Subsequent framebuffer diagnosis — 2026-10-09

Targeted controls identified an initialization bug in the packaged Piglit test:
its loop clears attachment 0 twice and leaves attachment 1 undefined before
blending with destination alpha. Explicitly clearing attachment 1 to zero passes
on unchanged Oracle SVGA, VM119 SVGA, and both guest llvmpipe references.
Initializing it to 0.75 reproduces the failure on all paths. The Oracle
observation control returned the same 0.125490 alpha before and after blending;
an isolated original run also passed. This explains the environment-dependent
pixel failure without establishing a renderer regression. See the
[diagnosis and controls](oracle-alpha-test-diagnosis-20261009.md).
At the close of that diagnosis, the gate still used unmodified fixtures and
retained the raw failed result. The
[October 10 validation](piglit-fixture-validation-20261010.md) records subsequent
fixture builds, installation checks and fresh-overlay controls.

The workflow is a local draft. No GitHub runner or required merge check has been
activated. Activation requires a clean full baseline and disposable-host
provisioning for untrusted jobs as described in [the gate guide](pr-graphics-gate.md).
