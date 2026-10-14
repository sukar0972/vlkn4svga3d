# PR graphics gate

## Policy and activation status

This is the project's required policy for activating the PR graphics gate.
The queue requirements below are requirements for implementation, not claims
that a scheduler is installed. Maintainers must keep this status current when
changing the workflow, evaluator, runner or repository rules.

| Item | Current state |
| --- | --- |
| Guest preparation, identity checks and cleanup | Implemented; exercised manually on Oracle |
| Focused Piglit followed by matched glmark2 | Implemented in the local draft evaluator |
| Performance threshold | Three samples per build; candidate median must equal or exceed base median |
| Piglit fixture repairs in the gate | Integrated; two direct fresh-overlay correctness controls passed |
| GitHub runner and required checks | Not activated |
| Queue admission, deduplication, priority and stale-run cancellation | Not implemented |
| Benchmark noise calibration and tolerance | Not implemented; 5% is a provisional proposal |
| Trusted evaluator, isolated execution and independent recovery | Required before untrusted PR activation |

See the [validation report](pr-graphics-gate-validation-20261008.md) and
[framebuffer diagnosis](oracle-alpha-test-diagnosis-20261009.md) and
[fixture validation](piglit-fixture-validation-20261010.md) for measured results.
Nested-worker startup still fails before testing; source-revision comparisons,
performance calibration and gate activation remain unfinished. The focused selection currently contains 146 top-level cases. Passing
it must never be described as passing all Piglit tests or full OpenGL conformance.

## Queue and merge requirements

The following rules apply to every PR admitted to the full graphics gate.
Queue position, a smoke-test pass or an infrastructure failure must never count
as a successful full gate.

1. Ordinary build and unit-test CI must pass before a PR enters the Oracle queue.
   Draft PRs do not enter automatically. Full testing requires ready-for-review
   status and maintainer admission of the exact candidate revision. Untrusted
   revisions require approval before execution; approval does not carry over
   to a new commit.
2. Keep at most one pending entry per PR. A new push replaces the pending entry
   with the latest revision. Queue entries record PR number, head SHA, exact
   base SHA, candidate merge SHA and environment/evaluator identities.
3. Reviewed PRs ready to merge take priority over other admitted PRs. Within
   each priority, use admission order. Updating a pending revision retains its
   position; withdrawing and readmitting a PR creates a new entry. Publish
   queue position and wait estimates based on completed run durations.
4. Run one graphics job at a time across all PRs and manual tests on Oracle.
   Stop the development guest gracefully before measurement, record its prior
   settings, and restore it after cleanup. Refuse to start if exclusive host
   ownership or the required workload conditions cannot be established.
5. An active job superseded by a push must stop at a safe cancellation point,
   retain partial evidence and complete cleanup before the next job starts.
   Never kill the worker without arranging cleanup. A superseded result cannot
   satisfy the merge requirement, even if it passed before cancellation.
6. A short graphics smoke test may provide early feedback. Every admitted PR
   still needs the full focused Piglit and matched glmark2 comparison before
   merging. The smoke check must have a distinct name and cannot replace it.
7. A changed base, candidate, evaluator, fixture or environment invalidates the
   full result. Retest the proposed merge against the current base before
   merging. If a GitHub merge queue is introduced, test its generated merge
   revision and support its events before requiring this check there.
8. Preserve the current result until a replacement attempt finishes. Publish
   every attempt, including failures, interruptions and superseded runs. A
   missing worker or long queue leaves the merge blocked.

GitHub workflow concurrency only serializes jobs; it is not the queue described
here. The draft uses `cancel-in-progress: false` to protect cleanup and does not
provide per-PR replacement or priority. A scheduler must own the host lock,
revision checks, cancellation and recovery before those features are advertised.
Persistent overload requires another isolated worker or reduced admission,
not parallel guest benchmarks on this small host.

## Pass criteria and retry policy

Correctness comes first. Both builds must pass the same focused Piglit cases
and subtests on SVGA and guest llvmpipe with Vulkan validation enabled. Existing
unsupported skips remain visible; a previously passing case becoming skipped
blocks the gate. Failures, warnings, crashes, timeouts, missing cases and
validation errors block progression to performance testing.

Repair the two documented Piglit fixture issues through reviewed, versioned
patches and validate the repaired tests on both rendering paths. Retain the
original results and patch hashes. Do not exclude these cases, waive their
failures or alter expected pixels merely to make the gate pass.

Performance must compare fresh base/candidate runs under matched conditions.
The current evaluator requires candidate median >= base median, with three
samples per build. It does not implement a 5% allowance or a noise check.
A proposed allowance is candidate median >= 0.95 * base median. Before adopting
it, record repeated base-versus-base comparisons, choose a fixed sample count
and variability limit, and review the threshold with its evaluator tests.
The allowance must not be selected separately for each PR. Excessive variation
must produce an inconclusive result that blocks merging. Historical scores
from another revision, machine or workload cannot be substituted for a baseline.

Report per-scene scores and changes alongside the overall score. Per-scene
thresholds remain advisory until their variability is measured and an explicit
policy is implemented. Matching renderer, version and scene coverage is required.

Classify results as pass, test failure, infrastructure failure, inconclusive or
superseded. Only a complete pass can satisfy the required check. Setup errors,
lost SSH and resource exhaustion are infrastructure failures; they do not prove
a renderer defect. A scheduler may automatically retry an infrastructure failure
once after successful cleanup and a host health check. Test failures require
investigation. Inconclusive benchmarks require investigation of measurement
conditions. Further attempts need a recorded maintainer reason; retain all
attempts and never select the highest score or rerun until a pass appears.
The current draft reports errors without implementing this classification or
retry scheduler.

## Evidence, ownership and activation

Each full result must identify both source revisions, the merge revision, build
flags, loaded-library hashes, evaluator and hook hashes, environment manifest,
Piglit selection and fixture patches. Publish counts and failing cases, all
benchmark samples, medians, percentage change, sample spread, per-scene results,
cleanup/restoration status and links to raw logs. Cache baseline Piglit only
when its source, build, selection, fixtures, evaluator and environment identities
all match, with complete passing evidence. glmark2 must retain fresh paired
baseline measurements. Baseline caching is not implemented in the draft.

Maintain an administrator-owned evaluator and hook outside the candidate
checkout. PR code must not choose pass thresholds, test selection or the code
that publishes required results. Keep reporting credentials outside the worker;
restrict result publication to the tested revision. The draft currently invokes
`candidate/scripts/run_pr_graphics_gate.py` and needs this corrected before
activation. Review changes to the gate policy and trusted evaluator separately
from the candidate whose result they assess.

The worker must be isolated and recreated after untrusted jobs. The library runs
in host QEMU, so a new guest overlay alone does not reset the host. An independent
watchdog must handle job timeouts, lost runners and interrupted cleanup, retain
evidence, remove owned processes/overlays, and restore the development guest.
Failed cleanup or restoration keeps the worker unavailable until an operator
verifies recovery. Require broader scheduled Piglit testing separately and retain
its selection and limits; it does not substitute for a PR's focused gate.

Before activation, maintainers must validate fixture repairs, complete a clean
full base-versus-base run, calibrate benchmark variation, implement and exercise
the trusted queue and recovery requirements, then configure required GitHub
checks with revision-bound results. Keep ordinary CI required as well. A workflow
file alone does not enforce merging. Split correctness and performance checks
may be introduced, but both must be required and performance must wait for
correctness. The draft currently exposes one combined check.

## Current evaluator behavior

`Guest graphics / Piglit and glmark2 gate` compares the exact PR base with the
candidate merge commit. It builds both with `make preload-lab`, identical
release flags (`-O2 -DNDEBUG`) and fresh output directories, runs the
existing focused Piglit selection on SVGA3D and llvmpipe with Vulkan validation
enabled, then measures glmark2 with validation explicitly disabled. Both Piglit
runs must exit successfully: failures, warnings, crashes, timeouts, incomplete
results and empty/all-skipped runs fail the check. Completed results must also
contain exactly the planned cases. The same cases and subtests
must run on both builds. Setup/discovery commands have a 120-second timeout;
individual tests have a 600-second timeout and each renderer run has a one-hour
limit. A previously passing case/subtest cannot become a
skip. Existing unsupported skips remain skips, not accuracy passes.

Performance uses three fresh-guest samples per build, alternating base/candidate
order and reversing the middle pair. Each sample warms the full default glmark2
selection for 0.5 seconds per scene, then measures one second per scene at
800×600 with vblank disabled. Renderer, glmark2 version and rendered scene list
must match. The candidate median score must be **greater than or equal to the
base median**, with no percentage tolerance. Small changes and noisy machines
can fail this strict policy; preserve evidence when rerunning. Historical scores
from a different VM or configuration are not the baseline.

A physical GPU is optional for this comparison. The host can use Mesa lavapipe
to execute the library's Vulkan commands on the CPU while the guest continues
using Mesa SVGA. Pin the host Vulkan ICD, Mesa/LLVM versions and worker-thread
count for both builds and record them in `conditions`. Guest llvmpipe remains
the separate OpenGL reference; using it for the accelerated run would bypass
the library. Scores from this setup measure software-rendering performance.
Shared cloud CPU contention can still move the median between runs.

## Runner setup

The workflow is stored under `.github/workflow-examples/` and is not active
GitHub Actions configuration. Move it into `.github/workflows/` only after
provisioning the isolated runner, approval controls and scheduler described
below. Ordinary hosted CI remains active.

GitHub-hosted Ubuntu runners cannot exercise this library's guest SVGA/QEMU
path. Register a dedicated, disposable Linux x64 self-hosted runner with label
`vlkn-guest-graphics`. Install Python 3, a C++ build toolchain and the build
dependencies. Give it a disposable guest with Xorg, SSH, Piglit, glmark2,
mesa-utils, the appropriate Mesa SVGA driver, and the supported QEMU binary on
a Vulkan-capable host. Pin guest/package versions and machine settings.

Create the GitHub environment `guest-graphics`. For repositories accepting
untrusted PRs, require maintainer approval in this environment before any PR code
executes. PR Makefiles and libraries execute arbitrary code: the **runner as
well as the guest must be disposable and isolated from production credentials,
networks and VMs**. Rebuild the runner after each job. Do not connect this job
to the shared development VM119. Fork workflows also need GitHub's normal
workflow-run approval. The workflow grants only read access and does not persist
checkout credentials.

Install an administrator-owned executable at `/opt/vlkn-ci/guest-hook`, outside
the checkout. Provisioning is site-specific because this repository's preload
adapter only supports explicitly reviewed QEMU builds; there is no generic
Proxmox deployment command. The hook supports two operations:

The prepared Oracle/lavapipe host has a concrete
[hook implementation and installation guide](../scripts/graphics/README.md).
Its independent runtime and SSH port allow trusted manual preflight without
restarting the development guest. It still requires disposable-host provisioning
before running untrusted PRs.

```text
guest-hook prepare --library /absolute/libqemu_svga3d.so --validation 0|1 --output /absolute/evidence-directory
guest-hook cleanup --output /absolute/evidence-directory
```

`prepare` must boot/reset an isolated guest with the **provided library**, set
`SVGA3_VLKN_VALIDATE` to the requested value in the QEMU process environment,
disable FIFO tracing, wait for SSH/Xorg, and verify the loaded library's SHA-256
and actual validation setting. Never patch candidate sources or substitute a
previously installed adapter. Record launch commands, loaded-process checks and
machine settings in the output directory. Return nonzero for any failure.

Write `guest.json` in that directory with this shape (values are examples):

```json
{
  "ssh": "graphics-ci@192.0.2.10",
  "display": ":0",
  "mesa_driver_path": "/opt/mesa-vlkn/lib/x86_64-linux-gnu/dri",
  "extended_state": true,
  "loaded_library_sha256": "SHA256_OF_THE_LIBRARY_VERIFIED_IN_THE_QEMU_PROCESS",
  "validation": 0,
  "conditions": {
    "host_gpu": "fixed GPU and driver version",
    "qemu_build_id": "reviewed build ID",
    "guest_image": "immutable image digest",
    "mesa": "fixed package/build digest",
    "piglit": "fixed package/build digest",
    "cpu_affinity": "fixed host and guest CPU allocation",
    "gpu_clocks": "fixed frequency policy",
    "desktop": "1920x1080",
    "background_load": "dedicated idle host"
  }
}
```

Omit `mesa_driver_path`/`extended_state` when using the ordinary guest Mesa
driver. All recorded conditions, display and Mesa/protocol options must match
between accuracy and performance runs and both builds. Guest addresses can
change. The hook's process-level checks are necessary: guest renderer strings
alone cannot identify which host library is loaded.

`cleanup` must stop and remove the disposable guest and any per-run resources,
including after a partially failed `prepare`. It must be idempotent. The runner
calls it in a `finally` block for each prepared stage; cancellation, runner loss
and job timeout still require an external janitor. Each operation has a ten-minute
timeout. No-op hooks or fabricated identities do not provide valid testing.

To check the hook before enabling merge enforcement, run a manual workflow on
the default branch: this compares that commit against itself. Alternatively:

```sh
python3 scripts/run_pr_graphics_gate.py \
  --base /path/to/clean/base-checkout \
  --candidate /path/to/clean/candidate-checkout \
  --guest-hook /opt/vlkn-ci/guest-hook \
  --output /path/to/new/evidence-directory
```

The workflow uploads build logs, hook verification, raw Piglit results, renderer
identities, glmark2 warmup/timed logs and `summary.json` even on failure. A missing
runner or hook never produces a successful check. No live guest tests are run
by the ordinary reporting-unit-test job.

`summary.json` also records library hashes, matched conditions and completion/
cleanup status for every attempted stage. Cleanup failures retain the original
stage error. Warmup and timed runs must report the same renderer, version and
scene list. The ordinary CI job tests failed preparation, Piglit/benchmark
failures, changing conditions, hash/validation mismatches, and cleanup ownership
without booting a guest.

## Merge enforcement

Once a real base-versus-base hardware run passes, add **Piglit and glmark2 gate**
to the default branch's required status checks/ruleset and require branches to be
up to date before merging. Also retain the ordinary `mock-and-translator` check.
Disable bypasses if this must apply to administrators too. Protect changes to
`.github/workflow-examples/guest-graphics.yml`, `scripts/run_pr_graphics_gate.py`,
`scripts/run_piglit.py` and the gate tests through required maintainer review;
PR authors can otherwise weaken a repository-owned workflow or evaluator.
Workflow files alone do not prevent merging until the repository rules require
the check. All PRs run it, with no path filter that could leave a required check
skipped. Concurrency serializes access to the dedicated hardware; GitHub may
replace pending runs, so rerun a displaced PR's workflow before merging it.
