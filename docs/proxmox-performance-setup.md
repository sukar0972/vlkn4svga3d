# Reproduce the VM119 performance configuration

The 648 one-second median and 659 standard-duration median use the renderer
changes, corrected guest Mesa module, private Mesa 26.2.4 Intel Vulkan driver,
and Xorg DirectPresents together. Source changes alone do not reproduce that
configuration. See [results and validation](renderer-performance-20261013.md).

These commands describe the measured Debian 12 guest on the Proxmox i5-7500T /
HD 630 host. The binary-patch lab adapter supports the explicitly allowlisted
stock QEMU build. Its executable SHA-256 is:

```text
383e4a30f5ae8f67f8a24fd2ea668f6592a72bd96507dadbc555b5dba5bc2103
```

## Build the adapter

On the Linux x86-64 build machine, install the project toolchain and Vulkan
headers described in [testing](testing.md). Build the measured release variant:

```sh
make -j4 preload-lab all \
  BUILD_DIR=build/perf-lto BIN_DIR=bin/perf-lto LIB_DIR=lib/perf-lto \
  CXXFLAGS='-O3 -flto -DNDEBUG -std=c++17 -Wall -Wextra -pthread -fPIC -MMD -MP'
sha256sum lib/perf-lto/libqemu_svga3d.so
```

The recorded adapter identity is `597b6634…`. Compiler/package changes can
change binary hashes; record the actual identity for each rebuilt run.

## Build the private Intel driver

Docker supplies Debian trixie, GCC 14 and LLVM 19 build dependencies. Mesa's
archive checksum pins the source. LTO is disabled because this upstream build
rejects it. Skylake CPU targeting matches the measured Kaby Lake host.
The default two build jobs fit the original four-GB build VM with swap.

```sh
mkdir -p artifacts/mesa-intel
curl -fL https://archive.mesa3d.org/mesa-26.2.4.tar.xz \
  -o artifacts/mesa-intel/mesa-26.2.4.tar.xz
printf '%s\n' 'bce5f7fbebb934373b86c999a064d52fb5065878dc57f287f95346648ec832e9  artifacts/mesa-intel/mesa-26.2.4.tar.xz' | sha256sum -c -
tar -C artifacts/mesa-intel -xf artifacts/mesa-intel/mesa-26.2.4.tar.xz
docker build -f tools/proxmox-lab/Dockerfile.intel \
  -t vlkn-intel-build:trixie tools/proxmox-lab
docker run --rm --cpus=2 \
  -v "$PWD/artifacts/mesa-intel/mesa-26.2.4:/mesa" \
  vlkn-intel-build:trixie
sha256sum artifacts/mesa-intel/mesa-26.2.4/build-vm119/src/intel/vulkan/libvulkan_intel.so
```

Use a fresh source/build directory for a new configuration. The builder runs
Meson's full dependency checks and builds only the Intel Vulkan driver.
Record container and package identities alongside the rebuilt library hash.
The measured driver hash is `61994010…`; system Vulkan packages stay installed.

## Install for VM119

Stop VM119 gracefully before replacing its adapter. On the Proxmox host,
verify the stock QEMU identity above and back up the existing VM119-specific
adapter and `/usr/bin/kvm` wrapper. Copy the newly built adapter to
`/usr/local/lib/libqemu_svga3d-vm119.so` while the VM is stopped.

Install the private driver and the checked-in ICD descriptor:

```sh
install -d /usr/local/lib/vlkn-vm119/mesa-26.2.4
install -m 755 libvulkan_intel.so \
  /usr/local/lib/vlkn-vm119/mesa-26.2.4/libvulkan_intel.so
install -m 644 intel-vm119.json \
  /usr/local/lib/vlkn-vm119/mesa-26.2.4/intel-vm119.json
ldd /usr/local/lib/vlkn-vm119/mesa-26.2.4/libvulkan_intel.so
```

Here `libvulkan_intel.so` is the built library copied to the host, and
`intel-vm119.json` is from `tools/proxmox-lab/`. Every dependency must resolve.
Compile `tools/proxmox-lab/vulkan_device_probe.c` on the build machine with
Vulkan headers, copy the executable to the host and select the private ICD:

```sh
cc -O2 -Wall -Wextra vulkan_device_probe.c -ldl -o vulkan_device_probe
VK_ICD_FILENAMES=/usr/local/lib/vlkn-vm119/mesa-26.2.4/intel-vm119.json \
  ./vulkan_device_probe
```

The probe creates an instance and enumerates devices without creating a GPU
queue or submitting commands. The measured host enumerates one Intel HD 630.
An error or zero devices blocks use of that ICD.

In the existing wrapper's stock-QEMU **VM119 branch**, retain its QEMU identity
guard and set these defaults:

```sh
export LD_PRELOAD="/usr/local/lib/libqemu_svga3d-vm119.so${LD_PRELOAD:+:$LD_PRELOAD}"
export SVGA3_VLKN_BATCH_IMAGE_TRANSFERS="${SVGA3_VLKN_BATCH_IMAGE_TRANSFERS:-1}"
if [ -z "${VK_ICD_FILENAMES+x}" ] && [ -z "${VK_DRIVER_FILES+x}" ]; then
    export VK_ICD_FILENAMES=/usr/local/lib/vlkn-vm119/mesa-26.2.4/intel-vm119.json
fi
```

Keep the other VM branches and allocations intact. The original global adapter
remains available for their existing configuration. The ordinary release build
uses validation off; for correctness runs start VM119 with:

```sh
SVGA3_VLKN_VALIDATE=1 \
VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT \
qm start 119
```

Install the host's Khronos validation layer before these correctness runs.
Verify the live process environment and loaded library paths, rather than
assuming the launcher settings reached QEMU.

## Configure the guest

Build and install the corrected Mesa 22.3.6 module using the
[guest tools instructions](../tools/mesa-guest/README.md). The measured module
hash is `88a1b240…`. Both the default desktop driver directory and the test
commands must use the same corrected module. Enable extended state through
`~/.xsessionrc` as documented there, keeping the system DRI directory available
for the software reference.

Back up the guest's `/etc/X11/xorg.conf.d/20-vmware-cursor.conf`, then use:

```text
Section "Device"
    Identifier "VMware SVGA Cursor"
    Driver "vmware"
    Option "HWCursor" "false"
    Option "DirectPresents" "true"
EndSection
```

Restart VM119 and verify Xorg reports `Direct presents are enabled`. In the
guest session, select the measured desktop conditions:

```sh
DISPLAY=:0 xrandr --output Virtual1 --mode 1280x800
DISPLAY=:0 xfconf-query -c xfwm4 -p /general/use_compositing -s false
```

Verify these conditions after each restart. Check the
[accuracy and mixed X11/GL probes](../tools/mesa-guest/README.md) before timing.
The mixed probe must report zero failures, including its color pattern.

## Match runtime conditions and test

The measured VM has two vCPUs and CPU weight 1000. After each restart, apply
only its measured thread affinity on the Proxmox host:

```sh
python3 pin-vcpus.py 119 2 3
```

The script is in `tools/proxmox-lab/`. It checks QMP thread ownership, assigns
one host CPU per guest vCPU, and leaves other VM threads untouched. VM119's
other threads can use the caller's allowed CPUs (0–3 on the measured host).
Thread affinity is a benchmark control reapplied after restart; the driver
and Xorg settings persist. Record background workload, CPU governor and GPU
clock settings. The measured host uses performance CPU governors and an HD 630
minimum/maximum GPU frequency of 1100 MHz. Keep these controls identical for
base and candidate and leave other VMs' CPU allocations unchanged.

With validation enabled, run the paired selected Piglit suite and focused FBO
selection. The fixture builder supplies the documented Debian 12 corrections:

```sh
python3 scripts/run_piglit.py --ssh USER@GUEST \
  --mesa-driver-path /home/USER/vlkn-mesa/dri --extended-state \
  --piglit /path/to/private-fixture-launcher/piglit \
  --test-timeout 600 --run-timeout 14400 --output artifacts/piglit-selected
python3 scripts/run_piglit.py --ssh USER@GUEST \
  --mesa-driver-path /home/USER/vlkn-mesa/dri --extended-state \
  --piglit /path/to/private-fixture-launcher/piglit \
  --include 'fbo-(blending|colormask|alphatest)' \
  --test-timeout 600 --run-timeout 3600 --output artifacts/piglit-fbo
```

See [fixture setup](../scripts/graphics/README.md) for the launcher build.
Inspect complete case/subtest counts, unchanged skips, Vulkan messages and
host/guest GPU kernel logs. Accuracy, glmark2 validation and the mixed probe
also need to pass. The selected suite is 146 cases, not the upstream full quick
profile. Software-reference commands unset the custom Mesa and extended-state
variables; the Piglit runner does this automatically.

For timing, stop VM119 and restart with `SVGA3_VLKN_VALIDATE=0` and
`VK_LAYER_ENABLES` unset. Reapply its affinity. Run in the guest:

```sh
export DISPLAY=:0 LIBGL_DRIVERS_PATH="$HOME/vlkn-mesa/dri"
export SVGA_VLKN_EXTENDED_STATE=1 vblank_mode=0
glmark2 --size 800x600 -b :duration=0.5
# Record three complete runs for the historical matched protocol:
glmark2 --size 800x600 -b :duration=1
# Separately record three complete standard-duration runs:
glmark2 --size 800x600
```

Retain every sample and use each protocol's median. Require 32 rendered scenes
per run and no renderer/submission/completion errors in the exact log interval.
Record adapter, host driver, guest module and QEMU hashes with the results.
Restore the saved adapter, wrapper, guest module and Xorg configuration to
return to the prior setup; normal `qm start` then uses those restored defaults.
