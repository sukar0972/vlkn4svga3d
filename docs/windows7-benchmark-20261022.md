# Windows 7: stock QEMU versus vlkn4svga3d — 2026-10-22

Median desktop responsiveness was broadly similar in this short A/B run. The
renderer enabled a working Direct3D 9 HAL; stock QEMU could not create one.

| Metric | Without vlkn4svga3d | With vlkn4svga3d |
| --- | ---: | ---: |
| Click → visible repaint, median | 62.9 ms | 56.9 ms |
| Click → visible repaint, p95 | 151.7 ms | 73.9 ms |
| Drag release → repaint, median | 32.7 ms | 35.1 ms |
| Drag release → repaint, p95 | 55.7 ms | 46.7 ms |
| Desktop pixel checks passed | 42 / 42 | 42 / 42 |
| QEMU CPU during desktop run¹ | 14.1% | 14.6% |
| D3D9 HAL, one-triangle throughput | Unavailable | 1,027.9 frames/s |

Lower latency is better. p95 uses the nearest-rank 95th percentile.
¹ One host CPU core = 100%; this is average QEMU process CPU, including all its
threads, across each desktop workload. The desktop runs lasted
9.62 and 8.82 seconds respectively.

## Direct3D throughput

The custom test clears, draws one RGB triangle, and presents a 640×480 window
with immediate presentation and no application timer cap. Each of three trials
uses two seconds of warmup and five seconds of measurement. This measures
synthetic render/present throughput, not game performance or monitor refresh.

| Trial | Completed frames | Measured seconds | Frames/s |
| --- | ---: | ---: | ---: |
| 1 | 4723 | 5.000704 | 944.467 |
| 2 | 5147 | 5.000865 | 1029.222 |
| 3 | 5140 | 5.000659 | 1027.864 |

Median: **1,027.9 frames/s**, observed trial range **944.5–1,029.2**.
Stock QEMU returned `0x8876086A` when creating a HAL device. No valid stock FPS
or 3D speedup ratio exists for this test; unavailable is not zero FPS.

## Matched settings and method

- Same VM 120, Windows 7 Professional SP1 x64, two vCPUs, 2048 MiB RAM,
  VMware SVGA device with 128 MiB VRAM, installed driver 7.14.1.1057,
  and 1024×768×32 desktop. Both mode changes succeeded.
- Without: exact stock QEMU binary, no renderer library in its process maps,
  no LD_PRELOAD or guest profile. Windows reported driver Code 43 and used
  its basic-display fallback. With: windows7-wddm profile, validation off,
  Intel HD Graphics 630 Vulkan backend; Windows reported Code 0.
- Desktop benchmark: identical fixed-color background and 400×240 window;
  three warmup clicks followed by 30 measured clicks, then two warmup drags
  followed by 12 measured drags. Click timing includes a fixed 40 ms hold.
  Drag timing starts at release, after the same 160 ms movement sequence.
- A local raw-pixel VNC client measured received frame updates and checked
  the expected colors, including restoration of the background after dragging.
  This excludes the user's browser, network, and physical display latency.
- Stock ran first, vlkn4svga3d second, after separate clean boots. Host load
  averages were 2.83 and 3.24 at the ends of the runs. Other host workloads were
  left running. This is a short sequential comparison, not an isolated hardware
  benchmark or a claim of statistically significant small differences.
- The complete integration is compared, including its capability and interrupt
  handling. The stock fallback and accelerated WDDM paths differ; this does not
  isolate the renderer's overhead inside a common working 3D driver.

The main result is Direct3D availability, not a substantial median desktop
speedup. Both conditions passed every sampled desktop pixel check. The lower
click p95 with the renderer was observed in these samples; more runs would be
needed to establish a general tail-latency improvement.

## Evidence and reproduction

Local sources, executable build method, raw samples and logs (excluded from
Git and not included in the repository) are in
`artifacts/windows7-vm120/benchmark-20261022/`:
`DesktopBench.cs`, `D3D9Bench.cs`, `desktop_benchmark.py`, `results.json`,
`stock-desktop.json`, `vlkn-desktop.json`, both guest logs and both D3D9 logs.
Compile the C# sources using the guest's Framework64 v3.5 `csc.exe`, with
`/target:winexe /r:System.Windows.Forms.dll /r:System.Drawing.dll`; launch them
in the interactive desktop with `LaunchDesktopCheck.exe`. The VNC benchmark
script runs on the Proxmox host with its phase argument (`stock` or `vlkn`).

Only the lab VM's preload branch was temporarily disabled. The original wrapper
was restored byte-for-byte (SHA-256
`bf131b363a33eb97963bc4aba3db5d574a7d7360a818af978c4e66b9d567f508`).
The temporary marker and host wrapper backup were removed. The VM is running
again with renderer SHA-256
`fe409a34abdd0c54152c381cc7f2fc8a895f05506df065b453ed1a04c1dcf48d`.
Its isolated network remains unchanged.

The benchmark harness sources (C# smoke program, Python VNC scripts) are
lab-only and are not in this repository; the numbers above cannot be
reproduced from Git alone.
