# Piglit fixture validation — 2026-10-10

Both versioned fixture repairs were rebuilt from Debian Piglit
`0~git20220119-124bca3c9-1` and passed isolated checks on Oracle SVGA and guest
llvmpipe. The framebuffer repair initializes both attachments before testing
destination-alpha blending. The depth/stencil repair accepts either applicable
API error for the documented unsized-storage call. Neither patch changes
expected pixels, excludes cases or waives a failing result.

The source archive SHA-256 is
`322e1b88aae35f92e15893b19e51ecd7854312f45e44c20c7a10cdde2adac454`.
The patch hashes are:

| Patch | SHA-256 |
| --- | --- |
| `piglit-20220119-fbo-blending-init.patch` | `8c7c2fc1391917ab106a8a13017dc2d5d42d9866f359be0b7b43b74908201bc8` |
| `piglit-20220119-depth-stencil-error.patch` | `2d84f2f2ac5715333ea0f7e99581956865976a1a935763b196b7676c04058fc9` |

The initial build command used the wrong depth/stencil target name and stopped
after building the framebuffer test. The corrected
`ext_packed_depth_stencil-depth-stencil-texture` target then built successfully.
Both build outputs, commands and isolated results are retained. These isolated
checks used Vulkan validation disabled; they do not establish a full gate pass.

The [build script](../scripts/graphics/build_piglit_fixtures.py) reproduces the
bundle from the pinned archive and both repository patches. The administrator
hook now accepts `piglit_fixtures`, checks the complete bundle before transfer,
verifies transferred hashes before replacing either test, and retains original
executables in each disposable overlay. Evidence records original binary hashes,
package version and the fixture manifest. The manifest is part of the conditions
that must match across controls. See the [hook setup](../scripts/graphics/README.md).

The script also completed an independent build in a Debian 12 container with
GNU 12.2 and Python 3.11.2. Both targets and utility libraries built, and all ten
bundle files passed manifest verification. That check caught and corrected the
use of tar extraction filters absent from Debian's Python, and completed the
dependency list with `make`, `libxkbcommon-dev` and `python3-numpy`. Archive
extraction now rejects links, special files and escaping paths explicitly.
Failed attempts and the successful build log/bundle are retained under
`builder-reproduction/`; the temporary build container was removed. Oracle
controls use the original Oracle-built bundle, whose exact hashes are recorded
with every run. Container compilation alone does not establish guest pixels.

## Fresh-overlay correctness controls

The controls reuse the verified release adapter
`26d2ca757188ff392e77e0fee9496114f4e6936674d58586be6fa4430f0e53df`
and corrected guest Mesa module
`88a1b2409b87b946843133ff28b1c0cf382dd21436b69e9a3507ce8ecaa7381a`.
Each overlay runs the same full focused selection with Vulkan validation enabled.

Both fresh overlays completed all 146 planned top-level cases on both paths.
Each recorded the same counts:

| Path | Pass | Fail/crash/timeout | Skip |
| --- | ---: | ---: | ---: |
| SVGA | 141 | 0 | 5 |
| Guest llvmpipe | 146 | 0 | 0 |

SVGA recorded 149 passing and 160 skipped subtests; llvmpipe recorded 309 passing
subtests. Both repaired fixtures passed. The selection had no missing or
unexpected cases. Comparing the original unmodified-fixture replay with this
control retained every case/subtest and introduced no skip of a previously
passing result. Cleanup completed without Vulkan validation errors or warnings.

The paired comparison passed with identical recorded conditions, including the
adapter, guest Mesa, fixture manifest, original executable hashes and hook.
Both overlays cleaned up successfully. The downloaded evidence also passed the
local evaluator comparison, and both overlays independently retained all cases
and subtests from the October 9 unmodified-fixture replay without new skips.
These controls assess correctness of the supplied artifact. They do not rebuild two source revisions or compare glmark2
performance, and do not constitute a complete PR gate pass.

The full pair used hook SHA-256
`32a4fb5d70ce1cfc95751c2759040ece656e1a4f21bc18026bc697dec42f107f`.
The final hook adds rejection of directory and broken symlinks in fixture
bundles; its SHA-256 is
`42033c40c2bceb76f19a46c0ee1fef11e96f9920703dfc47de0498557e69df78`.
This integrity guard passed the local hook tests, and both retained bundles
validate under it. The full pair was not interrupted to change hook identity.

A separate fresh-overlay smoke with the final hook selected exactly
`spec@ext_framebuffer_object@fbo-blending-format-quirks` and
`spec@ext_packed_depth_stencil@depth_stencil texture`. Both cases passed on
SVGA and guest llvmpipe with Vulkan validation enabled, and cleanup passed.
This verifies the final hook with the repaired tests; the full 146-case pair
remains evidence for the earlier hook identity. The smoke did not rerun the
full selection or performance measurements.

Two earlier smoke attempts are retained: the first executable-name selector
missed the depth/stencil case, and the second also selected two related GLES
cases. The second attempt failed its exact-selection assertion. The final
anchored selectors and assertion verify both intended cases with no extras.
Evidence is under `final-hook-smoke-v3/`; earlier attempts are not counted as
complete two-case controls.

## Nested worker startup

The previously provisioned Debian worker exposed `/dev/kvm` and enabled nested
AMD virtualization. Its inner guest made slow boot progress but did not reach
SSH/Xorg within the hook's 360-second startup limit. The control failed before
any Piglit test ran. Cleanup succeeded and removed its inner overlay and staged
adapter. This is an infrastructure failure. The nested-worker configuration is
not validated for gate activation.

The subsequent trusted manual controls use the previously working direct Oracle
runtime with the same pinned adapter. The bootstrap worker is stopped during
these controls. The development guest was already stopped when this turn resumed.
The supervisor restored the original hook/configuration after the paired run,
but then failed reading the worker PID file owned by `graphics-worker`. Its
service exit also stopped the restarted worker. Recovery starts the bootstrap
worker under a separate systemd service so the validation supervisor cannot
terminate it when exiting. After the final hook smoke, byte comparisons verified
the original hook/configuration, the worker remained active under
`vlkn-bootstrap-restored.service`, and its SSH health check passed on kernel
`6.1.0-53-amd64`. The graphics overlay and staged adapter were absent. These
checks are recorded in `direct-control-lifecycle.json`. The development guest
remains stopped, as it was when this work resumed.

Local verification passed 36 tests: 16 evaluator tests, ten Piglit runner tests
and ten hook tests. The added hook coverage checks corrupted bundles, missing or
unlisted files, escaping symlinks and retention of fixture/original-test hashes.
Both patches also applied cleanly to a fresh extraction of the pinned archive.

Evidence is retained locally under `artifacts/pr-gate-activation-20261010/` and
on Oracle under `/home/ubuntu/vlkn-ci/activation-20261010/`. The original failed
replays and [framebuffer diagnosis](oracle-alpha-test-diagnosis-20261009.md)
remain available. No GitHub runner or required merge check was activated.
