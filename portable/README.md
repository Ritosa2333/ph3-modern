# ph3-open-web

**WIP / ph3-modern.** The current starting point is
[`../AGENT_HANDOFF.md`](../AGENT_HANDOFF.md); the import-era handoff and dated
reports below are historical evidence, not a claim of current full acceptance.
Use `EMSDK` to point to an installed SDK outside the original workspace.
The parser/VM and platform regression commands require no HSoB resources.
Browser play/profiling commands below still target the HSoB adapter and need
the locally installed game and font; they are not a generic game installer.

Resource-free checkpoint (Node, Python and Emscripten are required):

```powershell
node portable/check-wip.mjs
```

This runs the optimized/legacy VM matrix, both exception modes, AddressSanitizer,
platform/container regressions and observer tests. It writes a fail-closed
report to ignored `build/check-wip/report.json`; it does not certify gameplay
compatibility, native desktop builds, full-campaign completion or phone FPS.

This worktree preserves the original ph3 gameplay source at
`5c92f28e17b6ef3721a3ac4caa9de3af3164b51e`. The archive reader and
bundled zlib come from Woo commit `900aa3ebeb329187e8c2852b8ad41caa5200ca9e`.
Web platform changes were imported from the tested `candidate19` overlay. The
original/Woo merge choices are recorded in `candidate19-merge-report.json`.

## Local build and play

```powershell
python portable/compile-effects.py
node portable/build-web.mjs
python portable/serve-local.py --port 8798
```

Open `http://127.0.0.1:8798/`, load the locally installed game resources, then
start the game. The server binds only to loopback and reads the installed game
and local Japanese font. Private resources and build outputs stay under ignored
`build/`; neither is part of the source commit.

The standard build compiles all 55 executor/platform translation units listed
by `DnhExecutor.vcxproj` and emits `build/web/ph3-open.wasm` plus `build.json`.
It has no Replay boundary or per-tick trace observer. Diagnostic builds require
an explicit variant:

```powershell
node portable/build-web.mjs --variant=boundary
node portable/build-web.mjs --variant=verify
```

`boundary` emits only stage lifecycle records needed for ordinary long Replay
acceptance. `verify` also compiles the read-only fixed-tick trace observer, for
divergence investigation. Serve either with
`python portable/serve-local.py --runtime build/web-boundary --port 8799` or
`--runtime build/web-verify` as appropriate. Do not report verifier timing as
production performance.

## Acceptance

The native Wasm exception candidate retains C++ exception handling while avoiding
JavaScript exception trampolines on indirect calls. It requires browser support
for Wasm exception handling. Build and validate it separately; the default JS
exception build remains available for compatibility:

```powershell
node portable/build-web.mjs --exceptions=wasm
node portable/test-platform.mjs --exceptions=wasm
python portable/serve-local.py --runtime build/web-wasm-exceptions --port 8852
```

Both exception modes run the indirect-throw, rethrow and RAII-unwind regression.
Native exception outputs have separate directories and truthful build metadata;
they do not overwrite the default JS exception build.

`node portable/test-platform.mjs` checks the portable ABI, encoding, and file
path behavior. `portable/browser-smoke.py` exercises the real browser UI and
controls. `portable/replay/run-browser.py` plays an unmodified Replay through
the original menus and stops at the first natural stage-end frame or score
mismatch. The current results and unverified scope are in `PORTING_HANDOFF.md`.

`portable/build-open-candidate.py` is retained as historical candidate19
construction evidence. It is not the production build entry point.

The current mobile performance worktree, optimization scope, replay/image
evidence and remaining limitations are recorded in
[`MOBILE_PERFORMANCE_20261001.md`](MOBILE_PERFORMANCE_20261001.md).
