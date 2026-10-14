# Library changes and validation — 2026-10-14

The library changes preserve the VM's approximately 650 glmark2 score and pass
the selected Piglit replay with the existing corrected fixtures. This report
covers the library changes based on `d7273605db53d636375873e002d12ca7ed94e0f9`.

## Changes delivered

- Declared GPL-2.0-or-later for original project material in `LICENSE.md`,
  preserving existing third-party notices and the GPL text in `COPYING`.
- Added a versioned C configuration and creation API, initialization diagnostics,
  explicit submission, a C embedding example, and installed-consumer checks.
- Added `PREFIX`/`DESTDIR` installation and pkg-config metadata for consumers.
- Made capability reporting depend on the initialized Vulkan device and format
  support. Corrected the float encoding of the maximum point-size capability;
  unavailable devices report no capabilities and no existing resources.
- Added three fenced command-buffer slots for asynchronous submissions, with
  conservative resource retirement, upload ordering and synchronous drains at
  readback, guest fences, memory reuse and destruction.
- Added a source integration for QEMU 9.2.0 with PCI guest-memory and display
  callbacks, bounded FIFO handling, GMR2 transfers, fences and reset tests.
- Expanded host, install and native-QEMU CI checks and source-hashed acceptance
  evidence. Tests cover submission-slot reuse, staging wrap, temporary uploads,
  draw ordering and public API errors.

## VM results

VM 119 on `10.0.0.200`, guest `10.0.0.144`, remains running the tested adapter.
The guest renderer was verified as SVGA3D. Only this VM's adapter was replaced;
the global adapter was preserved and a rollback copy remains on the host.

| Matched glmark2 run | Scores | Median |
| --- | --- | ---: |
| Baseline | 652, 645, 645 | 645 |
| Final library | 639, 647, 652 | 647 |

The median change is +0.31%, with overlapping ranges. This preserves measured
performance; it does not establish a reliable speedup. Runs used an 800×600
window, vblank disabled, a warmup followed by three runs at one second per scene,
and the same 32 supported scenes under shared host load. Terrain is unsupported.
The separate glmark2 validation run reported 27 Success, six Unknown and zero
Failure results. Unknown results are not counted as validation passes.

| Selected Piglit replay | Pass | Skip | Fail/crash/timeout |
| --- | ---: | ---: | ---: |
| Final SVGA3D | 141 | 5 | 0 |
| Guest llvmpipe control | 146 | 0 | 0 |

Both runs completed the exact 146-case selection with no missing or unexpected
cases. The five SVGA skips match the corrected-fixture baseline. These are
results for the selected replay, not the entire upstream Piglit suite.

The replay uses the existing versioned fixture repairs for a dated depth/stencil
error expectation and an uninitialized framebuffer-blending fixture. The
unmodified Debian baseline is retained separately: 140 passes, five skips and
one failure. Its failure must not be represented as a renderer regression fixed
by this change. The final candidate was tested with the corrected fixtures.

Live VM timing and replay used `SVGA3_VLKN_VALIDATE=0`. Host and native tests
below enabled validation. The final host inspection found no matching GPU hang,
reset or fence-timeout events in the test interval.

The deployed adapter SHA-256 is
`8f9c0e70c2742288254bd4a9a069dbfb55668a1fbf8995eb7ea3a158a9aed5e4`.
The guest Mesa module remains
`88a1b2409b87b946843133ff28b1c0cf382dd21436b69e9a3507ce8ecaa7381a`.

## Host and native integration results

`make acceptance` passed all 16 Vulkan harness suites, mock/reference checks,
SPIR-V validation, the C API test and the staged installed-consumer test.
Synchronization validation was enabled. All 36 Python unit tests also passed.
The acceptance manifest records hashes of the tested source and headers.

The native adapter built against the checksum-verified official QEMU 9.2.0
source. Actual QEMU PCI/FIFO tests passed framebuffer pixel checks, RAM DMA in
both directions, wrapped and incomplete fences, invalid FIFO bounds, capability
encoding, reset and migration rejection. A bare-metal guest cold-booted and
repainted successfully after a machine reset. Both QEMU processes exited cleanly;
Vulkan synchronization validation reported no errors or warnings.

Native QEMU was not tested with a Linux desktop. The VM Piglit and glmark2
results exercise the designated lab preload adapter. Native migration and
snapshots remain blocked, device loss requires a QEMU restart, and the source
patch has not been accepted upstream. See the
[native integration instructions](../integrations/qemu/README.md).

## Retained evidence

These paths are local generated artifacts, not source-controlled fixtures:

- `artifacts/acceptance-20261014T190025Z-XoSZyk/`: source manifest, driver identity
  and individual host-suite results.
- `artifacts/acceptance-20261014T220822Z-cqK7G9/`: successful pre-merge acceptance
  rerun, including the fresh-checkout evidence-directory fix.
- `artifacts/library-upgrade/acceptance-verified.log` and `python-all.log`:
  acceptance and Python test output.
- `artifacts/library-upgrade/verified-glmark2-{1,2,3}.log` and
  `verified-glmark2-validation.log`: final benchmark and validation output.
- `artifacts/library-upgrade/baseline-glmark2-{1,2,3}.log`: matched baseline.
- `artifacts/library-upgrade/piglit-verified/`: final raw replay, HTML and summary.
- `artifacts/library-upgrade/piglit-baseline-corrected/` and `piglit-baseline/`:
  corrected and unmodified baseline evidence.
- `artifacts/library-upgrade/guest-fixture-identity.txt`: fixture manifest and hashes.
- `artifacts/library-upgrade/native-qemu-results/`: native result and validation logs.
- `artifacts/library-upgrade/verified-runtime-{start,final}.txt`: VM state,
  adapter identity, validation setting and host checks.

Earlier rejected builds and their results remain under the same artifact root;
only the `verified-` benchmark and `piglit-verified` replay apply to the final
deployed adapter hash above.
