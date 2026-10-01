# HSoB/ph3 mobile continuation — 2026-10-01

Active owner: `D:/workspace/eagler/worktrees/hsob-eagler`, branch
`eagler/hsob-adapter`, inherited HEAD `bad2017`. The older
`fangame/engines/ph3-open-web` checkout is not the source owner for this work.

This continues `MOBILE_VM_PERFORMANCE_20261001.md`. All changes remain local,
uncommitted and undeployed. Preserve the inherited dirty changes and unrelated
ExRumia filename changes. Launcher/UI dirty changes were not edited here.

## Starting point actually verified

The inherited candidate was **mobile-arithmetic11-fixed**, not the rejected
unguarded arithmetic11 experiment. Its actual artifacts already passed the
29,577-tick original-replay-01 stage-1/2 comparison and the 15,099-tick partial
stage-6 comparison. Older prose saying those integrations were pending was
stale. The latter still ends at frame 15,098 rather than the recorded 34,829;
matching that reference does not establish stage-6 completion.

The inherited throttled baseline was highly variable. The median of Boss
per-second P95 frame-cost windows changed from 188.7 ms to 114.8 ms when gpu7
was repeated; arithmetic11-fixed measured 71.6 ms. These are not pooled frame
P95s, and not physical-phone results. See
`build/mobile-optimization-evidence/gpu7-bracket-arithmetic11-throttle4-summary.json`.

## Implemented in this continuation

### Standalone safety tests and measurement failures

The standalone VM test now supplies its null `Logger::top_` owner. Both legacy
and optimized AddressSanitizer runs previously stopped at this missing symbol;
both now pass (`--asan --tag=continuation12`). VM and platform runners no longer
turn a signal/null process status into exit 0. Platform tests also support
`--asan` with a separate output name.

`run-campaign-local.mjs` owns the local read-only preview server for one probe
and shuts it down with the probe. The first code14 stage-6 integration attempt
failed before gameplay because the detached server's timeout had expired
(`ERR_CONNECTION_REFUSED`, zero samples). The resumed run passed; retain both
artifacts rather than classifying the infrastructure failure as a game crash.

`summarize-campaign-runs.py` now explicitly excludes CPU profiling, failed
runs, and diagnostic variants from its timing-comparable flag. An existing
CPU-profile run and an ordinary pressure run exercise that distinction in
`build/mobile-optimization-evidence/profile-exclusion-stagecache15.json`.

### mobile-render12: reuse empty render-list nodes

`gstd/ListNodePool.hpp` reuses up to 4,096 empty list nodes per object manager.
`ClearRenderBucket` releases every object reference immediately; the pool owns
storage, not live game objects. Drawing order, list iterator stability and the
original bucket containers remain unchanged. Oversized scenes are not cut off.
This applies to the main stage/package and offscreen render paths. Native
builds retain the original path; `PH3_LEGACY_RENDER_NODE_POOL` is the Web A/B gate.

The Wasm microbenchmark (`node portable/test-render-node-pool.mjs`) checks real
gstd ownership and checksums with 512 objects/101 buckets and eight alternating
samples. Median queue time was 11.45025 ms old versus 9.4808 ms pooled, for 2,000
iterations per sample. **This approximately 17.2% queue-only improvement is not
an overall game or phone speedup.** Evidence: `build/render-node-benchmark.json`.

### mobile-render13: avoid repeated negative mesh RTTI queries

The Web render loops query a mesh capability instead of repeatedly using
`dynamic_cast` for every non-mesh object. Mesh subclasses inherit the positive
query. The original shared mesh's `IsCoordinate2D` state and depth clear logic
remain authoritative; object-local flags are not substituted.

Every query in verifier builds is compared with the old RTTI result and throws
on disagreement. Native builds and `PH3_LEGACY_RENDER_MESH_QUERY` use RTTI.

### mobile-code14: 16-byte tagged bytecode operands

The mutually exclusive literal pointer, lexical slot, call target and cached
branch target share the instruction operand union. Web32 instructions shrink
from 24 to 16 bytes, enforced at compile time; line numbers, pointers, indices
and binary64 values are not narrowed. `PH3_LEGACY_SCRIPT_CODE_LAYOUT` retains
the prior layout. Runtime data semantics, scheduling and instruction order do
not change.

The compact and old layouts produce the same VM test output in both exception
modes and under Wasm AddressSanitizer (`--tag=code14`). Tests exercise repeated
cached branches, alternative/case, nested loops, source literals, reentrant
events, COW, writable aliases, pause/resume and exception unwinding. This saves
one third of **instruction storage**, not one third of total process memory.

### mobile-stagecache15: retire obsolete stage prewarming

The full original-menu replay exposed cumulative retained compiled scripts:
every stage prewarmed its scripts into one campaign-long cache, without
releasing unused earlier-stage prewarm. `PrewarmCacheScope.hpp` tracks explicitly
prewarmed entries and retires cache-only entries at the next stage prewarm.
Live clients, unmarked cache entries, and all of the new stage's prewarm are
retained. Entries protected by a live client can be reconsidered at a later
stage boundary. There is no in-stage bytecode eviction/recompilation policy.

`PH3_LEGACY_STAGE_SCRIPT_CACHE` retains the old Web behavior; native behavior
is unchanged. The stage observer records retired entry count and expanded
source bytes, the latter explicitly excluding bytecode/literal storage.

The generic retirement tests pass in JS and Wasm exception modes and with
Wasm AddressSanitizer, including delayed client release and unmarked entry
preservation. Standard and verifier stagecache15 builds completed after fixing
the initial cache-owner getter typo. Gameplay verification is tracked below.

### mobile-uniform16: cache custom-effect parameter uploads

The inherited fixed-pipeline cache did not cover custom effects.
`source/GcLib/portable/UniformValueCache.hpp` now tracks the last actually applied bytes,
operation kind and count per linked-program location. Numeric parameters,
matrices, sampler integers and the reserved alpha-test override share one cache.
Texture binding/feedback resolution and writer ordering are unchanged. Array
uploads conservatively invalidate possible independently addressed element
aliases rather than assuming uniform locations have arithmetic meaning.

Signed zero, NaN payloads, vector/matrix shape changes, program isolation,
relink/loss invalidation and overlapping writers have focused tests. JS and
Wasm exception variants and Wasm AddressSanitizer all pass. The standard and
verifier builds completed. `PH3_LEGACY_EFFECT_UNIFORMS` is the A/B opt-out.
Actual game comparisons and new pressure measurement follow below; a reduced
number of driver calls is not by itself a claim of phone FPS improvement.

## Gameplay evidence ledger

Original resources and replay bytes were not edited. Tests only hide other
replay names in a disposable browser FS to make original-menu selection stable.
Diagnostic accelerated playback is not a timing measurement.

| Candidate | Original replay 01 stages 1/2 | Original replay 01 stage 6 |
| --- | --- | --- |
| render12 | 29,577 ticks, 6 images match | 15,099 covered ticks, 5 images match |
| render13 | 29,577 ticks, 6 images match, RTTI checks pass | Same partial 15,099 ticks/5 images, RTTI checks pass |
| code14 | 29,577 ticks, 6 images match | Resumed run: 15,099 covered ticks, 5 images match |
| stagecache15 | 29,577 ticks, 6 images match | Not run separately; cumulative validation below |
| uniform16 | 29,577 ticks, 6 images match | 15,099 covered ticks, 5 images match |

Images match outside only `[980,790,1080,815]`, the known FPS-number rectangle;
full-image changed bounds remain in every `equivalence.json`. Prefix references
are `build/verify-prefix12-reference` and `build/verify-stage6-reference`, not
newly blessed candidate output. Stage-1/2 trace hash remains
`24d1a50ce6955e8821bfe46f4ef51a352c3d8574e9bc46f3641c62a5a2950c4f`;
partial-stage-6 hash remains
`79cfb090eb6b1f81f7d7700fb6b1a1f9562a9b88d0fccd65c2704aed01469ea4`.

Additional original stage-6 fixtures:

- Replay 03 on arithmetic11-fixed reached frame 17,972 with no runtime error,
  but ended with no lives; its recorded endpoint is 31,361. This is not a pass.
- Replay 05 on render13 reached frame 26,127 with no runtime error and two lives;
  score 3,373,565,380 versus recorded 3,373,565,390 at frame 27,322. Neither the
  timing nor the final score is a full recorded-replay match.
- A **continuous** replay-05 code14 run (`--fast=8 --trace-stages=6 --memory`,
  400-second observation window) traversed stages 1–5 and reached stage 6 frame
  10,895, beyond the earlier blocking-load exception at 10,229. No runtime
  error/context loss occurred, but the window ended before full stage 6.
  Wasm high-water memory reached **679.4375 MiB**, versus roughly 275 MiB in
  isolated stage-6 probes. GPU logical texture bytes peaked at 154,079,692.
  See `build/verify-full-replay05-code14-20261001` and its generated memory
  summary. Do not sum browser JS/Wasm counters as independent physical RAM.
- A continuous stagecache15 replay-05 run with the same diagnostic mode ended
  its 520-second observation window in stage 5 (last sample frame 28,543,
  last trace frame 28,607), with no runtime error or context loss. Do not compare
  this wall duration with the earlier run as performance data. The full strict
  trace comparator returns **false** because both traces are incomplete and
  stop at different positions; its first difference is **candidate EOF only**
  at line 99,399, reference stage 5 frame 28,608. All preceding state/input/RNG
  records agree. No trace was cropped, reblessed or resynchronized.

At matched stage/frame windows, retained Wasm memory is:

| Window (original replay 05) | code14 | stagecache15 |
| --- | ---: | ---: |
| Stage 1, 5,000–7,399 | 229.4375 MiB | 229.4375 MiB |
| Stage 2, 10,000–13,999 | 275.375 MiB | 229.4375 MiB |
| Stage 3, 10,000–13,999 | 396.625 MiB | 229.4375 MiB |
| Stage 4, 10,000–13,999 | 476 MiB | 275.375 MiB |
| Stage 5, 27,000–27,999 | 679.4375 MiB | 330.5 MiB |

Evidence: `build/mobile-optimization-evidence/stagecache15-matched-stage-memory.json`.
The last window shows about **51.4% less Wasm high-water memory** for the same
simulation range; it is not a measurement of total Android process RAM. The
new run has not yet established continuous stage-6 completion.

## Ordinary standard-runtime pressure pair

Fresh sequential original-replay-01 stage-1 runs, fourfold Chrome CPU throttle
from stage start, no trace/fast mode/CPU profiler/GL allocation observer, same
12,800-frame target, both complete with no runtime error:

| Metric: median of per-second presented-callback P95 windows | arithmetic11 repeat | stagecache15 |
| --- | ---: | ---: |
| Stage-1 road | 46.4 ms | 37.05 ms |
| Stage-1 Boss | 126.4 ms | 127.35 ms |

See `build/mobile-optimization-evidence/continuation15-pressure.json`. There is
**no demonstrated Boss slowdown fix from render12/code14/stagecache15 in this
pair**. The memory gain is real but must not be substituted for FPS acceptance.
The previous baseline already varied substantially between runs; one pair is
not a robust causal estimate of the smaller road timing difference.

## Remaining validation while this continuation is active

Uniform16's prefix and partial-stage-6 comparisons completed successfully.
The later `build/verify-full-replay05-uniform16-fast32-20261001` run failed with
`Attempted to use detached Frame`; its last saved stage snapshot is stage 4,
frame 5,989, with 275.375 MiB of Wasm memory. This is incomplete browser-run
evidence, not full-campaign success and not a diagnosed phone crash. The older
code14 fast32 comparison run timed out at stage 6 frame 20,247. These two runs
do not establish full-run equivalence. Uniform16's ordinary Boss pressure
measurement is still pending.

For the subsequent ph3-modern source consolidation and next actions, start at
[`../AGENT_HANDOFF.md`](../AGENT_HANDOFF.md), rather than treating this dated
continuation log as the current worktree map.

`adb devices -l` was checked again and returned no attached device. Desktop
Chrome, CPU throttle, logical GL allocation counters and sanitizer tests cannot
establish physical RMX5080 WebView acceptance. Do not call the user's phone
slowdown or crash completely resolved from the evidence above.

No production/test site was deployed. Full `img.dat`, separate OGG handling,
real bullets/RNG/collisions/input and optional presentation limiting are intact.
