# ph3-modern mobile continuation — 2026-10-06

Owner: `D:/workspace/eagler/worktrees/ph3-modern`, `main`, starting source
checkpoint `e17060c`. Source changes are local and uncommitted. The measurement
unit ended before the subsequent explicitly authorized test publication below.
Read `../AGENT_HANDOFF.md` for private fixtures and the original VM authority.

## Acceptance requested

RMX5080, Android WebView 150 or newer is the eventual device target. The user
then explicitly prioritized **desktop Chromium with 4× CPU throttling first**.
Dense/high-difficulty play should hold 60 Hz for 99% of the time. This has
**not passed**. No Android device was attached when `adb devices -l` was read.

Later the user confirmed other high-load tasks were running and explicitly
withdrew the 99% acceptance requirement, asking this unit to be closed with
judgment. No quiet-machine performance pass or phone pass is claimed.

Do not conflate callback CPU deadlines, delivered display slots and logical
frequency. Keep gameplay, input, RNG, collisions, bullet counts and the
optional presentation policy unchanged.

## Completed checks and limitations

- mobile17: direct numeric result-slot reuse and GL framebuffer/viewport/
  program/vertex binding caches. Original Replay01 stages 1/2: 29,577 ticks and
  six screenshots match the pre-existing reference, excluding only the FPS
  rectangle `(980,790)-(1080,815)`.
- mobile18-simd: adjacent numeric loads/arithmetic, plus a separately flagged
  `-msimd128` build. The same complete prefix comparison passes. SIMD VM
  AddressSanitizer also passes. These are not phone or full-game acceptance.
- The VM-only benchmark excludes parsing. One sequential Node/Wasm sweep
  measured arithmetic/function-loop/nested-array kernels at unfused
  53.9815/66.2116/43.0515 ms, fused 31.6276/50.24565/46.77305 ms, fused SIMD
  27.11805/37.3495/35.0701 ms. In particular the non-SIMD array kernel regressed
  in this sweep. Never substitute these numbers for game FPS.
- mobile19-range: narrow RenderToTexture priority-range preparation avoids
  building unused render-list nodes. Its standard build completed. The first
  Hard timing pair does not prove an improvement. It is included in the latest
  passing prefix/partial-stage-6 differential below.
- mobile20-vm adds fused numeric assignment, explicit lambda inlining and
  original-function-identity hints for real predecessor/successor. Counter
  results preserve the operand's exact type identity. It is included in the
  latest passing game differentials. Its noisy Hard pressure probe completed
  to frame 10,000 but did not establish the original performance target.
- mobile21-strings experimented with caching short, unexposed character-array
  conversions. Const/mutable element exposure disables caching, boxed elements
  are excluded, and append/concatenate invalidate it. The cached build passed
  14 resource-free gates, SIMD AddressSanitizer, 29,577 prefix ticks/six images
  and 15,099 partial-stage-6 ticks/five images against the immutable references.
- **Final default:** VM numeric/load/assignment/counter optimizations, GL
  binding caches and render-range preparation are enabled. String caching is
  opt-in with `-DPH3_ENABLE_SCRIPT_STRINGS=1`; SIMD also stays opt-in. The final
  standard/verifier builds use `-O3 -flto`, without either experiment.

An eight-run alternating Node/Wasm comparison of cached/uncached binaries
found median cached costs 7.94% higher for arithmetic, 9.93% higher for calls,
and 14.45% higher for arrays, while string conversion cost fell 55.13%.
Evidence: `build/script-vm-benchmark/strings-bracket.json`. This motivates
keeping the cache off by default; it is not a game FPS measurement. The old
benchmark binaries predate that default change. The runner now captures source
hashes and supports `--tag` for independent future output directories.

The final resource-free matrix completed **15/15 PASS**, including a separate
experimental string-cache SIMD AddressSanitizer gate. Six default/legacy VM
outputs remain identical at SHA-256
`8a044a3e9b20379b5b148d30d8cdd013f4c257ec235a8dbba3c51d45bf8b0d42`.
The final default builds completed with 55/55 compiled units and no source
changes during either build. Final-default comparisons both pass:

| Run | Matching ticks | Matching images |
| --- | ---: | ---: |
| Replay01 stages 1/2 | 29,577 | 6 |
| Replay01 stage-6 previously covered segment | 15,099 | 5 |

Only the original FPS rectangle is excluded. Both traces are complete for
their requested comparisons with no first state/input/RNG difference. Stage 6
still ends prematurely at 15,098 / 3,352,752,270 rather than its recorded
34,829 / 4,256,112,150. Thus this is **before/after equivalence of the covered
segment**, not complete stage-6 or original-game replay acceptance.

Final local Wasm outputs:

```text
build/web-wasm-exceptions-mobile-final/ph3-open.wasm
3751467 bytes, SHA256 f8540bafa0439a27f9c260ed8d4516f84768f574681ef1be9fc479ef67fb26c9
build/web-verify-wasm-exceptions-mobile-final/ph3-open.wasm
3790890 bytes, SHA256 43e70676344521f4ab65f6df09195bbcb56a1b679c36846380ee76ee94865f2e
```

The corrected mobile20 resource-free matrix completed **14/14 PASS**, with
six VM outputs identical, including assignment error lines and counter type
identity. Normalized output SHA-256:
`558ad96402a90fbb483f96b70231925d0a4f846e203f162bcb6d75f6dbaf740e`.
Both complete mobile20 standard/verifier SIMD builds linked with unchanged
source snapshots. This still does not establish mobile20 game equivalence or
99% performance.

Local evidence paths (all ignored/private):

```text
build/verify-prefix12-mobile17-20261006/equivalence.json
build/verify-prefix12-mobile18-simd-20261006/equivalence.json
build/verify-prefix12-mobile21-strings-20261006/equivalence.json
build/verify-stage6-mobile21-strings-20261006/equivalence.json
build/verify-prefix12-mobile-final-20261006/equivalence.json
build/verify-stage6-mobile-final-20261006/equivalence.json
build/script-vm-benchmark/report.json
build/profile-boss-baseline-20261006/cpu-summary.json
build/profile-hard14-mobile18-affinity-20261006/cpu-summary.json
build/hard14-mobile18-affinity-summary.json
build/hard14-mobile19-affinity-summary.json
build/hard14-mobile20-affinity-summary.json
```

## Correct workload identity

Earlier performance work used `Main_replay01.dat`, which is **Easy**, not
Lunatic. Replay03/05 are also Easy, Replay04 is Normal. This was verified from
the original serialized `Stg.Difficult` common data and HSoB's constants.

`Main_replay14.dat` is untouched **Hard, stage-6 Practice** (`Difficult=2`,
`PlayMode=2`, `FirstStage=StageIndex=6`). Its engine replay stage index is 1.
It is therefore selected with `--stage=1`; do not relabel it as stage-1 Hard.
Recorded endpoint is 29,131 frames. The first pressure probes cover only
through frame 10,000, not its full run or Boss completion.

## Timing infrastructure and negative evidence

The first sandboxed browser launch timed out before gameplay. The successful
local browser runs needed execution outside the restricted process sandbox.
The preview is loopback-only and game/font files remain read-only.

The host is an Intel i7-14650HX with 24 logical processors. Other background
tasks and large variation were observed. No unrelated process was modified.
Later probes use `--affinity=0xff` for only the probe-owned Chrome process tree.
The actual applied masks/PIDs are saved in each report. Treat affinity and
unrestricted runs as different measurement conditions.

Timing schema 3 adds actual callback counts, >16.667 ms counts, P99, logical
window duration and missing 60 Hz refresh-slot estimates (half-slot jitter
tolerance). These are software presentation observations, not GPU scanout.
A timer can duplicate or miss completed timing windows. The first mobile17
run lost eight Boss windows and remains timing-incomplete. Later builds publish
each completed window directly to the bounded recorder. Diagnostic fast/trace
playback retains timer-based state sampling and is excluded from timing claims.

The first fixed-affinity Hard probes both completed to frame 10,000 without
runtime/context errors and with no missing timing windows. In frames 1,000–9,999:

| Candidate | CPU callbacks within 16.667 ms | Delivered 60 Hz slots | Weighted logic Hz |
| --- | ---: | ---: | ---: |
| mobile18-simd | 78.5798% | 71.1170% | 58.0673 |
| mobile19-range | 73.4487% | 63.0360% | 56.7301 |
| mobile20-vm | 78.5714% | 66.1886% | 55.1826 |

This is a **FAIL against 99%**, not a demonstrated speedup. The separate CPU
profile is diagnostic, not comparable timing. It places script execution first
(advance self ~8,969 ms, inclusive ~21,399 ms in ~36,293 ms of frame callbacks).
The confirmed external load prevents using these noisy pairs as causal
before/after game-speed estimates. Chrome version was 154.0.8037.98, Intel UHD
through ANGLE/D3D11, viewport 1100×1000 with 1100×825 captures.

One added VM test initially used the implicit return variable name `result`
for a local declaration. The parser correctly rejected that test at line 3.
The fixture was corrected to `computed` and checks were restarted. This was
a test authoring error, not an accepted engine regression.

## Reproduction

The surviving Launcher dependency is `../../eagler-touhou/node_modules`, not
the removed `../eagler-touhou-hsob` path. The harness now discovers either or
honors `HSOB_LAUNCHER`. The read-only preview effects were compiled into this
tree's `build/effects/` with the existing shader toolchain.

```powershell
$env:EMSDK = 'D:/workspace/eagler/toolchains/emsdk'
node portable/check-wip.mjs
$env:PH3_EXTRA_FLAGS = '["-O3","-flto"]'
node portable/build-web.mjs --exceptions=wasm --label=mobile-final
node portable/build-web.mjs --variant=verify --exceptions=wasm --label=mobile-final
node portable/run-campaign-local.mjs --runtime=build/web-wasm-exceptions-mobile-final --port=8873 --stage=1 --throttle=4 --affinity=0xff --until-frame=10000 --seconds=300 --fixture=D:/workspace/eagler/fangame/games/th_hsob_ver1.12/dnh/script/th_hsob/replay/Main_replay14.dat --out=build/perf-hard14-mobile-final-next
```

Do not compile or run another heavy test during ordinary pressure measurement.
Use separate output directories, original resources and immutable old trace/
image references. Legacy A/B flags include `PH3_LEGACY_SCRIPT_LOAD_ARITHMETIC`,
`PH3_LEGACY_SCRIPT_ARITHMETIC_RESULT`, `PH3_LEGACY_GL_BINDINGS` and
`PH3_LEGACY_RENDER_RANGE`. SIMD is a build experiment, not the default or a
claimed universally compatible runtime.

## 继续优化：不是目标完成

用户追问停止依据后继续工作。撤销的是 99% 量化验收，不是手机可玩的目标。
不要把前一轮 `mobile-final` 回归账本当作当前源码已经整体验证。

### opcode22 attribution

`PH3_VM_OPCODE_PROFILE` is a diagnostic-only compile flag. The harness requires
`--vm-opcodes` to match it, supports `--vm-from-frame`, and marks these reports
timing-incomparable. It records dispatched/fused instructions, actual native
calls, scalar boxing, body-storage requests and environment reuse. The active
instrumented standalone VM outputs match the normal regression output.

Original Hard stage-6 Practice, engine index 1, frames 7,536–10,113:
`build/profile-hard14-opcodes22-20261006/report.json`. Per observed frame:

- ~2,205 environment requests, ~2,200 reused;
- ~10,290 variable loads, ~6,503 result-call dispatches;
- ~290 body-storage requests but only ~3.57 inline-scalar boxing operations;
- ~1,242 native `index_`, ~498 `GetAreaCommonData`, ~446 `not` calls.

This identifies call/parameter and builtin dispatch as larger opportunities
than additional scalar-boxing special cases. Counts are attribution, not FPS.

### parameters23

Script functions, tasks and loop blocks with a bounded exact contiguous
parameter-assignment prologue can move caller arguments directly into fresh
variable slots. Bytecode remains unchanged. Transfer consumes the caller in
the original right-to-left order; variable type identity, task order, result
slot and ownership semantics are retained. Other prologues fall back.
`PH3_LEGACY_SCRIPT_PARAMETERS` preserves the old path.

The 15-gate matrix passed with identical six-mode outputs, including new mixed
types/COW/pause, recursion/task and 67-parameter fallback cases. Prefix 1/2
29,577 ticks and six images match the immutable reference:
`build/verify-prefix12-parameters23-20261006/equivalence.json`.

Four alternating Node/Wasm pairs in
`build/script-vm-benchmark-parameters23/bracket.json` showed medians:
arithmetic 11.0403 vs 11.858575 ms, function calls 15.9524 vs 17.3744 ms
(direct vs legacy parameters), roughly 6.9% / 8.2% lower in these kernels.
Array/string kernels were near parity. These are not game speedup claims.

Ordinary parameters23, no throttling/profiler/fast/trace, original Hard14 through
frame 10,000: `build/hard14-parameters23-native-summary.json`. Frames
1,000–9,999 and 7,500–8,999 both had median per-second presented-callback P95
~6.2 ms. One ~928-bullet window measured P95 9.1 ms/max 9.4 ms. This is the
current loaded desktop, not a phone or 4× result. No timing windows were lost.

### builtins24

Original native-function identities select array-read/length/not/true/false
opcodes. Valid fast cases preserve the original returned value/type and parent
ownership. Non-real index coercions, invalid index/type cases and suspension
use the original native path. These opcodes avoid indirect calls and do not add
checks to ordinary unrelated API calls. `PH3_LEGACY_SCRIPT_BUILTINS` is the A/B
opt-out. Expanded standalone cases include child ownership, COW, empty arrays,
boolean index coercion and original error messages/lines. The full 15-gate
matrix passed; all six compared VM modes produced the same output SHA-256:
`08bd35fc6326ee1d09bb33b08861eae6ac0566091f9e5fe1bc46fc50c6408214`.
Standard/verify O3+LTO builds both linked (55/55), without SIMD or string cache.

Prefix 1/2 again matches all 29,577 ticks and six captures (zero changed pixels
outside the FPS rectangle):
`build/verify-prefix12-builtins24-20261006/equivalence.json`.
Partial stage-6 also matches all 15,099 ticks and five captures:
`build/verify-stage6-builtins24-20261006/equivalence.json`.
The pre-existing early Replay01 termination remains; this is covered-prefix
equivalence, not a complete stage-6 acceptance.

Four alternating precompiled Node/Wasm pairs in
`build/script-vm-benchmark-builtins24/bracket.json` measured array-kernel
medians 13.1463 vs 14.5646 ms (fast vs original builtins), ~9.7% lower.
Arithmetic/function kernels were near parity; the string kernel was ~2.6%
higher. This supports a bounded array-path improvement, not a whole-game
speedup claim. The sequential sweep was heavily affected by external load.

Ordinary 4× Hard14, no affinity/profiler/fast/trace, reached frame 10,000 without
runtime/context errors or missing timing windows:
`build/hard14-builtins24-4x-summary.json`. For frames 1,000–9,999, median
per-second presented-callback P95 was 16.0 ms, CPU budget compliance 95.25%,
delivered refresh slots 89.96%, weighted logical Hz 58.85. For 7,500–8,999,
the corresponding figures were 18.8 ms / 92.75% / 84.38% / 58.94 Hz.
Large late spikes (including a ~135 ms callback window) show that this is
still not stable 60 Hz. Other load remains present. Different affinity/load
conditions prevent treating the gap from older runs as a causal speedup.

### queries25

Render-object script APIs use an inherited virtual capability query instead
of repeatedly searching RTTI. The capability is independent of mutable
script-visible object type tags, preserves null/wrong-type results and the
adjusted derived pointer, and adds no per-object cache or lifetime ownership.
52 call sites use it. Native builds and `PH3_LEGACY_SCRIPT_OBJECT_CASTS` retain
RTTI. Every query in verify builds is checked against the original dynamic
cast, following the existing mesh-query differential observer pattern.
Standard and verify O3+LTO builds linked (55/55 each). Prefix 1/2 matches
29,577 ticks and six captures, with every real render query also checked
against RTTI:
`build/verify-prefix12-queries25-20261006/equivalence.json`.

The attempted longer original Hard14 playback ended at frame 17,567 / score
880,406,730 rather than recorded 29,131 / 2,505,775,630. The previous
`mobile-final` build repeats exactly the same 17,568 ticks, trace SHA-256 and
four covered captures. The comparison correctly fails full coverage because
the 25,000/28,500 captures are missing:
`build/verify-hard14-full-queries25-20261006/equivalence.json`.
This is no complete-Hard6 acceptance. User explicitly instructed
“正常 别在意replay问题”; investigation of this known replay discrepancy stops.
It is not an outstanding blocker for the authorized performance work.

### loadguards26 (iteration closed)

Reject a non-arithmetic instruction sequence before resolving its second
variable operand. The eligible arithmetic, fallback instruction order,
errors/line identity and bytecode remain unchanged. The legacy check order is
available with `PH3_LEGACY_SCRIPT_LOAD_GUARDS`.

The full 15-gate matrix passed with identical six-mode VM outputs. Four
alternating precompiled Node/Wasm pairs in
`build/script-vm-benchmark-loadguards26/bracket.json` measured medians (new vs
old checks): arithmetic 11.931125 vs 12.15585 ms, function 16.807625 vs
16.830625 ms, arrays 15.4063 vs 16.394875 ms, strings 35.005875 vs 36.1483 ms.
Array kernel is ~6.0% lower; function kernel is essentially unchanged.
These are kernel results under external load, not a claimed full-frame gain.
Latest standard/verify O3+LTO builds both linked (55/55), without SIMD or
string cache. Standard Wasm is 3,753,160 bytes, SHA-256
`0bcbf297b8d62335496ae5b6f30aaaae0db3e9eefc7bacb78c987e02153f5bc9`;
verify Wasm is 3,796,718 bytes, SHA-256
`00466132f0689a8275c255c514fca69b95f153d937aa329e3361faa9ff8a4a70`.

Ordinary 4× Hard14, same no-affinity/no-profiler settings as builtins24,
reached frame 10,000 without runtime/context errors or missing timing windows:
`build/hard14-loadguards26-4x-summary.json`. Frames 1,000–9,999 had median
per-second presented-callback P95 17.05 ms, CPU budget compliance 92.30%,
delivered refresh slots 88.42%, weighted logical Hz 59.08. For 7,500–8,999,
these were 16.5 ms / 91.80% / 85.30% / 59.85 Hz. Some windows contained
~209 ms callbacks. The run is still not stable 60 Hz. Overall median worsened
while dense median improved against builtins24; the fluctuating external load
prevents attributing either game-timing difference to the source change.

Final current-source regressions match the immutable previous references:

- Prefix 1/2: 29,577 ticks, six captures, zero changed pixels outside FPS:
  `build/verify-prefix12-loadguards26-20261006/equivalence.json`.
- Stage-6 covered prefix: 15,099 ticks, five captures, zero changed pixels
  outside FPS: `build/verify-stage6-loadguards26-20261006/equivalence.json`.

Total 44,676 covered ticks / 11 captures. All verify-build render capability
queries also matched RTTI. The standalone 15-gate matrix and whitespace check
pass. Coverage is unchanged; no original-full-replay or phone acceptance is
being claimed. Current normal runtime is
`build/web-wasm-exceptions-loadguards26`, verify runtime is
`build/web-verify-wasm-exceptions-loadguards26`.

The host still has no attached adb device. User declined connection/testing
and explicitly allowed stopping this iteration (“不用 想停停”). Current desktop
regressions are complete; preserve the measured limits and end this unit.
No commit, push, deployment or phone performance acceptance is authorized by
that reply. Remaining phone evidence is not an active blocking question.

### Meaning of 4×

Chrome's 4× slowdown is relative to this host's CPU execution, not a named
phone SoC. The i7-14650HX result is a desktop pressure test. Chrome documents
4× as a rough mid-tier mobile approximation, with architecture differences
and host dependence. It does not reproduce phone GPU/memory/thermal behavior:
https://developer.chrome.com/docs/devtools/device-mode#throttle-the-network-and-cpu
The RMX5080/WebView >=150 target still requires same-workload device evidence.

## Authorized test publication, 2026-10-06 23:00:49 SGT

User subsequently requested “更新test站试试”. Published only the two existing
ph3 test entries, `https://test.touhou.vip/hsob/` and
`https://test.touhou.vip/hsob-perf/`, to the verified normal loadguards26 runtime.
No source commit or push. Existing game data, font/package declarations,
frontend assets and user-storage identities were retained. No game/font files
were uploaded. The test site's other game control manifests were unchanged.

Runtime generation:
`84164c809dc0411c3c85d6d67bb129c1bc474573826951c0ed1826e31255c519`.
Public Wasm SHA-256 matches the tested standard build:
`0bcbf297b8d62335496ae5b6f30aaaae0db3e9eefc7bacb78c987e02153f5bc9`.
Both sites retained old immutable runtime files and atomically switched their
own `current` links. Previous releases remain `20260930-ogg-v2` (hsob-import)
and `20260930-wasm-eh` (hsob-perf). Their other test-site/current pointer was
not switched. App Shell/runtime pointers, deployment/release checksums and
provenance were updated together and verified before activation.

Evidence is under `build/deploy-test-loadguards26-20261006/`:

- `publication.json`: server publication receipts and rollback points.
- `public-verification.json`: both entries' seven runtime file hashes,
  JS/Wasm MIME, web-manifest/robots MIME, real unknown-path 404, retained old
  Wasm identities and unchanged other-game control manifests.
- `browser-verification.json`: two isolated Chrome 154 sessions each imported
  the existing local offline ZIP client-side, loaded the new generation, entered
  original Hard6 Practice and exceeded 600 frames without page/runtime/context
  errors. This is a startup window, not phone or full-run acceptance.

The first six automated startup attempts are preserved as negative probe
evidence. The original helper closed the native first-use dialog without
acknowledging the Launcher handler, then later tried to click a sliding dialog
while the pending launch was replacing it. The helper now acknowledges the
actual close-button event after the notice appears and awaits the import
window; `browser-hsob-ui7` and `browser-hsob-perf` pass. These test-harness
adjustments did not rebuild or change the published runtime.
