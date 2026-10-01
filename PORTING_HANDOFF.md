# ph3-open-web handoff

> Historical import / replay investigation record. For the merged ph3-modern
> WIP and current next steps, start with [AGENT_HANDOFF.md](AGENT_HANDOFF.md).

## Source authority and import

- Worktree branch: `portable-web-open`, based on original ph3 commit
  `5c92f28e17b6ef3721a3ac4caa9de3af3164b51e`.
- Woo commit `900aa3ebeb329187e8c2852b8ad41caa5200ca9e` supplies the pinned
  DAT reader and zlib source. Woo gameplay code is not the project authority.
- The Web overlay comes from the already tested `ph3-woo-web` candidate19 tree.
  `portable/candidate19-merge-report.json` records its file-level merge
  decisions. Subsequent changes are project identity, explicit Replay observer
  build gating, diagnostic tooling, and one portable VM numeric-format ABI fix.
  Gameplay scripts, RNG behavior, and fixture bytes are unchanged.
- The original `ph3-original` and `ph3-woo-web` source worktrees remain separate.
  Their existing local modifications were not changed by this import.

## Current verification

| Claim | Result | Evidence |
| --- | --- | --- |
| Standard Web build | **PASS**, 55/55 compiled and linked | `build/web-objects/compile-report.json`, `build/web/build.json` |
| Platform regression | **PASS** | `node portable/test-platform.mjs` |
| Browser title to Stage 1 and controls | **PASS**, Edge on Windows | `build/open-web-smoke-format-fix-controls/report.json` |
| Normal build Replay observer isolation | **PASS**: boundary calls are compiled only with `PH3_REPLAY_BOUNDARY` or `WOO_REPLAY_VERIFY`; the standard build declares neither | `source/TouhouDanmakufu/Common/StgStageController.cpp`, `build/web/build.json` |
| Main six-stage Replay | **FAIL at Stage 3**: Stages 1 and 2 exact; Stage 3 ends early after lives reach zero | `build/replay-main01-format-fix/report.json`, `samples.jsonl`, `failure.png`, `console.log` |
| Main Replay Stages 4–6 | **UNVERIFIED**; first-mismatch rule stopped at Stage 3 | `build/replay-main01-format-fix/report.json` |
| Practice Replay | **UNVERIFIED**; not started after Main Stage 3 failure | — |
| Extra Replay | **UNVERIFIED**; not started after Main Stage 3 failure | — |
| Cross-browser, mobile, manual playthrough | **UNVERIFIED** | — |

Standard runtime SHA-256 (`build/web/ph3-open.wasm`):
`e07e790fd30d7cde0355d91b93c1f77272cea87b756a1b8a592abe366b55b143`.

### Stage 2 cause and portable fix

The first Web attempt ended Stage 2 at the correct frame but short by
3,999,010 score. The original-source native diagnostic run ended both Stages
1 and 2 exactly. Read-only per-tick comparison located the first score
difference at Stage 2 frame 9,226. Both engines had the same `MAX_SCORE`, but
the Stage 2 spell base was 9,919,120 natively versus 7,347,500 on Web.
The final spell in Stage 1 similarly used the native 2.5 multiplier but the
Web default 1.0 multiplier. The formatted spell index itself was `10010` on
both engines, so the discrepancy lay in the numeric-to-string path consumed
by the script's `atoi` call.

`gstd::script_real` is `double` in the portable VM to match MSVC's binary64
`long double`. `value::as_string()` passed that `double` to `swprintf` with
`%Lf`, which expects a different vararg type on Web. The Emscripten branch
now uses `%f` for the actual `double`; native Windows formatting remains as
before. This is a platform ABI correction in
`source/GcLib/gstd/Script.cpp`, not a game-script or scoring rule change.
The corrected verify trace shows the same 11,537,500 Stage 1 final-spell base
and 9,919,120 Stage 2 spell base as native. It naturally ended Stage 1 at
14,485 / 32,756,990 and Stage 2 at 15,090 / 147,368,070, both exact.
Evidence: `build/replay/native-spell-index/trace.raw.jsonl`,
`build/replay-web-spell-index/trace.raw.jsonl`,
`build/replay-stage2-format-fix/trace.raw.jsonl` and `report.json`.

### Replay long-run acceptance: first remaining mismatch

The corrected explicit `boundary` variant compiled and linked 55/55. It was played in
Edge through the original Replay menu with the unmodified
`Main_replay01.dat` fixture. The runner requested all six stages, used no
per-tick trace or performance observer, and stopped at the first remaining
boundary mismatch in Stage 3. No later fixture was launched.

- Boundary build: `build/web-boundary/ph3-open.wasm`, SHA-256
  `6cc65f482c724deb8960aec08c63b4a340717522bf7aed56fc1598cf0165377a`.
- Fixture: original `Main_replay01.dat`, SHA-256
  `e60d05c80d1bfc55b85711003dd864d767961bb3b106402a7974f7ed2576d4a9`.
- Gameplay authority: `ph3-original` `5c92f28e17b6ef3721a3ac4caa9de3af3164b51e`;
  DAT source: Woo `900aa3ebeb329187e8c2852b8ad41caa5200ca9e`.

| Natural boundary | Actual / recorded end frame | Actual / recorded score | Result |
| --- | ---: | ---: | --- |
| Stage 1 end | 14,485 / 14,485 | 32,756,990 / 32,756,990 | **PASS** |
| Stage 2 end | 15,090 / 15,090 | 147,368,070 / 147,368,070 | **PASS** |
| Stage 3 end | 10,278 / 18,789 | 280,229,890 / 422,566,890 | **FAIL**, early Replay Finished |

The ordered boundary sequence was Stage 1 start/end, Stage 2 start/end,
Stage 3 start/end. Stage 3 started at the recorded 147,368,070 score.
One-second browser samples saw remaining lives become 3 at frame 5,552,
then 2 at 6,951, 1 at 9,642, and 0 at 10,008. The failure screenshot shows
the game's `Replay Finished` menu at Stage 3 frame 10,278. This records an
early game-over outcome; its underlying first gameplay divergence has **not**
been established. Do not infer that the Stage 3 script naturally completed.

The corrected Replay report recorded **zero page errors, zero console errors**,
and **35 failed requests**, all `net::ERR_ABORTED` during resource loading.
Request URLs, raw one-second samples, boundary events, the failure screenshot,
and browser console are preserved in `build/replay-main01-format-fix/`.
The runner exited 1 because of the Stage 3 frame/score mismatch.

### Browser smoke details

The corrected standard-build Edge run loaded 567 original files (289,178,729
bytes), entered the original title and menus, and reached Stage 1. Player X changed 224→147
on left input; holding Z produced real player shots and enemy shots were
observed. Escape froze the native stage frame, resume advanced it,
X consumed a bomb 3→2, and fullscreen entered. PCM output was nonzero
(sampled peak 0.2191). The final stage frame was 1204. There were zero page,
native, and WebGL errors. One transient `net::ERR_CONNECTION_TIMED_OUT`
console error occurred during resource loading; the request was retried and
the game ran. Source request URL was not retained by this smoke harness.
The corrected smoke record is `build/open-web-smoke-format-fix-controls/report.json`.

### Performance observations only

No profiling or optimization was performed for this acceptance. During the
corrected smoke, stage entry showed a one-time maximum callback near **2.0 s**.
At the bomb checkpoint, the on-page observer showed about **60 display FPS**,
**60 logic Hz**, and **7.7 ms** P95 callback time; after fullscreen it showed
about **60 display FPS**, **60 logic Hz**, and **5.0 ms** P95. These are
intermittent automation observations, not a controlled performance benchmark.
The earlier candidate19 smoke had much slower intermittent readings, so
performance still needs controlled investigation.

During corrected ordinary Replay playback, Stage 1 took about 269.3 seconds
for 14,485 frames, Stage 2 about 258.8 seconds for 15,090 frames, and the
partial Stage 3 about 183.0 seconds for 10,278 frames. These are simple
wall-clock observations from boundary timestamps, not profiling.

## Replay acceptance rule

Use the explicit `boundary` build and original Replay menu. Each stage-end
event must match the recorded frame and score exactly. The browser
runner saves the first mismatch and stops at that stage. The three requested
fixtures are ordered Main_replay01, Main_replay14, Main_replay02, but the
latter two were deliberately not started after Main Stage 3 failed.
Any incomplete or failed fixture remains **UNVERIFIED/FAIL**, with the last
observed boundary and errors preserved. No epsilon, skipped frames, RNG,
script, or gameplay changes are allowed to make a mismatch pass.
