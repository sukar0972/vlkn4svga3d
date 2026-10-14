# Oracle graphics test host

The spare Ubuntu host at `40.233.104.74` was inspected on 8 October 2026.
It has approximately 956 MiB RAM, two exposed AMD EPYC logical CPUs, and a
48 GiB root disk. `/dev/kvm` is available and nested QEMU/KVM startup succeeded.
There is no physical GPU exposed. Mesa's lavapipe Vulkan ICD works; its device
string uses the name `llvmpipe`, despite this being the Vulkan driver.

Host preparation installed QEMU 8.2.2, Mesa Vulkan drivers, Vulkan tools and
validation layers, plus genisoimage. A 2 GiB swap file was added to `/etc/fstab`.
The repository's existing real-Vulkan buffer test and verified-rendering suite
both passed on the host using the lavapipe ICD. These are host library checks;
they do not establish guest Piglit or glmark2 results.

The supported lab QEMU was located on the existing Proxmox host and copied
without modifying the lab machine. It is `pve-qemu-kvm_10.1.2-4`, with build ID
`2e870e400e1692f5f5a54da897d1dc152b187278` and binary SHA-256
`383e4a30f5ae8f67f8a24fd2ea668f6592a72bd96507dadbc555b5dba5bc2103`.
Its library dependencies, firmware and lavapipe ICD were packaged into an
isolated runtime under `/opt/vlkn-ci/qemu-root`. The binary remains unchanged.
This avoids replacing Ubuntu's libc or other host system libraries with the
Proxmox runtime's versions. The preload's build checks accepted this binary.

Working files are in `/home/ubuntu/vlkn-ci`; host preflight logs are in its
`preflight/` directory. The administrator launch helper is
`/opt/vlkn-ci/launch-guest.sh`. It accepts validation mode `0` or `1`, forces
the runtime's lavapipe ICD, and sets `LP_NUM_THREADS=1`. It drops privileges to
`graphics-ci` before loading the supplied library. A chroot is a dependency
environment, not a guarantee that malicious code cannot escape.

A Debian 12 generic cloud image was downloaded from Debian's cloud-image
service and checked against its published SHA-512 checksum. The bootstrap
guest uses one vCPU, 512 MiB RAM, an 8 GiB disk, a 1 GiB guest swap file, and
the VMware SVGA device. Cloud-init installs Xorg, Mesa, Piglit and glmark2.
SSH is forwarded only on `127.0.0.1:2222` on the Oracle host. The host's
`vlkn-guest` SSH alias uses a newly generated key that grants access only to
this guest. No personal SSH key was copied into the guest.

The lab's opt-in Mesa module was copied to `/opt/mesa-vlkn/dri/vmwgfx_dri.so`
inside the guest. Its SHA-256 is
`2c4bb2130da43a1e6f81904ef361538349025e15a287200170cccb0dfb0de43f`.
Use `LIBGL_DRIVERS_PATH=/opt/mesa-vlkn/dri` and
`SVGA_VLKN_EXTENDED_STATE=1` for SVGA runs; clear both for the software reference.

The configured guest reports SVGA3D with OpenGL 2.1. A narrowed Piglit selection
(`depthfunc|depthrange-clear`) completed with two passes on SVGA and two passes
on the guest llvmpipe reference, with no skips or failures. The full default
glmark2 selection completed at 800×600 and one second per scene with Vulkan
validation disabled, scoring **21** across 32 rendered scenes. Terrain was
unsupported because vertex texture fetch is unavailable. This single score is
a setup check, not a performance comparison or a frozen required baseline.
The guest QEMU uses the packaged runtime's Mesa 25.0.7 and LLVM 19.1.7; the
native Ubuntu host checks above used Ubuntu's Mesa 25.2.8 and LLVM 20.1.2.

A follow-up run forced guest `LIBGL_ALWAYS_SOFTWARE=1` and
`GALLIUM_DRIVER=llvmpipe`, clearing the custom Mesa path and protocol option.
Direct guest llvmpipe (LLVM 15.0.6) scored **14** at 800×600 and one second per
scene, across 33 rendered scenes including terrain. glmark2 warned that GLX swap
control was unavailable. This and the earlier score of 21 are single runs with
different scene coverage, not a matched performance comparison. The raw direct
run is saved as `preflight/glmark2-llvmpipe.log` with renderer identity, command,
exit status and JSON summary alongside it.

The configured guest was shut down before preserving its base disk at
`/opt/vlkn-ci/qemu-root/images/debian12-graphics-base.qcow2`. The benchmark ran
from a fresh disposable overlay. The guest is left stopped after verification.
For future overlays, use this image as the backing file; inside the QEMU chroot
its path is `/images/debian12-graphics-base.qcow2`. The cloud-init Xorg service
uses `StandardInput=tty-force` so it can take control of VT1. Host evidence,
including loaded-process hashes/settings, is retained under
`/home/ubuntu/vlkn-ci/preflight/` and `preflight-evidence.tar.gz`.

The graphics PR workflow is still a local draft. No GitHub runner or required
merge check has been activated. The administrator
[prepare/cleanup hook](../scripts/graphics/README.md) now uses an independent
runtime at `/opt/vlkn-ci/qemu-gate-root` and SSH port 2223; the development guest
on port 2222 is unchanged. The first full 146-case control recorded 130 passes,
11 failures and five skips on SVGA, and 146 passes on guest llvmpipe. The gate
failed and removed its disposable guest. See the
[gate validation report](pr-graphics-gate-validation-20261008.md) for setup
corrections, source/library identities and later results.

The original module above is stale for the retained renderer's depth/shadow
changes. The hook can install the corrected module
`88a1b2409b87b946843133ff28b1c0cf382dd21436b69e9a3507ce8ecaa7381a`
into each fresh overlay without modifying the shared base image.

Before activation, obtain a clean full base-versus-base comparison using
[the gate setup](pr-graphics-gate.md). Test each build in a fresh overlay. Keep cloud
credentials off the test account, prevent its access to instance metadata,
and recreate the host after untrusted jobs. The Oracle account's provisioning
controls are separate from SSH access to this instance.

## PR scheduling policy

Follow the [PR graphics gate policy](pr-graphics-gate.md#queue-and-merge-requirements)
for admission, revision validity, cancellation, retry limits and host ownership.
Oracle must run one graphics job at a time, with the development guest stopped
and restored around the job. The hook does not implement a PR queue, automatic
development-guest restoration or an external watchdog. Those remain activation
requirements, alongside isolated disposable-host provisioning.
