# Native QEMU adapter

This source integration targets **QEMU 9.2.0** and adds an explicit
`-device vmware-svga,vlkn=on` option. It uses QEMU memory regions, PCI address
spaces and display notifications. It does not patch instructions or inspect
`/proc` mappings. The normal device remains available when the option is off;
initialization fails with a diagnostic if Vulkan was requested but unavailable.

Build and test from the repository root:

```sh
docker build -t vlkn-native-qemu -f integrations/qemu/Dockerfile integrations/qemu
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
  vlkn-native-qemu sh integrations/qemu/build.sh /work/artifacts/qemu-native
```

The script verifies the official release archive's SHA-256, applies the source
patch, installs the library to a private prefix, builds QEMU and runs the tests.
For an existing clean 9.2.0 source tree, use `sh integrations/qemu/apply.sh TREE`,
install this library, and expose its pkg-config directory when configuring QEMU.
The integration builds only when `svga3-vlkn` is found. A request for `vlkn=on`
without that dependency fails instead of silently selecting another renderer.

The adapter accepts legacy 3D commands, GMR2 mappings and completion fences.
QEMU retains its existing legacy 2D and cursor handlers. Guest-memory transfers
preflight RAM ranges and reject MMIO before copying; writes mark RAM dirty.
Framebuffer presentation uses the PCI VRAM mapping and QEMU display callbacks.
An end-of-batch submit allows work to overlap; fence completion still waits for
all earlier work. No DX rendering or screen-object capability is advertised.

`test_native.py` drives the real PCI device through qtest and QMP. It checks
Vulkan framebuffer pixels, guest-RAM DMA in both directions, wrapped and
incomplete fences, and reset. The accompanying Multiboot guest executes PCI,
port and FIFO accesses on an emulated CPU, checks its framebuffer pixels, then
repeats after a full machine reset. It is a bare-metal protocol test, not a
Linux desktop boot. Both paths use lavapipe with Vulkan synchronization
validation; logs and results are retained in the output directory.

Migration and snapshots are explicitly blocked because renderer state is not
serialized. Device loss requires restarting QEMU to recreate the Vulkan device.
The patch is maintained here; it has not been accepted by upstream QEMU. Other
QEMU versions require porting and the same tests. The existing lab VM's Piglit
and glmark2 runs exercise its designated preload adapter, separately from this
native adapter's tests.

API references: [QEMU memory API](https://www.qemu.org/docs/master/devel/memory.html)
and [device load/store API](https://www.qemu.org/docs/master/devel/loads-stores.html).
