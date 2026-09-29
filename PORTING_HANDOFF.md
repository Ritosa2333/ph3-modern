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
| Main six-stage Replay | **RUNNING** | `build/replay-main01/` |
| Practice Replay | **UNVERIFIED** | — |
| Extra Replay | **UNVERIFIED** | — |
| Cross-browser, mobile, manual playthrough | **UNVERIFIED** | — |

Standard runtime SHA-256 (`build/web/ph3-open.wasm`):
`b7e3e5baac0da669caa2095084d420642e429e1eea20550289e81a00195f3850`.
Build/fixture hashes for every Replay run will be recorded with the final
stage table after those runs end.

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

## Replay acceptance rule

Use the explicit `boundary` build and original Replay menu. Each natural
stage-end event must match the recorded frame and score exactly. The browser
runner saves the first mismatch and stops at that stage. The three requested
fixtures are played in order: Main_replay01, Main_replay14, Main_replay02.
Any incomplete or failed fixture remains **UNVERIFIED/FAIL**, with the last
observed boundary and errors preserved. No epsilon, skipped frames, RNG,
script, or gameplay changes are allowed to make a mismatch pass.
