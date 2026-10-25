# Experimental Windows 7 support

The binary-patch QEMU lab adapter has an opt-in Windows 7 SP1 x64 profile:

```sh
make preload-lab-release
export SVGA3_VLKN_GUEST_PROFILE=windows7-wddm
```

Scope these settings and the adapter preload to the intended QEMU process.
The adapter requires its exact allowlisted QEMU build; it is not a general
Windows driver installer or a feature of the native source adapter.

## Guest driver

The tested driver is VMware WDDM **7.14.1.1057**, from VMware Tools 8.6.0.
Driver **8.15.1.65** blue-screened in the tested setup and is unsupported.
The older driver has rendered a Direct3D 9 HAL triangle with hardware vertex
processing. Broader application compatibility is unverified.

The profile enables PCI completion interrupts, retains QEMU's native input
handling, and uses a software cursor. Pending interrupt events remain latched
while masked; acknowledgement clears them. FIFO and register accesses share
the adapter lock. QEMU instruction-byte checks run before hooks are installed;
a mismatch exits with status 78 before patching.

## Experimental OpenGL

```sh
export SVGA3_VLKN_WINDOWS_OPENGL=1
```

This additionally exposes the shader versions required by the older VMware
OpenGL ICD. An accelerated OpenGL 2.1 triangle and pixel readback worked in
the lab. This mode breaks the installed driver's Direct3D 9 HAL validation;
leave it unset for Direct3D 9.

The Windows smoke tests were performed in a private lab. Their harnesses are
not distributed here, and they have not been rerun on the current PR head.
Host-side regression tests do not establish Windows application compatibility.
