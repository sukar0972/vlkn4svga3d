# Publication checks — 2026-09-25

A separate source snapshot was built without using the development directory's existing object files.

| Check | Result |
| --- | --- |
| `make -j4 all` | Passed; compiler warnings remain |
| `make test` | Passed, including 3,407 mock-engine assertions |
| `make acceptance` | Passed all stages of the existing runner |

The real-Vulkan driver check identified Mesa llvmpipe (LLVM 20.1.2, 256 bits), Vulkan API 1.4.318, software rendering, mock fallback disabled, and validation layers enabled. That check reported zero validation errors and warnings.

These results describe the current tests, not an independent certification of their coverage. The QEMU integration executable is an in-process device harness, not a booted guest. No real VM was booted or modified during publication. The project owner reported entering a Minecraft world on September 24. Recorded screenshots inspected afterward show terrain and a textured hand. A live September 25 check identified the Intel HD Graphics 630 Vulkan driver and nonzero GPU engine activity. Full visual correctness and cold-boot reliability remain unverified. The fresh test runs above use local software Vulkan, not the live guest. Hardware-GPU performance and broad guest compatibility remain unverified.

Build and test transcripts from this snapshot are in `validation/`.
