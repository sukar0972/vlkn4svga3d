# vlkn4svga3d

An experimental SVGA3D-to-Vulkan rendering library and QEMU integration prototype. Internal APIs and build targets still use the original `svga3_vlkn` name.

**Work in progress — incomplete and not production-ready.** This is a source snapshot, not a finished virtual GPU or a drop-in replacement for VMware graphics. Shader, format, state, and guest compatibility are not comprehensively verified. Minecraft has reached an in-game world in the PlayBook guest. Recorded September 24 screenshots show terrain, the hotbar, and a textured player hand after integration fixes. A September 25 live check identified the Intel HD Graphics 630 Vulkan driver and measured GPU activity. This is a narrow compatibility milestone, not comprehensive visual validation or a cold-boot reliability guarantee. No general compatibility or performance guarantee is made.

## Warning:

Currently runs as root on the host!

## What is included

- SVGA3D FIFO decoding, surfaces, contexts, and Vulkan rendering code.
- A D3D9-style shader bytecode to SPIR-V translator (partial coverage).
- Guest-memory/GMR handling and framebuffer presentation code.
- A device integration harness and experimental QEMU preload library.
- Mock unit tests, real-Vulkan tests, and a VirtualBox D3D9 reference harness.

## Source layout

| Directory | Contents |
| --- | --- |
| `include/` | Public library headers and bundled Vulkan headers |
| `include/internal/` | Private renderer headers |
| `src/` | Renderer and QEMU implementation files |
| `tests/` | Test programs |
| `shim/include/` | Compatibility headers, including `mock/` |
| `shim/src/` | Compatibility implementations |
| `tools/include/` | Reference harness helper headers |
| `tools/` | Reference harness implementation |

Internal headers are not a stable public API. `.editorconfig` defines whitespace conventions; Makefile recipes use tabs.

## Build on Linux

Requires a C++17 compiler, GNU Make, binutils, pthreads, and libdl. Vulkan headers are bundled. Real rendering additionally requires the Vulkan loader, a working Vulkan driver, and validation layers. Software Vulkan such as Mesa lavapipe is usable for testing.

Example Debian/Ubuntu dependencies:

```sh
sudo apt install build-essential libvulkan1 mesa-vulkan-drivers vulkan-tools vulkan-validationlayers
```

Build from the repository root:

```sh
git clone https://github.com/sukar0972/vlkn4svga3d.git
cd vlkn4svga3d
make -j4 all
```

Outputs include `lib/libsvga3_vlkn.a`, `lib/libqemu_svga3d.so`, and test executables under `bin/`. Public interfaces are in `include/svga3_vlkn.h` and `include/qemu_vmsvga.h`. Link consumers with the static library, `-ldl`, and `-pthread`.

Use `make clean` before rebuilding after header changes: the current Makefile does not generate complete header dependencies.

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

## QEMU integration limitations

`src/qemu_svga3d_preload.cpp` is an experimental, build-specific hook with guest-specific workarounds. Do not install it globally or preload it into an arbitrary QEMU build. It requires separate review of binary offsets, device layouts, capabilities, and guest-memory handling. This repository does not provide a supported Proxmox deployment procedure. Use disposable, isolated VMs for integration work.

## Snapshot and licensing

Updated from the development source on 2026-09-25. Build products, downloaded system packages, VM images, host configuration, and old runtime logs are excluded. See `VALIDATION.md` for publication-time checks.

Third-party source notices are retained. `DevVGA-SVGA3d-win.cpp` and related VirtualBox files carry GPL-2.0 notices; the license text is in `COPYING`. VMware protocol headers retain their permissive notices. Bundled Khronos headers carry their own license identifiers. No blanket relicensing of third-party code is implied; consult individual files. A separate license grant for original project code has not yet been selected.
