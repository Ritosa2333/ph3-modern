# ph3-modern agent entry

Read [AGENT_HANDOFF.md](AGENT_HANDOFF.md) first. This is a WIP source checkpoint,
not a stable all-games engine or a claim that mobile Boss slowdown is solved.

Inspect `git status --short`, the branch and remotes before editing. Preserve
unrelated work, old evidence and upstream history. Do not reset/clean, apply
old stashes, upload game data/fonts/credentials, or deploy without authorization.

The original ph3 parser/VM and gameplay are the authority. HSoB is the current
browser adapter/test target, not a license to specialize language semantics,
skip bullets, alter RNG/input/collisions or weaken validation for a green test.

Run `node portable/check-wip.mjs` for resource-free regression gates. Browser
and full-campaign checks are separate and require private fixtures. Preserve
negative and incomplete results. Verifier/accelerated playback timing is not
ordinary runtime or physical-phone performance.

In the original Eagler workspace, read the relevant primary playbook under
`eagler-touhou/docs/playbooks/` before source changes. See the handoff for code
anchors, exact reproduction commands, historical build paths and dependencies.

Keep the handoff current after a completed unit of work. GitHub source upload
is not permission to update the Launcher, test site, production or game assets.
