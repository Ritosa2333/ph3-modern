# ph3-modern — WIP

Work-in-progress modernization / browser port of Touhou Danmakufu ph3.
The original parser, script VM and game engine are retained; this is not a
replacement interpreter written only for Hollow Song of Birds (HSoB).

**Not a stable release or an all-games compatibility guarantee.** HSoB is the
current integration and regression target. Its browser shell, resource
preparation and save paths are still title-specific. Other ph3 games may need
their own entry/package adapter; ph3sx / other dialect extensions and arbitrary
DirectX effects are not promised. Replay divergences, mobile performance and
full-campaign validation remain unfinished.

Build notes: [portable/README.md](portable/README.md).
Next agent: **[AGENT_HANDOFF.md](AGENT_HANDOFF.md)**.
Historical import evidence: [PORTING_HANDOFF.md](PORTING_HANDOFF.md).

## Upstream and license

Based on [james7132/Danmakufu-ph3](https://github.com/james7132/Danmakufu-ph3),
gameplay baseline `5c92f28e17b6ef3721a3ac4caa9de3af3164b51e`.
Woo `900aa3ebeb329187e8c2852b8ad41caa5200ca9e` supplies the DAT/zlib mechanism;
import decisions are in [portable/candidate19-merge-report.json](portable/candidate19-merge-report.json).

Original code is distributed under [NYSL](LICENSE); bundled components retain
their own notices. This fork is maintained separately from the read-only
upstream and is not an official Danmakufu release. No warranty is provided.

HSoB game data, Microsoft fonts, local build outputs and private deployment
credentials are not supplied. Legacy sample files/executables inherited from
upstream are not builds of this WIP browser runtime.
