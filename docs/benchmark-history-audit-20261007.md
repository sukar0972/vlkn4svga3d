# Benchmark history audit, 7 October 2026

The historical **556 full-suite score is real and repeated**, not a remembered
single-scene result. Current measured release throughput is around **439–465**.
The 556-to-439 difference is **21.0%**, but it is not an isolated measurement of
code regression: background load, GPU clocks, scheduling, rendering accuracy,
and binaries changed between those dates. The history contains confirmed
regressions, subsequent recoveries, reporting errors, and a remaining performance
gap. It does not establish that every correctness fix preserved throughput.

The strongest confirmed performance regression was the October 2 experimental
Minecraft patch: a matched five-scene median fell **504 → 285 (−43.5%)**. Its
replacement recovered a full-suite median of **543**, and October 3 comparisons
remained around **554–556**. The subsequent loss to roughly 440 has not been
isolated to a commit. Recent targeted optimizations have not recovered 500.

There is also a concrete guest compatibility regression: October 5 commit
`298a207` changed the advertised hardware version from `0x20001` to `0x20000`.
This guest disables SVGA direct rendering with the latter. October 7 comparisons
used the old value in both isolated adapters; their scores do not establish that
the unmodified current adapter works in this guest.

## Scope and interpretation

Read-only review covered 19 related local Codex session histories, found by
workspace metadata and project-name references across 207 available rollout
files, plus 15 related T3 Codex thread projections. These overlap and are not 34
independent conversations. Two histories discuss the project from other
workspaces. The T3 UI sometimes keeps a thread while its provider session changes.
This audit covers locally available history; deleted or cloud-only history is
not established by this search. No benchmark was rerun and no VM setting changed.

Thread extracts, metadata, raw-score inventories and duplicate-content hashes
are retained in `artifacts/benchmark-thread-audit-20261007/` (ignored by Git).
The accompanying [run inventory](benchmark-history-runs-20261007.csv) lists saved
scored logs, including warmups and diagnostics, with their workload and role.
It is an inventory of evidence, not a count of independent experimental samples.
It contains 508 scored log files: 175 health/recovery checks, 47 warmups, and
286 timing/scene/diagnostic logs. Identical-content copies in nested worktrees
are excluded; distinct primary run files are retained even when their output
matches exactly.

Full-suite scores, subset scores, single-scene FPS and microsecond timings are
different metrics. A full selection renders 32 SVGA scenes; terrain is unsupported.
Native Intel and llvmpipe can render 33. Unless stated otherwise, tests use VM119,
the Intel HD 630 host, an 800×600 benchmark window and glmark2 2023.01.
Early runs often used a different desktop size or default vblank behavior.
Medians below exclude warmups. Early single observations and averages remain
labeled as such. A glmark2 validator success does not validate its six unknown
cases or broader rendering accuracy.

## September 21–29: initial results and recovered optimizations

These results come chiefly from Codex session
`01a0e3c3-ce18-7202-8eb7-402eae955c49`, the T3 **Debian VM Graphics Benchmark
Results** thread (`13096e80-1a96-4c91-81d2-6975166246af`). Some earliest logs
exist only in the session's tool outputs rather than standalone artifact files.

| Date / change or experiment | Recorded benchmark | Assessment |
| --- | --- | --- |
| Sep 21–25, early QNX/Minecraft discussion | Predicted 30–60 and 15–35 FPS; external JIT report of swap rate 0.57 → 24–27 | Predictions and an unverified external report, not measured Codex library speedups. Different workload from Debian glmark2. |
| Sep 27, user-supplied starting table | Full SVGA 66 vs llvmpipe 248; texture 77 → 95, Phong about 77 → 104; refract 17 with skipped draws, then 24 vs software 18 | Supplied historical observations. Exclude skipped-draw 17 from performance claims. No new Codex measurement at this stage. |
| Sep 27, submission/constant-ring changes | Host tests passed; no reachable-VM timing | No measured FPS benefit. |
| Sep 28, first fresh current build | Refract/nearest/Phong 19/77/80 twice; full score 68 | Mixed versus starting table, not a matched 66 → 68 speedup. |
| Sep 28, saved faster adapter vs current vs clean parent | Targeted FPS 23/102/103 vs 19/77/80 vs 18/76/78; corresponding three-scene scores 75, 57, 56 | Real loss relative to installed adapter; parent shows the new constant-ring patch did not explain the gap. |
| Remove redundant DMA-upload preflush | About 19/77/81 targeted FPS | No material measured gain. |
| Shader-interface experiment | Three-scene score 44; restored path about 56 | Worsened results; reverted. Pipeline creation errors limited confidence. |
| Restore full-surface readback cache, PR #3 (`157e629` merge) | Targeted 19/75/77 → 23/104/111 FPS; full 68 → 87 | Recovered a missing optimization. Early output still had validation failures, so not an equal-quality claim against the final renderer. |
| Dirty-window presentation, PR #4 | Targeted desktop/ideas/jellyfish/texture 36/62/85/103 → 36/73/87–90/106–111; full 87 → 95 | Historical gain; full run still had pipeline failures. Later history identified `glmark-skip-presents-full` as an unfair normal-presentation comparison. Do not use 95 as an unquestioned equivalent-work baseline. |
| Failed-pipeline cache and diagnostics | Full 95 → 93, later 92; repeats 94/93/94 | Measured small decreases, no controlled attribution. This was not a demonstrated throughput improvement. |
| Persistent buffer mapping experiment | Buffer average 75.3 original vs 74.7 candidate; candidate 73/75/76 | No gain; reverted. Busy-host full run not accepted as causal evidence. |
| Avoid extra cache copy / failure-cache shadow probe | Ideas/jellyfish/refract 72/77/27; old shadow 74 vs new 71–73 | No reliable broad gain; cache-copy experiment reverted. Shadow did not exercise the failing pipeline keys. |
| Controlled retry after host-load cleanup | Current full 96/96; baseline 148/98; repeated texture 166–171 within one boot | Large cross-boot variation on the same binary; does not isolate the small diagnostic patch. |
| Resolution probe | Two-scene score 280 at 400×300 vs 167 at 800×600 | Different pixel workloads, not a code speedup. |
| Depth-sampling repair / skip empty fence waits | Nearest FPS current 117/116/116 vs baseline 115/117/117 | Effectively flat; still 27 glmark2 pixel failures. |
| Suppress duplicate fence readback | Nearest FPS 118/115/117 → 126/125/125, +7.4% average | Measured selected-scene gain, but nearest still failed pixel validation. |
| Framebuffer reuse after rendering corrections | Five-scene scores 72/72/71 → 72/73/73 | Small aggregate change; 27 known-result checks now pass. Scene means: nearest 117.0 → 116.7, Phong 120.7 → 122.3, blur 37.0 → 38.3, desktop shadow 62.0 → 62.7, refract 28.3 → 30.0 FPS. |
| Same corrected-workload llvmpipe reference | Five-scene scores 142/184/179 | Software comparison, not a renderer revision. Texture varied substantially. |
| Historical PR snapshots on five scenes | Before #3 55/55; after #3 75/76; after #4 79/80; #5 78/79; #6 79/80; #7 78/78; corrected local build 85/85 | #5–#7 essentially flat. Old snapshots failed four selected image checks; corrected local build passed them. These are subset scores. |
| Sep 29, repaired PR #8 (`595c98c`) | Full 97, nearest 115 FPS; nearby current full 96/96 | Effectively flat. A timed score alone does not prove image validation. |
| Sep 29, live visual verification | Full 103; separate texture probe 162 FPS | Different run conditions. Initial unbounded texture stress crashed the Intel Vulkan driver; finite full suite later completed. |
| PR #14 author claim | Host score 2072 from an earlier PR state | External claim, not reproduced on this VM or a fresh comparison of the merged revision. |

Evidence: `artifacts/investigation-20260928/`, `artifacts/accuracy-20260928/`
and its `pr-vm/` subtree. The latter's `RESULTS.md` retains the framebuffer-reuse
scene means and caveats. Profiling-only results (texture 113, buffer 76,
desktop 35, refract 26) are listed in the run inventory, not treated as clean A/B
timings. Failed experiments and crashes are not silently discarded as successes.

## September 30–October 1: desktop-size cost and integration gaps

| Change or experiment | Scores / selected FPS | Assessment |
| --- | --- | --- |
| Sep 30, current fixes inside desktop vs isolated X | Five-scene scores 59/60 vs 98/99 | Different display environments; three selected image checks failed. Not a clean code gain. |
| Mip-view and API fixes, four-scene comparison | Nearest/mipmap/Phong/blur 136/130/134/42 → 135/141/135/43 FPS | Single samples; only nearest passed validation. No demonstrated throughput gain. |
| No-cull experiment | Four-scene score 111 vs mip-fix 112; working-tree run 109 | Did not resolve rendering and was reverted. Subset scores. |
| Fresh normal full suite | 68 | Not comparable with five-scene 98/99. |
| Desktop-size control, unchanged driver | Nearest about 76 on 1920×1080 desktop vs 123–124 on 800×600 desktop | Direct evidence that presentation scales with desktop size, even for an 800×600 window. |
| Visible-region readback and bulk row copies | Full 68 → 94; nearest 76 → 106, Phong 75 → 104, blur 34 → 40 FPS | Measured improvement with the 1080p desktop retained. Four selected checks pass afterward. Three four-scene scores 69/70/70 are separate subset results. |
| Sep 30, GitHub base and first fixed PRs | Main 48/48; #16 48/48; #17 49/49; installed local optimized 98/98 | PRs did not contain local optimizations. Main/#16/#17 had 21 validator failures; local had zero. This is an integration gap, not isolated evidence that the PR edits caused a 50% loss. |
| Oct 1, later PR heads | Local optimized 98; #16 `59a97fc` 49; #17 `e01f3bc` 48 | All 27 known-result checks pass. Performance work still missing from PR heads; roughly half local throughput. |
| Author host-harness claims for those PRs | #16 1221; #17 1223 | Host harness bypasses the VM; these claims did not predict guest 49/48. |
| Port local optimization into PR #16 | Baseline normal 94, short repeats 98/98/98; candidate normal 110 then 108, repeats 108/108/108 | Initially higher, but visual checks found black Refract and incomplete Shadow/Ideas despite 27 validator successes. Not accepted as complete correctness. |
| Correct depth, sampler lifetimes, transfer ordering and batching | First verified #16 normal 284, repeats 285/284/285; #17 normal 286, repeats 286/285/286; return baseline 92/93/94 | Large recovered gain with additional visual checks. Not a separate speedup attribution for each fix. |
| Subsequent complete candidates | #16 282/288/288; #17 280/286/285 | Interim candidates, superseded by later checks. |
| Transfer-ordering release candidates | #16 286/287/287; #17 281/284/284 | Gain retained after ordering fix. |
| Final PR #16/#17 sets | #16 283/289/290 (median 289); #17 279/286/285 (median 285) | Final comparable guest sets; small #17 difference not isolated as hardening overhead. |
| Vulkan-validation hardware runs | #17 full 228–229 | Diagnostic instrumentation; do not compare directly with release 285. |

Evidence: `artifacts/performance-20260930/`,
`artifacts/pr-merge-benchmark-20260930/results.json`,
`artifacts/pr-latest-test-20261001/results.json` and
`artifacts/pr-integration-20261001/`. Individual visual-scene and stress-run
scores, including failed low-FPS probes, are retained in the run inventory.

## October 2–3: confirmed regression, repair and 500-plus recovery

| Change or experiment | Timed scores | Assessment |
| --- | --- | --- |
| Experimental Minecraft patch vs original, five scenes | Original 435/504/504, median **504**; patch 288/285/285, median **285**; repeat 290 | **Confirmed 43.5% loss** in this matched subset. Not a full-suite 504 → 285 result. Patch also had fence and display defects. |
| First safer rewrite, five scenes | 398/476/481 | Interim result; below original. |
| Additional iterations, five scenes | 520/515/518; then 513/513/515 | Interim improvements. Do not confuse these subset scores with full-suite recovery. |
| Final v3 before scanout rewrite | Subset 546/549/541, median **546**; full 551/543/542, median **543** | Original matched subset 499/496/495, median 496; original full 503/504/503, median 503. Better measured throughput, but later desktop corruption prevented treating this snapshot as finished. |
| First scanout rewrite | Black VNC display while guest rendered | Failed test; throughput not accepted as a successful optimization. |
| Oct 3, old saved adapter vs repaired current adapter | Full old 173/174/174, median **174**; current 545/533/544, median **544** | Same full selection, 800×600, three-second scenes, vblank off. Both built-in validation sets pass, but old adapter's live display corruption limits equal-quality interpretation. |
| Oct 3, fresh PR-review baseline | Full 549/556/556, median **556** | **Yes, 556 was repeatedly measured.** |
| Staging-wrap repair, PR #22 | 554/552/556, median 554 | Essentially preserves baseline throughput. |
| DX lifecycle correction, PR #23 | 559/556/555, median 556 | Essentially preserves baseline throughput; no DX rendering claim. |
| Core/adapter build repair, PR #24 | 553/556/555, median 555 | Essentially preserves baseline throughput. |
| Preload release policy, PR #18 | 558/556/555, median 556 | Essentially preserves baseline throughput. |

The October 3 PR comparisons use a 0.5-second-per-scene warmup and one-second
timed scenes; they are not the same timing protocol as the preceding three-second
A/B. The later Piglit replay found **64 passes, 70 failures, five skips and seven
timeouts** on that era's renderer. Glmark2's 27 successes did not establish broad
accuracy, so 556 is a throughput result for a less accurate build.

Evidence: `artifacts/minecraft-final-review-20261002/`,
`artifacts/minecraft-safe-20261002/`, `artifacts/glmark2-ab-20261003/`,
and `artifacts/pr-review-20261003/vm-bench/summary.json`.

**Reporting correction found during this audit:** the October 2 conversation
said both experimental and baseline adapters passed all 27 validators. The saved
experimental `final-glmark2-validate.log` and corrected `review.md` actually
record **26 successes and one mipmap failure**; baseline has 27 successes. Raw
evidence takes precedence over that conversation claim.

## October 4: loss from 556, accuracy work and host-profile changes

| Build / host profile | Timed scores | Median | What this establishes |
| --- | --- | ---: | --- |
| Accuracy candidate | 227/235/255 | 235 | Historical throughput much lower than 556. |
| Previous renderer retested at that time | 238/248/193 | 238 | Old code also slowed; does not attribute the whole loss to accuracy changes. |
| Higher GPU clock, intermediate build | 404/406/424 | 406 | Clock profile explains substantial throughput variation. |
| Combined intermediate accuracy build | 407/403/413 | 407 | Comparable neighborhood, still below 500. |
| Higher CPU-priority intermediate probe | 435/433/433 | 433 | Scheduler setting improved observed throughput. |
| Intermediate `17368fa`, VM105 paused | 533/531/532 | **532** | 500 reached in this paused-VM configuration, not normal background load. |
| Final accuracy `384081f`, GPU min 350, weight 100, VM105 running | 176/180/173 | 176 | Low-clock / contended profile. |
| Same, VM105 paused | 267/269/267 | 267 | Pausing VM helps but does not recover old throughput. |
| GPU min 1100, weight 100, VM105 running | 230/231/231 | 231 | Higher GPU clock alone does not remove CPU contention. |
| Same, VM105 paused | 465/455/466 | 465 | Remaining gap from 532; different build/background load. |
| GPU min 1100, weight 1000, VM105 running | 363/354/363 | 363 | Scheduling helps. |
| Same, VM105 paused | 478/471/465 | 471 | Still below 500. |
| Add separate vCPU affinity, VM105 running | 443/450/435 | **443** | Final repeatedly tested operating profile. |
| Same affinity, VM105 paused | 462/463/472 | **463** | Pausing alone no longer restores 500. |
| Single affinity / extreme-weight probes | 433 / 452 | — | Exploratory single runs, not repeated revision comparisons. |

Jellyfin remained active and consumed roughly one CPU core; VM105 consumed about
two while running. The early 556 runs lack equivalent background-load records.
The old-renderer retest is useful evidence for environmental effects, but it does
**not** prove there was no additional code regression. The 532 intermediate build
and the final 463 paused result are also not a controlled per-fix comparison.

Final accuracy improved to **140 passes, one known API expectation mismatch,
five unsupported skips and zero timeouts/recoveries**. That is a real improvement
over the 556-era renderer. The history does not quantify the cost of each accuracy
fix independently.

Evidence: `artifacts/piglit-accuracy-benchmark/`,
`artifacts/piglit-accuracy-baseline-benchmark/`, and `artifacts/full-accuracy/`.
See [the accuracy report](piglit-accuracy-20261004.md) for the profile matrix.

## October 4–7: smaller optimizations and current baseline

| Change / reference | Scores or operation times | Assessment |
| --- | --- | --- |
| Cached coherent readback / visibility candidate | Initial baseline 410/412/408 (median 410); candidate 445/439/444 (444) | Apparent +8.3%, but baseline drift invalidated causal claim. |
| Return baseline and same candidate | Baseline 447/446/442 (446); candidate 440/446/444 (444) | **−0.45%**, overlapping samples; no whole-suite gain established. |
| Profiling runs | Baseline 409; candidate 425 | Instrumented, not clean throughput samples. |
| Startup FIFO trace-option cache | CPU dispatch median **80.089 → 45.024 ns/command**, −43.8% cost | Verified targeted CPU benefit, not 43.8% more FPS. |
| Final tracing candidate `6847241` | Full 450/449/453, median **450** | About +0.9% versus return baseline 446; no reliable whole-suite speedup. |
| Oct 5, fresh software vs SVGA | llvmpipe 194/193/190 (193); SVGA 430/430/436 (**430**) | Same current load, different supported coverage. Native software includes terrain. |
| Native Intel host reference | 1677/1588/1620, median **1620** | Direct host rendering, not VM passthrough or a renderer change. |
| Presentation/shader reuse `2a60939` | Single full probe **410 → 443** | Separated by accuracy replay and load changes; does not establish an 8% whole-suite gain. |
| Oct 7, isolated depth-bias addition | Before 303/296/301/294 (298.5); after 301/301/297/302 (301) | **Validation enabled**, contrary to original report. +0.84% inside variation; cannot compare these scores with release 556. |
| Oct 7, diagnostic installed adapter | Warmup 464, timed **465** | Older `2a60939` under current load; one timed reference, not a matched A/B against current source. |
| Oct 7, diagnostic current debug adapter | Timed 302/289; profiled 287 | Validation-layer overhead; not release throughput. |
| Oct 7, release image-format-list ABBA | Before 437/438/442/440 (439); after 447/442/446/437 (444) | +1.14%, overlapping ranges; no reliable whole-suite gain. |
| Oct 7, release format list + safe constant reuse ABA | Before 442/443/442/441 (**442**); after 438/440 (**439**) | **−0.68% measured**. No guest FPS improvement demonstrated; stable causality not established by six short runs. |
| Constant-reuse focused local lavapipe | Median **50.17 → 3.35 µs/draw**, −93.3% elapsed time | Ten alternating pairs, 3,000 identical-constant draws per sample, pixels verified. Synthetic software-Vulkan workload, not guest throughput. |

The October 5 presentation microbenchmarks used five alternating pairs on the
Intel host and verified framebuffer output:

| Operation | Before | After | Time reduction |
| --- | ---: | ---: | ---: |
| Full frame | 904.54 µs | 810.36 µs | 10.4% |
| 64×64 update | 629.78 µs | 90.31 µs | 85.7% |
| Overlapping clips | 127.77 µs | 122.23 µs | 4.3% |
| Fully offscreen update | 603.10 µs | 0.34 µs | 99.9% |
| Identical shader redefinition | 17.60 µs | 0.063 µs | 99.6% |

These are useful operation improvements, but neither they nor the constant
benchmark establish faster glmark2. A claim that the latest changes made the
VM perform better overall would exceed the evidence.

Evidence: `artifacts/performance-20261004/`,
`artifacts/llvmpipe-glmark2-20261005/`,
`artifacts/native-intel-baseline-20261005/`,
`artifacts/presentation-efficiency-20261005/`,
`artifacts/depth-bias-benchmark-20261007/`, and
`artifacts/performance-recovery-20261007/`.
See [the October 4 report](renderer-performance-20261004.md),
[presentation report](presentation-efficiency-20261005.md), and
[October 7 report](renderer-performance-20261007.md).

## Which scenes are now slower than the 556-era release

Comparing three October 3 PR #18 runs (median full score 556) with four October 7
current-source baseline runs (median full score 442), **all 32 common rendered
scenes have lower median FPS**. The losses range from 16.5% to 32.1%. This is a
cross-date comparison, not an isolated code experiment, but shows the remaining
gap is broad rather than a single unsupported scene changing the average.

| Scene | Oct 3 median FPS | Oct 7 baseline median FPS | Difference |
| --- | ---: | ---: | ---: |
| Build, VBO | 709 | 563 | −20.6% |
| Texture, nearest | 668 | 543 | −18.7% |
| Texture, mipmap | 660 | 522 | −20.9% |
| Phong | 645 | 521.5 | −19.1% |
| Desktop blur | 362 | 266.5 | −26.4% |
| Buffer subdata | 381 | 283.5 | −25.6% |
| Ideas | 294 | 224.5 | −23.6% |
| Shadow | 519 | 420.5 | −19.0% |
| Refract | 140 | 95 | −32.1% |

The [complete scene comparison](benchmark-history-scenes-20261007.csv) contains
all 32 medians. Different background load, correctness and binaries still prevent
attributing those decreases to one change.

## Minecraft observations kept separate

| Observation / comparison | Recorded FPS | Limit |
| --- | --- | --- |
| Oct 2 initial Minecraft | 2–3 at 1920×1028, 12 chunks | Not comparable with later smaller windows and fewer chunks. |
| Experimental patch captures | Reported 11–35; live 16 at 1920×1028; later saved 47 at 854×480 | Corruption/black windows and changed settings; no verified 23–25× library gain. |
| Safer rewrite captures | 36 at 854×480; old baseline about 44 | Different snapshots; not controlled gameplay benchmark. |
| Oct 3 stationary readings | 56 initially, then 46/46/45 at 854×480; later 27 in another view | Sparse F3 observations, not frame-time averages. |
| Matched old/current stationary scene | Old 31–32, mean 31.4; current 37–38, mean 37.6 | About +19.7% for that view; five readings each, old rendering corrupted. Earlier 2 FPS did not reproduce. |
| Modern/Fabulously Optimized Minecraft | Software fallback / launch and API-compatibility problems | No measured accelerated before/after benchmark found. |

Evidence: `artifacts/minecraft-fps-20261003/`,
`artifacts/minecraft-ab-20261003/` and the October 2 review/safe-rewrite trees.
Game settings, camera, pixel count and GPU-clock changes cannot be relabeled
as renderer-only improvements.

## Where regression coverage was missing

The following October 5 runtime commits have host correctness evidence but no
recorded independent guest throughput comparison per change in the reviewed
history. The old installed adapter was left running while source evolved.

| Commit | Change | Guest benchmark gap |
| --- | --- | --- |
| `0cbe471` | FIFO framing and guest-controlled memory bounds | No per-change timing |
| `e13d4f1` | Query lifecycle/completion | No per-change timing |
| `f3f77b9` | Draw/copy validation and failed replacements | No per-change timing |
| `783fd8b` | Shader math/register contracts | No per-change timing |
| `0aaf869` | Sampling/surface feature advertisement | No per-change timing |
| `a07a632` | Capacity bounds and dynamic stencil | No per-change timing |
| `468a4c9` | Render features, clear/depth-range state | No per-change timing |
| `a89f1f8` | Sampler filters and mip state | No per-change timing |
| `09de26f` | Deferred deletion and failure draining | No per-change timing |
| `3843eb1` | Clipping and line width | No per-change timing |
| `c3c3bac` | Vertex semantics / SM2 pixel inputs | No per-change timing |
| `34483e5` | Signed formats / mipmap checks | No per-change timing |
| `298a207` | Remove caches/heuristics and tighten preload policy | No guest test caught `0x20001 → 0x20000` breaking accelerated Xorg |
| `7096042` | Fixed-function subset contracts | No per-change timing |
| `10ace66` | Runtime integer/boolean constants | No per-change timing |
| `3d7af8e` | Screen-blit scaling/support contracts | No per-change timing |
| `32504fe` | Subresource layouts / DMA barriers | No per-change timing |
| `753fb24` | Stretch-blit subresources / same-image snapshots | No per-change timing |
| `36f9c99` | Depth formats/stencil/support | No per-change timing |

October 6 `93aeaff` then added gamma, relative constants, depth bias and
device-loss contracts. Only depth bias was isolated in the first October 7
comparison, and that comparison had validation enabled. The corrected release
benchmarks compare two builds containing all those fixes; they do not measure
their combined cost against `2a60939`. The single 465 installed-adapter reference
versus 439–442 current-source results suggests a possible additional roughly
5–6% gap, but is insufficient to attribute it or call it a confirmed regression.

The performance loss was noticed in earlier conversations, especially October
2 and October 4. What was missing was a consistently recorded release baseline,
guest replay after every substantial integration group, and a matched test of
the old 556-producing binary against the final accurate binary under one profile.
Passing host tests and unchanged glmark2 validators repeatedly coexisted with
large throughput losses or visible defects.

A useful next investigation is an alternating release comparison of the exact
556-era adapter, `384081f`, `2a60939`, `f067e37`, and `93aeaff`, with explicit
validation off, verified process maps, one common compatibility value, fixed
desktop/window sizes, clocks, scheduling, warmups and scene durations, and
recorded background load. Then narrow any reproducible loss between adjacent
revisions while retaining pixel checks. This audit did not perform that replay.

## Thread coverage

| Codex session | Topic / benchmark contribution |
| --- | --- |
| `01a093a2-561e-7b40-9b31-ae2d9358fd0c` | Publication/original project discussion from another workspace; early speculative and external QNX/JIT claims |
| `01a0e3b3-d254-7b80-8c59-a35f4787b9b4` | Line count; no benchmark |
| `01a0e3c3-ce18-7202-8eb7-402eae955c49` | Main Sep 27–Oct 2 performance history, PR integration, Minecraft regression audit |
| `01a0e617-cf16-7dd2-881b-6b6d34409219` | Guest OS discussion; no independent benchmark |
| `01a0f2c5-a5ee-7ae3-9244-3ad6a5db6b99` | Cross-platform code review; no independent guest timing |
| `01a0fa8d-bd13-7ba1-8a25-e188c959ac16` | Cross-workspace Minecraft patch audit; no independent timed result |
| `01a0ff34-c4b8-72a2-a943-1d8a6df655e7` | Scanout continuation, matched Minecraft and Oct 3 glmark2 old/new comparison |
| `01a0fff2-32b0-7f10-a722-d514bb321787` | Modern Minecraft compatibility/launch; no accelerated benchmark |
| `01a10000-17c7-7c42-8db4-df482c15c03e` | Oct 3 PR benchmarks, Piglit/accuracy, host tuning, native/software references, presentation optimization |
| `01a10005-63bf-7422-b49b-19b0bb2a10af` | Logo; no benchmark |
| `01a1000d-6063-7dc1-9365-2a8988bb51b0` | PR review; no independent timing |
| `01a10010-0763-7831-857e-4fd0aa277219` | Project duration; repeated existing evidence |
| `01a10cca-ee3d-7033-907d-cc8ca4ab83a2` | README information; repeats existing benchmark reports |
| `01a10d8d-fb0a-7e40-84dc-aec8392078dd` | Issue review; no independent timing |
| `01a10d94-9a61-7031-9652-e864a8c6b6c8` | October 5 correctness commits; no per-change guest throughput baseline |
| `01a10da1-bb33-71e0-9907-063f7dbdf041` | README draft review; no benchmark |
| `01a10f65-d6b9-7180-9f67-f2b67ed4b3e4` | CI failure discussion; no guest benchmark |
| `01a10f68-5be2-7f13-bbd9-2a85e92533e7` | Depth-bias continuation and Oct 7 validation-policy correction/release measurements |
| `01a11157-cb35-7593-ba69-d3c6928da56f` | Open-issue status; no benchmark |

Scores such as 705, 1,104 and 1,855 in short build/graphics-health probes, and other one-scene
recovery checks, are not later full-suite records. They are retained in the
inventory as health/diagnostic evidence. Failed or interrupted tests without a
completed score remain failures/incomplete measurements, not zero-FPS samples.


## Same-day follow-up after the inventory

The [retained-fixes follow-up](renderer-performance-20261007.md#follow-up-exact-historical-binary-and-retained-candidate)
retested the exact `9270408e` adapter recorded at 558/556/555. Under today's
running-workload profile it scored **466, 470, 465, 464** (median **465.5**).
The current correctness baseline scored **438, 441, 441, 439**, and the
additional direct-state-lookup candidate scored **445, 439, 441, 438**; both
medians are **440**. Thus most of the cross-date score drop reproduces with
the original adapter itself, while a smaller 5.48% matched gap remains between
the historical and corrected builds. This does not attribute that remaining
gap to a particular commit or make the historical 70-failure Piglit build an
acceptable replacement.

The follow-up also records the rejected +0.8% uniform-robustness experiment,
the native submission probe, and a 17.38% focused draw-cost improvement that
does not raise the full-suite median. An initial diagnostic used another
PR18-era binary with a different hash; the identity mismatch and exact retest
are documented. These subsequent measurements are listed in that report;
the 508-file CSV inventory above remains the original thread-audit snapshot.

The October 7 full replay also caught an integration regression that the
benchmark validators missed: the corrected DF24 capability caused Mesa to
select a legacy depth format and omit shadow comparison code. The first
current-build replay stopped at 88 pass / 3 skip / 1 new fail. An initial
normalized D24S8_INT fallback passed the selected replay but cost **2.27%**
in its matched driver benchmark (441 → 431). It was removed. The final opt-in
fix keeps the original formats and retains Mesa's shader shadow comparisons.
Its matched driver benchmark scored **436, 428, 438, 434** before (median **435**)
and **438, 436, 438, 429** after (median **437**): **+0.46%**, with overlapping
ranges and no established speedup. That driver subsequently failed `arb_depth_texture@texdepth` at case 112
(108 pass / three skip / one fail), despite passing all shadow variants.
A second legacy depth-blit guard needs correction; see the retained-fixes
report for the replacement driver and its correctness evidence.
This is another reason to keep performance scores and correctness evidence
attached to exact adapter **and guest-driver** identities.

The replacement driver also enables the opt-in raw-depth blit path and
passes nine focused cases, including `texdepth`. Its matched old/new/new/old
benchmark scored **436, 434, 443, 429** before (median **435**) and
**431, 431, 435, 434** after (median **432.5**): **−0.57%**, with overlapping
ranges. Its completed 146-case replay recorded **140 pass / one known API-expectation
fail / five skip / zero timeout**, with zero new regressions, recoveries,
Vulkan errors or warnings, and GPU hangs. The original adapter and affinity
were restored, and the default driver stayed unchanged. Neither raw-depth experiment recovered
500+, and the superseded +0.46% comparison must not be attributed to the
replacement driver.
