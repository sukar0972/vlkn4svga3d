# Oracle guest hook

`oracle_guest_hook.py` implements the gate's prepare/cleanup contract for the
runtime and Debian graphics image described in
[the Oracle host notes](../../docs/oracle-graphics-runner.md). Install a reviewed
copy outside the checkout. This hook is suitable for trusted manual preflight on
that host. It does not make the host safe for executing untrusted PRs; follow the
disposable-runner requirements in [the gate setup](../../docs/pr-graphics-gate.md)
before registering a GitHub runner.

Provision an independent QEMU chroot at `/opt/vlkn-ci/qemu-gate-root` with the
reviewed QEMU executable, matching shared libraries, firmware, Mesa lavapipe and
Vulkan validation layers. Do not reuse a running development guest's chroot:
the adapter's `/tmp/svga3d.log` and shader cache belong to each runtime. Bind
`/dev/null`, `/dev/zero`, `/dev/random`, `/dev/urandom`, and `/dev/kvm` into its
`dev/` directory and mount proc at its `proc/`. Restore these mounts after a
host restart. Create `run/`, `images/` and a mode-1777 `tmp/` directory. The host
also needs Python 3, `qemu-img`, `chroot`, `runuser`, and a `graphics-ci` account
with access to the host's `kvm` group.

Place the stopped, immutable graphics base image under the runtime's `images/`
directory and retain its cloud-init seed ISO. The guest needs a running
`graphics-xorg.service`, `glxinfo`, Piglit, glmark2, and the reviewed
opt-in Mesa module at `/opt/mesa-vlkn/dri/vmwgfx_dri.so`. Prepare an SSH alias for
the operator named by `ssh_user` using a guest-only key and the base image's
verified SSH host key. Use port 2223 for this runtime, keeping the development
guest's port 2222 separate. The hook calls SSH through `runuser` as that operator;
the gate itself must also be able to use the configured alias.

Install the hook and configuration as root-owned files:

```sh
sudo install -m 0755 scripts/graphics/oracle_guest_hook.py /opt/vlkn-ci/oracle-guest-hook.py
sudo install -m 0644 scripts/graphics/oracle-hook.example.json /opt/vlkn-ci/guest-hook.json
```

Edit the configuration for the actual host and image. If `mesa_driver` is set,
install the reviewed corrected module at that host path, readable by `ssh_user`.
The hook copies it into each disposable overlay and verifies its guest SHA-256;
the shared base image stays untouched. Omit this option when the base image
already contains the correct module. The `base_image` must live
inside `runtime`: overlays use a relative backing path that works both on the
host and inside the chroot. Record fixed workload conditions rather than retaining
the example's placeholder. Install a root-owned executable `/opt/vlkn-ci/guest-hook`
that calls the administrator script with its original arguments. For the trusted
Oracle operator, which already has passwordless sudo, its contents are:

```sh
#!/bin/sh
exec sudo -n /usr/bin/python3 /opt/vlkn-ci/oracle-guest-hook.py "$@"
```

The optional `piglit_fixtures` configuration entry names an administrator-owned
bundle containing the two repaired Debian 20220119 Piglit tests. Build it on
Debian 12 with the matching compiler/runtime and Piglit build dependencies
(`cmake`, `make`, `g++`, `pkg-config`, `patch`, `libgl1-mesa-dev`, `libwaffle-dev`,
`libpng-dev`, `libx11-dev`, `libgbm-dev`, `libdrm-dev`, `libxkbcommon-dev`,
`python3-mako`, and `python3-numpy`):

```sh
python3 scripts/graphics/build_piglit_fixtures.py \
  --source /path/to/piglit_0~git20220119-124bca3c9.orig.tar.xz \
  --output /path/to/new-fixture-build
```

The script checks the source archive SHA-256, applies both versioned patches,
builds only the two test targets and their utility libraries, and writes a
`fixtures/manifest.json` containing source, patch, binary, library and wrapper
hashes. Retain its build log and source tree. Install the resulting `fixtures/`
directory outside the checkout and set `piglit_fixtures` to its absolute path.
The hook verifies the bundle before transfer and every transferred hash before
replacing either test. It requires guest Piglit `0~git20220119-124bca3c9-1`.
It preserves the original executables under `/opt/vlkn-ci/piglit-original` in
the disposable overlay and records their hashes and the bundle manifest in
`piglit-fixtures.json`. The manifest also becomes part of the matched conditions
in `guest.json`; a fixture change invalidates the comparison. Omitting this
option runs the original packaged tests. Keep the original failed-run evidence
alongside corrected results.

Each prepare creates a fresh overlay, clears the runtime's shader cache, stages
the exact supplied library, and launches QEMU as `graphics-ci` with a cleared
environment, one vCPU, 512 MiB RAM, and `LP_NUM_THREADS=1`. It waits for SSH/Xorg
and checks SVGA3D, the library mapping's inode and SHA-256, lavapipe, and whether
the validation layer is actually mapped. `guest.json` records the base-image,
QEMU, ICD, seed and guest-Mesa hashes, guest package versions and machine settings.
Raw process maps, settings and launch arguments are retained alongside it.

Cleanup checks the owning evidence directory and process start time before
signaling QEMU. It retains QEMU, serial and adapter logs before deleting the
overlay and staged library. Vulkan validation errors or warnings fail cleanup
after resources are removed. Repeating cleanup for an already removed stage
succeeds. A different stage's owner is never cleaned up. After cancellation or
operator interruption, call cleanup using the original stage directory recorded
in `run/guest/owner.json`; retain that directory for evidence. Runner loss still
requires an external janitor.

Verify both validation modes, then use the full gate against two clean copies of
the same source revision before configuring merge enforcement. Do not replace
the focused Piglit selection with the earlier two-case setup check. Run the local
regression tests with:

```sh
python3 -B -m unittest discover -s tests -p test_pr_graphics_gate.py
python3 -B -m unittest discover -s tests -p test_oracle_guest_hook.py
```
