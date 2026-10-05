# Presentation efficiency, 5 October 2026

The renderer reads back only the bounding source region needed by visible PRESENT
rectangles, streams their metadata without allocating, and merges consecutive
rectangles only when their source mappings agree and their union is rectangular.
BLIT clips share one mapping and use at most 64 reusable scratch entries to merge
compatible regions; larger lists retain streaming behavior. Merging never fills
holes in the requested destination. Different source mappings retain write order.
Tightly packed framebuffer rows use one memcpy; pitched rows keep their fallback.
Fully clipped updates leave earlier work queued until a real reader or fence.

Color readbacks retain their transfer-source layout until the next consumer.
Repeated transfer-source reads do not add another image transition. Other layouts
retain their write-to-read dependency, and the transfer-to-host buffer barrier and
completion wait remain. Depth/stencil restoration is unchanged. Identical shader
redefinitions retain their existing translated modules, variants, and dependent
pipelines. Failed depth variants still trigger the original retry/cleanup path.
This is reuse for an existing shader ID, not a persistent shader disk cache.

| Candidate | Audit and implementation |
| --- | --- |
| Empty updates | Skip fully clipped PRESENT/BLIT readback and submission; test queued upload preservation |
| Unchanged frames | Must still copy: the external display owner can reset the framebuffer; retain regression coverage |
| Overlapping copies | Merge compatible clips and consecutive PRESENT rectangles, preserving source mappings and order |
| Temporary buffers | Existing persistent GPU staging; add bounded reusable clip metadata and allocation-free PRESENT iteration |
| Pipeline/descriptor caches | Already present, including immutable descriptors and bounded retirement; retain them |
| Partial buffer uploads | Already update only the supplied range; retain copy-on-write for buffers referenced by queued draws |
| Command buffers | Already reset and reuse the backend command buffer after completion |
| Transfer batching | Existing uploads remain queued in the shared staging arena; one bounded readback serves all PRESENT rectangles |
| Layout transitions | Defer color readback restoration until the next consumer; preserve required memory dependencies |
| Contiguous copies | One memcpy when source and destination pitches equal the copied row width |
| Shader translation | Reuse identical existing shader definitions without translation, module creation, or pipeline eviction |
| Measurement | Optional transfer/copy/submission/wait counters and pixel-verified presentation/shader microbenchmarks |

## Measurements

Five alternating baseline/candidate pairs on the host Intel HD 630, with all VMs
and services running, use 200 operations per scenario after ten warmup operations.
The baseline retains renderer behavior from `92f6e53` with the same measurement
instrumentation backported. Both use real Vulkan, coherent cached transfer memory,
and validation disabled. Each presentation scenario verifies the entire framebuffer.
These are isolated operations through the CPU framebuffer adapter, not native
OpenGL or guest glmark2 scores.

| Operation | Baseline median | Candidate median | Lower time |
| --- | ---: | ---: | ---: |
| Full frame | 904.54 µs | 810.36 µs | 10.4% |
| 64×64 update | 629.78 µs | 90.31 µs | 85.7% |
| Overlapping clips | 127.77 µs | 122.23 µs | 4.3% |
| Fully offscreen update | 603.10 µs | 0.34 µs | 99.9% |
| Identical shader definition | 17.60 µs | 0.063 µs | 99.6% |

The small update transfers 16,384 bytes rather than 1,920,000. A full frame uses
one CPU copy instead of 600. The overlapping clip example copies its union once,
reducing copied pixels by 25%. An offscreen update submits no work. Background
load varies, and these timings do not establish a corresponding whole-suite FPS
gain. Raw evidence is in `artifacts/presentation-efficiency-20261005/`.

## Verification

All 14 local harness suites pass, including real software Vulkan presentation,
shader execution, ordering, readback, and strict validation through teardown.
The local SPIR-V validator is unavailable; its optional checks report skips.
CI installs and requires `spirv-val`. New controls cover merged/split/reordered
notifications, missing coverage, partial PRESENT of an image larger than staging,
queued upload visibility, different source mappings, and overlapping commands.
Guest Piglit, glmark2, and deployment verification are pending.
