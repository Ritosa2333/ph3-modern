# HSoB / ph3 mobile performance — 2026-10-01

Latest continuation: `MOBILE_VM_PERFORMANCE_20261001.md` records the recovered
gpu7 checkpoint, scalar8 correctness/pressure evidence and literal-pool follow-up.

## Active worktree and scope

Continue in `eagler/worktrees/hsob-eagler`, branch `eagler/hsob-adapter`.
The older `fangame/engines/ph3-open-web` checkout is not the active candidate.
Launcher integration remains in `worktrees/eagler-touhou-hsob`.
Existing touch, audio, launcher and filename changes were already dirty on
entry; do not discard or attribute them to this optimization pass.

The user reports severe Android WebView Boss slowdown and a stage-6 road
crash. Desktop replay equivalence and desktop performance are separate
evidence. No attached Android device was available through `adb devices -l`.
No phone acceptance, full-campaign acceptance, commit, push or deployment is
implied by this report.

## Inherited work, not newly implemented in this pass

- Correct strip/fan-to-triangle-list conversion and adjacent fixed-function
  batching; native Wasm exceptions with a separate JS-exception fallback.
- Bounded script-body allocation, recycled VM environments, value moves,
  fused dispatch and cached bytecode branch targets (`mobile-opt4`).
- Indexed render-state tables, cached GL raster/uniform state, bounded
  streaming vertex buffer, and a 64 MiB opportunistic texture LRU. Active,
  loading and explicitly pinned textures are protected; active resources may
  exceed the cache budget. GPU destruction happens after presentation flushes.
- Cooperative required loading for Boss scripts and mesh rendering. The old
  `build/perf-stage6-before` failed at frame 10229 on a positive blocking
  `Sleep`; the candidate goes past it without replacing `Sleep` with a no-op.

The previous `build/verify-prefix12-opt4` stopped after a detached browser
frame without a usable trace. It was not evidence of correctness. The new
`build/verify-prefix12-opt4-resume-20261001` completed both stage boundaries:
29,577 ticks, exactly the same trace SHA-256 as the reference:
`24d1a50ce6955e8821bfe46f4ef51a352c3d8574e9bc46f3641c62a5a2950c4f`.

## Changes in mobile-opt5

### Recycle only populated VM slots

`lightweight_vector` now remembers the prefix hidden by its original `clear()`
operation. Environment recycling clears the union of that prefix and the
current logical length, not every spare capacity slot. An already popped
64-slot stack needs zero assignments; hidden references after loop unwinding
are still released. Assignment and expansion reset the hidden-prefix bound
because only active elements are copied. This respects the VM's direct
extension of variable-array `length`.

The optional `PH3_LEGACY_STACK_RESET` diagnostic flag retains the old full
capacity scan. Tests cover aliasing, several clear/reuse cycles, direct length
extension, copy/assignment, expansion and oversized-cache release.

### Specialize the common two-dimensional vertex stream

`ScreenVertex.hpp` handles only exact, non-indexed D3D `XYZRHW | DIFFUSE`
and `XYZRHW | DIFFUSE | TEX1` streams without a custom declaration. The caller
validates stride and the whole vertex range once. Indexed, declared, skinned
and other layouts still use the original generic path. No texture, effect,
topology, blending order or gameplay content is omitted.

The original float operation order, half-pixel convention, render-target Y
orientation, RHW-zero behavior and disabled-Z clamp are preserved. The
`PH3_LEGACY_VERTEX_DECODE` diagnostic flag retains generic conversion.
24,576 cases compare all output fields bit-for-bit, including unaligned input,
padded stride, tiny/nonzero viewports, negative RHW, out-of-range Z and colors.
These checks passed with both JS and native Wasm exception builds.

The measurement summarizer was also corrected to read the actual runtime
`displayFps` field rather than the nonexistent `presentedFps` field.

## Correctness evidence for mobile-opt5

Both complete runtime builds linked:

```text
build/web-wasm-exceptions-mobile-opt5
build/web-verify-wasm-exceptions-mobile-opt5
```

The container regressions passed in both exception modes. Optimized and legacy
VM regression output matched (arithmetic, COW, pause/resume, argument/result
ownership, microthreads, reentrant events, branches, return and unwinding).

`build/verify-prefix12-opt5-20261001/equivalence.json` passes against
`build/verify-prefix12-reference`: all 29,577 ticks are identical, and six
requested images have zero changed pixels above the bottom 5% FPS strip.
The full-image differences remain recorded, rather than being suppressed.
Stage 1 ends at frame 14485 / score 32756990; stage 2 ends at frame 15090 /
score 147368070. Both match the untouched replay's recorded results.

`build/verify-stage6-opt5-20261001/equivalence.json` passes against
`build/verify-stage6-reference`: all 15,099 ticks and five requested images
match with the same explicit FPS-strip exclusion. The old frame-10229 crash
does not recur, and no runtime errors are reported in this run.

**Stage 6 is not a full-stage golden pass.** Both reference and candidate end
at frame 15098 / score 3352752270 instead of the recorded frame 34829 /
score 4256112150. This pass preserves the pre-existing behavior; it does not
fix that later replay divergence or exercise the remainder of the stage.

## Performance evidence and remaining acceptance

`build/perf-stage1-opt4-profile-20261001` is the fresh baseline: normal-speed,
original-menu replay, through frame 12800, CPU sampling every 2 ms. Road
frames 5000–7399 and Boss frames 8200–12799 are the comparison windows.
Its per-second callback-P95 medians are 5.5 ms and 7.9 ms respectively.
These are medians of window P95s, not a pooled all-frame P95.

The candidate counterpart is
`build/perf-stage1-opt5-profile-20261001`. Do not derive timing from the fast
trace/capture runs: their performance snapshot can be stale by design.
The candidate's performance result must be read after the normal-speed run
finishes, before deciding whether the optimization is beneficial.

Still required: phone/WebView measurements, later-stage replay investigation
and coverage beyond the known stage-6 end. GPU allocation estimates do not
measure real driver residency, thermal throttling or Android process limits.

## Reproduce the candidate

```powershell
node portable/test-script-containers.mjs --exceptions=wasm
node portable/test-script-containers.mjs --exceptions=js
node portable/test-script-vm.mjs --mode=optimized
node portable/test-script-vm.mjs --mode=legacy
node portable/test-platform.mjs --exceptions=wasm
node portable/test-platform.mjs --exceptions=js
node portable/build-web.mjs --exceptions=wasm --label=mobile-opt5
node portable/build-web.mjs --variant=verify --exceptions=wasm --label=mobile-opt5
```

Serve the verification build on a loopback port with `portable/serve-local.py`.
Use `portable/profile-campaign.mjs --fast=8 --trace --trace-stages=2` for the
stage-1/2 trace and `--stage=6 --fast=8 --trace --trace-stages=1` for stage 6.
Use matching capture lists from the respective reference reports and compare
with `portable/compare-campaign-evidence.py --ignore-bottom=.05`.
Normal-speed performance runs must omit fast, trace, captures and memory
observation; comparisons must use the same CPU-profiling settings.
