# ph3-open-web handoff

## Source authority and import

- Worktree branch: `portable-web-open`, based on original ph3 commit
  `5c92f28e17b6ef3721a3ac4caa9de3af3164b51e`.
- Woo commit `900aa3ebeb329187e8c2852b8ad41caa5200ca9e` supplies the pinned
  DAT reader and zlib source. Woo gameplay code is not the project authority.
- The Web overlay comes from the already tested `ph3-woo-web` candidate19 tree.
  `portable/candidate19-merge-report.json` records its file-level merge
  decisions. The only changes since that snapshot are project identity,
  explicit Replay observer build gating, documentation, and first-mismatch
  rejection in the browser acceptance harness.
- The original `ph3-original` and `ph3-woo-web` source worktrees remain separate.
  Their existing local modifications were not changed by this import.

## Current verification

| Claim | Result | Evidence |
| --- | --- | --- |
| Standard Web build | **PASS**, 55/55 compiled and linked | `build/web-objects/compile-report.json`, `build/web/build.json` |
| Platform regression | **PASS** | `node portable/test-platform.mjs` |
| Browser title to Stage 1 and controls | **PASS**, Edge on Windows | `build/open-web-smoke/report.json` |
| Normal build Replay observer isolation | **PASS**: boundary calls are compiled only with `PH3_REPLAY_BOUNDARY` or `WOO_REPLAY_VERIFY`; the standard build declares neither | `source/TouhouDanmakufu/Common/StgStageController.cpp`, `build/web/build.json` |
| Main six-stage Replay | **FAIL at Stage 2**: Stage 1 exact; Stage 2 frame exact, score mismatch | `build/replay-main01/report.json`, `samples.jsonl`, `failure.png`, `console.log` |
| Main Replay Stages 3–6 | **UNVERIFIED**; first-mismatch rule stopped the run | `build/replay-main01/report.json` |
| Practice Replay | **UNVERIFIED**; not started after Main Stage 2 failure | — |
| Extra Replay | **UNVERIFIED**; not started after Main Stage 2 failure | — |
| Cross-browser, mobile, manual playthrough | **UNVERIFIED** | — |

Standard runtime SHA-256 (`build/web/ph3-open.wasm`):
`b7e3e5baac0da669caa2095084d420642e429e1eea20550289e81a00195f3850`.
### Replay long-run acceptance: first mismatch

The explicit `boundary` variant compiled and linked 55/55. It was played in
Edge through the original Replay menu with the unmodified
`Main_replay01.dat` fixture. The runner requested all six stages, used no
per-tick trace or performance observer, and stopped at its first detected
boundary mismatch. The browser had advanced to Stage 3 frame 26 by the
runner's next one-second observation; no later stage-end was accepted or
further fixture launched.

- Boundary build: `build/web-boundary/ph3-open.wasm`, SHA-256
  `5c3fe7488e85e1c64e9c8c4d20aaab658f54eff3770d97fe65a00376119b6189`.
- Fixture: original `Main_replay01.dat`, SHA-256
  `e60d05c80d1bfc55b85711003dd864d767961bb3b106402a7974f7ed2576d4a9`.
- Gameplay authority: `ph3-original` `5c92f28e17b6ef3721a3ac4caa9de3af3164b51e`;
  DAT source: Woo `900aa3ebeb329187e8c2852b8ad41caa5200ca9e`.

| Natural boundary | Actual / recorded end frame | Actual / recorded score | Result |
| --- | ---: | ---: | --- |
| Stage 1 end | 14,485 / 14,485 | 32,756,990 / 32,756,990 | **PASS** |
| Stage 2 end | 15,090 / 15,090 | 143,369,060 / 147,368,070 | **FAIL**, score short by 3,999,010 |

The ordered boundary sequence was Stage 1 start/end, Stage 2 start/end,
then Stage 3 start. Stage 2's final samples at frames 15,011 and 15,071
both had score 143,369,060. The Stage 2 end hook recorded the same score.
Stage 3 then started at the fixture's recorded 147,368,070. That new-stage
value is not evidence that Stage 2 matched: the original replay path assigns
the recorded start score while opening a stage. The cause of the gap remains
**unverified**; it could involve stage-end timing or gameplay divergence.
No gameplay, RNG, script, score tolerance, or frame behavior was changed to
resolve it.

The Replay report recorded **zero page errors, zero console errors**, and
**72 failed requests**, all `net::ERR_ABORTED` during resource loading. The
game continued and reached Stage 2's natural end. Request URLs and reasons
remain in `build/replay-main01/report.json`; native console text is in
`build/replay-main01/console.log`. The exit code was 1 because of the exact
score mismatch. The failure screenshot and raw one-second stage samples are
preserved alongside the report.

### Browser smoke details

The automated Edge run loaded 567 original files (289,178,729 bytes), entered
the original title and menus, and reached Stage 1. Player X changed 224→140
on left input; holding Z produced real player shots and enemy shots were
observed. Escape froze native stage frame 1377→1377, resume advanced it,
X consumed a bomb 3→2, and fullscreen entered. PCM output was nonzero
(sampled peak 0.3122). The final stage frame was 1654. There were zero page,
native, and WebGL errors. One transient `net::ERR_CONNECTION_TIMED_OUT`
console error occurred during resource loading; the request was retried and
the game ran. Source request URL was not retained by this smoke harness.

### Performance observations only

No profiling or optimization was performed for this acceptance. During the
smoke, stage entry had a one-time main-thread callback of about **10.3 s**.
At the bomb checkpoint, the on-page display observer showed about **5.4 FPS**,
**32.5 logic Hz**, and **217 ms** P95 callback time; after fullscreen it showed
about **5.4 FPS**, **32.3 logic Hz**, and **225 ms** P95. These are intermittent
observations during automation, not a controlled performance benchmark. They
are recorded for later investigation and do not change Replay frame/score
acceptance criteria.

During ordinary Replay playback, the Stage 1 boundary interval was about
306.5 seconds for 14,485 frames (roughly 47 logic frames/s), and Stage 2
about 290.8 seconds for 15,090 frames (roughly 52 logic frames/s). These are
simple wall-clock observations from the boundary timestamps, not profiling.

## Replay acceptance rule

Use the explicit `boundary` build and original Replay menu. Each natural
stage-end event must match the recorded frame and score exactly. The browser
runner saves the first mismatch and stops at that stage. The three requested
fixtures are ordered Main_replay01, Main_replay14, Main_replay02, but the
latter two were deliberately not started after Main Stage 2 failed.
Any incomplete or failed fixture remains **UNVERIFIED/FAIL**, with the last
observed boundary and errors preserved. No epsilon, skipped frames, RNG,
script, or gameplay changes are allowed to make a mismatch pass.
