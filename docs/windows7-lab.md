# Windows 7 lab guest

A Windows 7 Professional SP1 x64 guest can create a
Direct3D 9 HAL device with hardware vertex processing and display a colored
triangle through the Intel HD Graphics 630 Vulkan backend. This is a limited
smoke test, not general Windows application or shader compatibility coverage.
On October 22, 2026, the unmodified smoke program completed all 600 frames
with successful clear, draw, and present calls.

The tested guest driver is VMware WDDM **7.14.1.1057**, from VMware Tools
8.6.0. Windows reports device error code 0. The newer **8.15.1.65** driver
blue-screened during boot in this lab and remains unsupported.

## Host configuration

Build with `make preload-lab-release` and select
`SVGA3_VLKN_GUEST_PROFILE=windows7-wddm` only for the designated guest. The
binary-patch adapter still requires its exact allowlisted QEMU build; the
tested host is Proxmox QEMU 10.1.2. Keep injection scoped to this VM. The
profile logs to `/tmp/svga3d-windows7.log`.

The profile declares PCI INTA and exposes IRQMASK. Fence, fence-goal, and FIFO
progress notifications use QEMU's PCI interrupt router. The IRQ status port
supports acknowledgement and masking, and completed goals are checked when
unmasked. Other guest profiles retain polling. The pinned QEMU chip offset and
IRQ entry point are checked against instruction bytes before use.

The Windows profile leaves QEMU's native mouse/tablet input handling intact.
The PlayBook input hooks add a 150 ms minimum click dwell on the QEMU main
thread and split relative movement; they are bypassed for this profile.
Repeated definitions of an unchanged screen preserve its display surface
and framebuffer import rather than forcing another resize and full repaint.
A changed VRAM mapping or console forces a new display surface.
Hardware cursor capabilities are disabled for Windows because this FIFO
adapter does not apply cursor-definition packets; Windows draws its cursor.

The Windows profile's format advertisement includes matching render-target flags
for XRGB8888 and RGB565. XRGB1555 stays offscreen: the Windows runtime
rejected simultaneous RGB565 and XRGB1555 display-mode advertisements.
Other profiles retain their previous format capabilities.

The default profile exposes shader model 2.0 using the legacy SVGA version
enumerants: **5** for vertex shaders and **11** for pixel shaders. Packed
versions such as `0x300` are not these enumerants. The core capability table
remains unchanged.

### Experimental accelerated OpenGL

Set `SVGA3_VLKN_WINDOWS_OPENGL=1` alongside `windows7-wddm` to expose the
legacy shader model 3.0 enumerants: **7** for vertex shaders and **13** for
pixel shaders. The installed VMware OpenGL ICD requires both thresholds
before it exposes accelerated pixel formats. The switch has no effect on
other guest profiles.

With this mode, Windows creates an accelerated WGL context reporting
`VMware, Inc.`, `Gallium 0.3 on SVGA3D; build: RELEASE;`, and
`2.1 Mesa 7.7.1-DEVEL`. An OpenGL triangle was displayed and its center
read back as RGBA `(0,255,0,255)` with no GL error through vlkn4svga3d on
the host Intel HD Graphics 630 Vulkan driver. No guest software OpenGL DLL
was installed.

Context attachment defaults must use `SVGA3D_INVALID_ID`: SID 0 is a valid
resource. This ICD creates a 32×32 helper texture at SID 0. Aliasing unused
render targets to that texture incorrectly restricted a 640×480 framebuffer
to 32×32. A real Vulkan readback regression covers this case.

The installed Minecraft 1.16.5 instance (Fabric, 34 mods) now reaches its rendered main menu in this mode instead
of GLFW error 65542. The launcher's bundled Qt 5.15.2 platform plugin also
needs its optional D3D9 GPU identity probe disabled: the Windows runtime
corrupts its heap while releasing the rejected D3D9 object. The pinned
two-byte patch and its original-DLL hash guard are recorded in the lab
artifacts (not in Git). Minecraft's OpenGL driver is unchanged.

A creative superflat world named `vlkn gpu test` loads and renders terrain
with Sodium 0.2.0+build.4's Oneshot backend. Disable **Use Compact Vertex
Format** in Sodium's Advanced settings: with that setting enabled, terrain
was missing. The setting is saved as `use_compact_vertex_format: false` in
this instance's `minecraft/config/sodium-options.json`. A debug screenshot
at 854×480 with the existing 12-chunk view distance showed **21 FPS**. This
is a single stationary observation, not a benchmark or comparison. Entity
and first-person hand colors remain incorrect; general game compatibility
is incomplete.

This OpenGL mode currently sacrifices Direct3D9 compatibility with the
installed WDDM UMD: it reports VS 3.0 but lowers PS to 2.0 because additional
SM3 requirements are unavailable (including six user clip planes and
texture gradients), and the Windows runtime rejects its HAL capabilities.
Use the default mode for D3D9; its unmodified smoke program passed another
600 HAL frames with this build and the OpenGL switch disabled. These
triangle, menu, and flat-world checks are limited coverage of the modpack. Sodium 0.2
includes an older OpenGL backend; compatibility must be tested rather than
inferred from the OpenGL version string alone.

## Verification

`make test`, `bin/test_preload_fifo`, `bin/test_preload_fence`, and
`bin/test_real_vulkan` pass.
The preload tests cover profile selection, unchanged core shader caps,
display format flags, FIFO framing, screen objects, and fence ordering.
They also check interrupt masks, acknowledgements, level transitions, fence
goal wraparound, malformed FIFO allocation bounds, completion before
notification, configuration reset, and scanout remapping.

After enabling completion interrupts, the same VNC input/repaint probe fell
from 4876 ms to 614 ms, including 600 ms of deliberate pointer movement and
button hold. A Start-menu check took 343 ms including an 80 ms hold. A real
VNC window drag restored the sampled old background and new client pixels
within 40 ms of release. These are individual functional checks, not a general
performance benchmark. The unmodified D3D9 test also passed another 600 frames
on the interrupt-enabled build.

The guest smoke program uses the normal system `d3d9.dll` and installed
VMware UMD. It calls `CheckDeviceType`, `GetDeviceCaps`, and `CreateDevice`
with `D3DDEVTYPE_HAL` and hardware vertex processing, then repeatedly clears,
draws an RGB triangle, and presents a 640×480 window. No diagnostic capability
hooks are present in this program. Driver initialization alone is not a pass;
check successful HRESULTs, the rendered image, and completed frame counts.

The paths below describe local lab evidence, which is excluded from Git and
is not included in this repository. The guest runs on an isolated virtual
network with NAT for internet access; host/LAN access is blocked in both
directions. A rollback snapshot is retained.

### Merge validation (2026-10-23)

The integration review scoped the WDDM format overrides to the Windows
profile, preserved the default and PlayBook format advertisements, checked
FIFO allocation bounds before IRQ goal reads, and invalidated screen reuse
when VRAM or the console changes. Regression tests cover those cases.

`make test`, the real Vulkan, QEMU, presentation and verified-rendering
targets, `make preload-lab`, and `make test-install` passed. One complete
host harness iteration passed all 16 suites on lavapipe with synchronization
validation enabled and external SPIR-V validation using the local
SPIRV-Tools installation. This validation did not repeat the Windows VM smoke
checks after the integration review; the guest results above are from
October 22. The OpenGL/D3D9 conflict and Minecraft rendering limitations
remain open.
