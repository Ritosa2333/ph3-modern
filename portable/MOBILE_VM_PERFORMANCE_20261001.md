# HSoB mobile VM continuation — 2026-10-01

Newer continuation: `MOBILE_CONTINUATION_20261001.md` (render queues, mesh
queries, compact bytecode, sanitizer completion and cross-stage memory work).
Use its evidence ledger rather than the pending-work wording below.

Active owner: `D:/workspace/eagler/worktrees/hsob-eagler`.
This continues `MOBILE_PERFORMANCE_20261001.md`. Do not use the older
`fangame/engines/ph3-open-web` checkout for these changes.

## Inherited checkpoint

HEAD remains `bad2017`. The pre-existing dirty changes already contained
triangle-strip/fan batching, Wasm EH, allocation/environment reuse, indexed
state tables, GL raster/uniform caches, vertex streaming, bounded texture
ownership, cooperative required loads, XYZRHW decoding and modulate shaders.
The newest built candidate found was `mobile-gpu7`, not `opt5`.
Its prefix-1/2 and partial-stage-6 differential evidence passed; that did NOT
establish physical-phone performance or completion of stage 6.

The independent earlier `/hsob-perf/` preview is an older publication. None of
this continuation's candidates has been committed, pushed or deployed.

## Scalar8 implementation and correctness

Web `value` stores ordinary real/character/boolean cells inline, preserving
exact binary64 bits and original `type_data` identity. Arrays remain reference
counted. `unique()` boxes an inline scalar before returning a writable array
element handle; `overwrite()` retains the existing reference semantics.
Copy/move assignment saves a source array element before destroying its owner.
`PH3_LEGACY_SCRIPT_SCALARS` retains the boxed implementation for comparison.

The `mobile-scalar8` builds used the SAME compile/link flags as `mobile-gpu7`
(`-O3`, `-flto`, `-ffp-contract=off`, Wasm EH). Source manifests differ only in
`source/GcLib/gstd/Script.hpp` and `Script.cpp`. Standard Wasm: 3,727,610 bytes,
SHA256 `78bac1cb0521fd348d439c0811ed4e59c4113e2cbc1947929ac957b5f50d67c8`.

Completed scalar8 checks:

- Optimized/legacy VM tests under both Wasm and JS EH: all four textual outputs
  identical. Includes signed-zero/NaN bits, type identity, scalar/array ownership,
  writable numeric/string elements, COW, stop/resume, tasks, reentrant events,
  return, termination and unwinding. Container tests pass under both EH modes.
- `build/verify-prefix12-scalar8-20261001`: all 29,577 ticks equal to reference;
  both original replay boundaries match frame/score exactly.
- `build/verify-stage6-scalar8-20261001`: all 15,099 covered ticks equal,
  including the former frame-10229 blocking-load error position. Both versions
  still end at 15098 / 3352752270, not the recorded 34829 / 4256112150.
  This is NOT a full-stage golden pass.
- Eleven snapshots are identical outside the outer FPS rectangle
  `(980,790)-(1080,815)` in 1100x825 captures. No whole bottom strip is excluded
  in the final equivalence reports. Raw differences/bounding boxes are retained.

## Normal-speed pressure comparison

Untouched `Main_replay01.dat`, original menus, standard builds, normal speed,
4x Chrome CPU throttle from stage entry, target frame 12800, browser-side
one-second sampling. No trace/fast playback/CPU profiler/GL memory observer;
no concurrent heavy build or browser test during the measured runs.
This is one desktop pair, NOT phone data or proof of sustained mobile fluency.

| Per-second callback P95 median | gpu7 | scalar8 |
| --- | ---: | ---: |
| Stage 1 road, frames 5000–7399 | 57.40 ms (42 samples) | 50.80 ms (42 samples) |
| Stage 1 Boss, frames 8200–12799 | 188.70 ms (105 samples) | 156.50 ms (94 samples) |
| Whole-run Wasm capacity median | 229.4375 MiB | 275.375 MiB |
| Whole-run Wasm capacity maximum | 275.375 MiB | 275.375 MiB |

Evidence: `build/mobile-optimization-evidence/gpu7-scalar8-throttle4-summary.json`
and its two referenced run directories. These are medians of one-second-window
P95 measurements, not pooled per-frame P95. Logical-frame windows match, but
wall-clock dwell and sample counts differ. The Boss statistic is about 17.1%
lower, yet severe slow frames remain. Wasm capacity grows earlier in scalar8;
no memory reduction is claimed.

## Literal-pool9 follow-up — tested

Every bytecode instruction embeds a `value`, even for non-literal opcodes.
Inline cells enlarge that field unnecessarily. Current source moves literal
values into an engine-owned address-stable deque and keeps only a pointer in
the instruction. `PH3_LEGACY_SCRIPT_LITERALS` retains scalar8's inline-instruction
layout; the fully legacy scalar mode is unchanged. A compile-time check caps
Web32 instruction size at 24 bytes.

A new test generates 3000 literals across code/pool growth and checks that
repeated array-literal returns do not share writable state. All four VM modes
pass with identical output including this test. Standard and verify builds
are `mobile-pool9`; standard Wasm is 3,739,228 bytes, SHA256
`9628b153b262f1769748ebb41f6ee8170840c21e0765391db223926b970663ec`.

`build/verify-prefix12-pool9-20261001` and `build/verify-stage6-pool9-20261001`
pass the same 29,577 / 15,099 tick and eleven image comparisons against the
references, with only the explicit FPS-number rectangle excluded. Stage 6
still ends at the SAME premature frame/score; this does not resolve that bug.

The matching 4x-throttle normal-speed run
`build/perf-stage1-pool9-throttle4-20261001` reaches frame 12800 with no runtime
exception or WebGL context loss. The measured road P95-window median is
**45.20 ms** (41 samples), Boss median
**115.40 ms** (87 samples). Whole-run Wasm capacity median/maximum are both
**229.4375 MiB**, versus gpu7's 275.375 MiB maximum. The Boss statistic is
about 38.8% below the single gpu7 baseline, but this is not a replicated phone
benchmark. Total timed segment elapsed: gpu7 250.245 s, scalar8 241.109 s,
pool9 232.897 s. All used Intel UHD / ANGLE D3D11 on the same desktop.

Combined evidence:
`build/mobile-optimization-evidence/gpu7-scalar8-pool9-throttle4-summary.json`.

## Draw-local transforms — tested

`DrawTransforms.hpp` now snapshots world/view/projection once per draw instead
of looking them up per 3D vertex. The cache is recreated for every draw, is
not used by XYZRHW streams, and preserves the three separate transformations
and float operation order. Unused normal transforms are skipped only when
fixed lighting is disabled or the source has no normal. Skinning positions
and the lit-normal path retain the original math. A
`PH3_LEGACY_VERTEX_CONSTANTS` diagnostic flag bypasses both savings.

The platform suite checks 16,384 vertex transformations bit-for-bit plus
matrix lookup counts and cache renewal between draws; it passes in both EH
modes. Standard and verify `mobile-transform10` builds succeeded. Both
`build/verify-prefix12-transform10-20261001` and
`build/verify-stage6-transform10-20261001` pass the same trace/image comparisons
and retain the same partial-stage-6 failure boundary.

Its normal-speed 4x-throttle run reaches frame 12800 in 229.475 seconds. Road
P95-window median is 30.90 ms (40 samples), Boss median 119.70 ms (89 samples),
and Wasm capacity maximum stays 229.4375 MiB. This is a road improvement in
this sample, NOT a measured Boss improvement over pool9's 115.40 ms. Evidence:
`build/mobile-optimization-evidence/gpu7-pool9-transform10-throttle4-summary.json`.

## Arithmetic11-fixed — unit checks passed, integration checks pending

Six compact opcode hints remove indirect native function calls for ordinary
real addition/subtraction/multiplication/division/negation/comparison. They
are selected by the original native function POINTER, never its name; no
API/user routine can accidentally take this path based on naming. Non-real
operands, vector arithmetic, conversions and original errors use the original
function. Calls still retain stop/resume handling and result-slot ownership.
`PH3_LEGACY_SCRIPT_ARITHMETIC` disables this feature. The bytecode cell remains
the same size. There is no fast-math, FMA, RNG, input or timing-rule change.

The initial unguarded `mobile-arithmetic11` candidate was REJECTED by an added
type-identity test: comparison requires the SAME type object, not merely two
types of kind real. `build/vm-regression/optimized-identity-before.txt` retains
the failure; `legacy-identity-before.txt` passes. Do not publish the unguarded
arithmetic11 binaries. Current source has the identity guard and is building
as `mobile-arithmetic11-fixed` in separate output directories.

The corrected VM suite passes in optimized/legacy and Wasm/JS EH modes with
identical outputs. It covers exact type identity, array fallback, large-real
rounding, signed zero, infinities and the original ph3 NaN comparison behavior,
in addition to all preceding COW, task and exception tests. Actual campaign
traces, images and normal-speed timing still have to pass for this candidate.

The read-only `inspect-replay-fixtures.py` inventory also found untouched
Main replay 03/04/05 fixtures containing stage 6, with recorded endpoints
31361/31245/27322 respectively. They are candidates for additional original
playback coverage, not evidence of an actual run or a passed replay yet.

Physical Android testing is unavailable: `adb devices -l` found no device.
No claim is made about RMX5080 GPU/thermals, all six stages, or every cause of
the mobile crash. The remaining replay divergence must stay visible.
