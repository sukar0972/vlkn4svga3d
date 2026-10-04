# Renderer CPU overhead and readback visibility, 4 October 2026

The renderer now caches the startup FIFO tracing option outside command and draw hot paths. Presence-based trace enablement is preserved, including an empty-but-present environment value; changing that option requires restarting the process. It also prefers CPU-cached, coherent Vulkan transfer memory when the device supports it. Required memory properties remain mandatory, and incompatible or unavailable cached memory falls back to the previous compatible selection. Explicit transfer-write to host-read barriers precede CPU readbacks; GPU completion remains synchronous. No pixel conversion, shader, test expectation, or skip was changed.

The Intel HD 630 exposes coherent host-visible memory type 1 with flags `0x7` and cached coherent type 2 with flags `0xf`. The deployed renderer selects type 2. Vulkan requires a memory dependency for device writes to become visible to host reads; queue completion and coherent allocation do not replace that dependency. See the [Vulkan synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html), [fence waiting rules](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-fences-waiting), and [memory property definitions](https://docs.vulkan.org/spec/latest/chapters/memory.html).

## Accuracy

The exact 146-case hardware replay retains **140 passes, one known API error-expectation failure, and five unsupported skips**. There are zero timeouts, zero recoveries, zero regressions, and zero failed pixel probes. The raw depth/stencil API expectation failure remains visible; its independent corrected control passes. See the [accuracy report](piglit-accuracy-20261004.md) for the error-expectation distinction.

The expanded paired framebuffer run has 13 hardware passes, 16 unsupported skips, and 154 passing hardware subtests. The unchanged software reference passes all 29 top-level cases. Neither renderer reports a problem. Vulkan validation reports zero errors and zero warnings during the full hardware replay and expanded framebuffer run.

Release glmark2 validation retains **27 Success, six Unknown, zero Failure**. Unknown cases and unsupported terrain remain visible and are not correctness passes. Independent depth/stencil orientation, overlapping-copy, and rejected-storage controls pass. All 14 host harness suites pass (11 configured for lavapipe, three ICD-free), together with mock/oracle and preload checks.

## Performance

Benchmarks use the complete glmark2 scene selection at 800×600, vblank disabled, a half-second-per-scene warm-up, then three one-second-per-scene runs. VM119 has two vCPUs pinned to physical cores 2 and 3, CPU weight 1000, and an Intel GPU minimum/maximum of 1100 MHz. VM105 and Jellyfin keep running. No unrelated workload was suspended during these measurements. Vulkan validation and ordinary tracing are disabled for release measurements.

Initial baseline scores were 410, 412, and 408 (median 410). The candidate scored 445, 439, and 444 (median 444, +8.3%). The closer return-to-baseline run scored 447, 446, and 442 (median 446), followed by candidate scores 440, 446, and 444 (median 444, −0.45%). The overlapping results do not demonstrate a whole-suite gain; the earlier +8.3% comparison is not a causal speedup. An alternating GPU-written scanout-copy microbenchmark also shows overlapping copy times for memory types 1 and 2 on this Intel GPU.

CPU profiling identifies scanout copies as a substantial cost. Active GPU render-engine utilization has median 39.5% before and 41.6% after in the sampled runs. These utilization figures do not measure shader correctness or prove causality; profiles also perturb throughput. Relative CPU copy sample percentages increased after the change, so they do not establish reduced copy cost. Use the release comparisons for throughput.

The final tracing candidate scores **450, 449, and 453 (median 450)** with VM105 and Jellyfin active. That is +0.9% against the closer baseline median of 446; the short runs and changing background load do not establish a reliable whole-suite improvement. All final correctness results above are from this exact candidate, after the tracing change.

The subsequent tracing optimization reduces median cost for valid render-state FIFO commands from 80.089 to 45.024 ns/command (43.8%) across ten alternating before/after pairs of two million commands on the lab host. Every command returns success with exact payload consumption, and the final render state is verified. This isolated dispatch benchmark does not measure FPS. An enabled startup trace control retains the expected command logs.

The original 500 target remains unmet. Results are specific to this profile and background workload; the small repeated-run samples do not establish a universal speedup.

## Provenance and retained evidence

- Tested renderer source: `6847241145e957067de36b9adafe07d13f296080`.
- Candidate adapter SHA-256: `85406435b750f0b8fd44a9545a1b842af42f94c4209ed6935b2d31597590f21b`.
- Baseline adapter SHA-256: `dbba46d242f38cee8640e623d9e66658d7c7734194dc1c22ae254db4c953c857`.
- Unchanged guest Mesa driver SHA-256: `2c4bb2130da43a1e6f81904ef361538349025e15a287200170cccb0dfb0de43f`.

Local evidence is under `artifacts/performance-20261004/`: `full-replay-6847241/`, `final-framebuffer-comparison-6847241/`, `final-verification-6847241/`, `baseline/`, `return-comparison/`, `fifo-throughput-summary.json`, the alternating FIFO logs, the CPU profiles, and GPU JSON captures. Earlier `0c05da8` directories retain the pre-tracing candidate comparison. The preliminary cache-only replay was explicitly stopped to add host visibility barriers; it is not the final accuracy result.

The candidate is installed in VM119's host adapter, with the guest custom Mesa driver selected by the existing login environment. GPU clock and vCPU affinity are runtime settings: affinity must be reapplied after QEMU restarts, and the clock minimum resets after a host reboot. OpenGL support remains 2.1 / GLSL 1.20 / GLES 2.0.

To reproduce the isolated FIFO benchmark after `make`, build it against the library from each revision, then alternate the two executables on the same host. Leave `SVGA3_VLKN_TRACE_FIFO` unset for timed runs. The benchmark uses the mock backend to isolate CPU dispatch.

```sh
g++ -O2 -std=c++17 -pthread -Iinclude -Iinclude/internal -Ishim/include -Idata -Itools/include \
    tools/benchmark_fifo_dispatch.cpp -Llib -lsvga3_vlkn -ldl -o /tmp/benchmark-fifo-dispatch
env -u SVGA3_VLKN_TRACE_FIFO /tmp/benchmark-fifo-dispatch 2000000
```
