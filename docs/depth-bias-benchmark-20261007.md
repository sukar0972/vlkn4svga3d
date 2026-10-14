# Depth-bias performance comparison, 7 October 2026

The matched VM119 glmark2 comparison produced a median score of **298.5 before**
and **301 after** depth-bias support, a **0.84% increase**. The ranges overlap;
these short samples do not establish a reliable speedup or a small regression.
These are validation-enabled measurements, not release throughput results.

| Build | Scores, in execution order | Median | Range |
| --- | --- | ---: | --- |
| Before depth-bias support | 303, 296, 301, 294 | 298.5 | 294–303 |
| After depth-bias support | 301, 301, 297, 302 | 301 | 297–302 |

Both builds recorded **27 glmark2 validation successes, six unknowns, and zero
failures**. Unknown results are not correctness passes. Terrain remains
unsupported because the guest exposes no vertex texture fetch. All eight timed
runs used the same 32 rendered scenes from the full default selection and
identified the SVGA3D renderer.

## Comparison and compatibility control

Both builds start from source `0a09f02c11e617712f55be6b045e9d0c07c35981`.
The before build removes only the depth-bias implementation, including its state
validation, pipeline enable bit, format conversion, and Vulkan dynamic command.
Gamma, shader, and device-loss changes remain identical in both builds.

The unmodified adapter advertises hardware version `0x20000`. With that value,
this guest's Xorg disables SVGA direct rendering; the custom Mesa path cannot
create a GLX context. No timing results from that configuration are included.
Both isolated benchmark adapters instead advertise `0x20001`, matching the
previously installed lab adapter. This common compatibility adjustment permits
an isolated depth-bias comparison, but the results do not validate deployment
of the unmodified current adapter. Repository runtime source was not changed.

## Conditions and evidence

The run order was before/after/after/before, with two timed runs per block.
Each block restarted VM119 and warmed up the full scene selection for 0.5
seconds per scene. Timed runs used glmark2 2023.01, one second per scene,
an 800×600 window, and vblank disabled. The desktop was 1920×1080.
FIFO tracing was disabled. **Correction:** the initial report incorrectly said
Vulkan validation was disabled. The adapters were compiled without `NDEBUG`,
and clearing `SVGA3_VLKN_VALIDATE` selected the debug default of enabled
validation. A later CPU profile of the same adapter showed the validation layer
executing. Release comparisons must explicitly use `SVGA3_VLKN_VALIDATE=0` or
build with `NDEBUG`, and check the loaded process before timing.

The host used Intel HD Graphics 630, GPU minimum and maximum of 1100 MHz,
VM119 CPU weight 1000, and vCPUs pinned to CPUs 2 and 3. VM105 remained running
before and after every timed run; no background workload was suspended. The
guest used Mesa 22.3.6 with its existing custom driver and extended-state option.
Background contention and these short samples limit sensitivity to small changes.

Evidence is retained in `artifacts/depth-bias-benchmark-20261007/`: exact source
revision, baseline and common compatibility patches, adapter hashes, build logs,
per-run logs, renderer identities, host conditions, validation logs,
`run-verification.json`, and `summary.json`. Artifacts are excluded from Git.
The original adapter and vCPU affinities were restored after the comparison.
Its SHA-256 matched the saved original, the guest again identified SVGA3D,
a graphics-health probe passed, and the desktop remained at 1920×1080.
