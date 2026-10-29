# RMX5080 rendering continuation

User reported desktop smoothness but severe phone slowdown in the main menu
and gameplay, while Options remained smooth, even with effects disabled.
User then explicitly provided the phone over USB and authorized Via/Chrome
debugging. The earlier desktop 4× measurements are not mobile evidence.

## Named device and live build

RMX5080, Android 16. Installed Chrome and active system WebView are
154.0.8037.57. Via package is present. Chrome hardware renderer is
`ANGLE (ARM, Mali-G720 MC7, OpenGL ES 3.2)`. Device pixel ratio is 3.5;
the live canvas drawing buffer measured 361×270, with the original portable
backbuffer initialized at 640×480. No viewport/device settings were altered.

The phone loaded the actually published loadguards26 generation
`84164c809dc0411c3c85d6d67bb129c1bc474573826951c0ed1826e31255c519`.
The user's no-music setting was retained. No phone game files or saves were
uploaded or replaced. Existing local Package Store data launched successfully.

Evidence: `build/phone-rmx5080-20261006/` (directory preserves the original
session date across midnight).

## Classification

Main menu presentation was ~6–9 FPS, while measured C++ callbacks were ~3–4 ms.
The 12-second V8 diagnostic profile recorded ~11,561 ms idle. GPU timer queries
are unavailable on this context; there is no GPU-time measurement claim.
Observed API counts did not show texture feedback copies/readbacks in this
menu. This is outside-CPU submission/scheduling delay, not proof that the VM
needs a 100× optimization.

The initial API probe's buffer byte counter used the whole source heap view
(64 MiB), which overstates transfer bytes for WebGL2's five-argument overload.
The real first upload was 312 bytes from offset 42,628,152. Do not interpret
that initial counter as hundreds of GB of actual uploads.

## Reversible phone A/B evidence

The existing VBO stream allocates 32,768×52 = 1,703,936 bytes per backing store,
then submits many small sub-updates with increasing first-vertex indices.
The original supported alternative uploads exactly the current vertex array
with bufferData and draws from first vertex zero. No vertices, primitive order,
render state, simulation, inputs or resources are reduced by that alternative.

Three independent temporary renderer API experiments were restored afterward:

| Experiment | Stable presentation | Restored stream |
| --- | --- | --- |
| Exact per-draw bufferData with subarray view | ~59.94–59.98 FPS | ~6.18–6.60 FPS |
| Exact per-draw bufferData with original WebGL2 range arguments | ~59.90–60.05 FPS | ~6.13–6.32 FPS |
| Keep large stream, only change bufferSubData to a subarray view | ~6.27–6.51 FPS | ~5.95–6.78 FPS |

Transition windows are retained, not counted as stabilized A/B measurements.
During successful direct-upload windows, logical rate also recovered to ~60 Hz
and presented-callback P95 was ~2.2–2.6 ms. This isolates the upload/storage
strategy rather than the range-argument API overload. It does not identify the
exact internal Mali/ANGLE driver mechanism.

Artifacts: `vertex-upload-ab.json`, `vertex-upload-ab-direct-range.json`,
`vertex-upload-ab-subarray.json`. V8/API diagnostics remain separate from the
ordinary A/B timing. All API/RAF wrappers and probes were removed after each run.

## Formal vertex27 fix

`DxDevice.cpp` now defaults to the existing exact per-draw bufferData path.
The large stream remains explicit opt-in through `PH3_ENABLE_VERTEX_STREAM`;
`PH3_LEGACY_VERTEX_UPLOAD` continues to force direct upload. This is a generic
portable renderer change, not a phone/game-specific bypass.

Standard/verify O3+LTO `vertex27` both built and linked 55/55. The resource-free
matrix passed 15 gates. Original covered state/pixel regressions match all
44,676 ticks and 11 captures, excluding only the FPS rectangle:
`build/verify-prefix12-vertex27-20261007/equivalence.json` and
`build/verify-stage6-vertex27-20261007/equivalence.json`.

Published both existing ph3 test entries at 00:26:54 SGT on 2026-10-07.
Generation `c658ba224cd131980fd2f1938b7f4cb0ea31df81573cfb3e073fbb710ef80a40`,
Wasm SHA-256 `87e4dac5cb75874568724481d1ba248fa71a4858d285e783b904a9b555fd75c1`.
`build/deploy-test-vertex27-20261007/public-verification.json` passes public
byte hashes, MIME, retained previous generation and unrelated test-site
control identities. Source remains uncommitted/unpushed.

The user requested their phone back. They were told it could be unplugged;
do not continue phone interaction or invent formal-build phone/dense-game
acceptance from the temporary A/B result. Every temporary probe was restored
and each CDP connection disconnected. No saved game/replay files were changed.

## Display density/orientation continuation

User also raised resolution. SDL's Web window lacked the high-pixel-density
flag, so its drawing buffer used CSS pixels on a DPR-3.5 display. `display29`
adds `SDL_WINDOW_HIGH_PIXEL_DENSITY` and `SDL_WINDOW_RESIZABLE` to the existing
Web window. SDL now owns pixel-density and orientation resize handling; the
authored D3D backbuffer, game resolution settings, simulation and aspect ratio
remain separate and unchanged. The intermediate high-DPI-only `hidpi28` build
is retained as an unpublished intermediate.

Standard and verify `display29` both linked 55/55. Desktop Chrome with DPR 3.5 verified:
portrait CSS 361×270.75 → backing 1263×945 initially (SDL initial flooring),
landscape CSS 481.328125×361 → backing 1685×1264, then restored portrait
→ backing 1264×948. Runtime remained running without page errors.
Evidence: `build/display29-check/report.json`, scoped to desktop DPR/orientation
emulation, not actual phone performance. Both covered state/pixel regressions
also match the immutable reference: 44,676 ticks and 11 images, excluding only
the FPS rectangle. Evidence: `build/verify-prefix12-display29-20261007/` and
`build/verify-stage6-display29-20261007/`.

Published display29 to both existing test entries at 00:50:53 SGT on
2026-10-07. Generation
`cfaf6ef6ba7e361f655dd7e5f5e8860e4e31b8dc805fcda79240e951fceb27f9`,
Wasm SHA-256 `03f065ec11120b644229f26c655b8ed102a46be6a416f2968a856956283aab47`.
Publication and public identity/MIME/retention evidence is under
`build/deploy-test-display29-20261007/`. The preceding vertex27 release is the
immediate rollback point. Game data, fonts, storage identity and unrelated
test products remain unchanged. No source commit or push.

No additional phone interaction is required to release the user's USB device.
The phone's ordinary full-game/dense-window performance on compiled display29
is not yet accepted; the user will verify the refreshed version manually.
