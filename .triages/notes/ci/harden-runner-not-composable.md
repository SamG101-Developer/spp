---
name: harden-runner-not-composable
description: "Don't hoist the repeated harden-runner blocks into a composite action - it declares a `pre:` entrypoint"
updated: 2026-09-18
---

The ~10 copies of the `step-security/harden-runner` step (with its identical 5-endpoint `allowed-endpoints` list) in
`.github/workflows` are the biggest remaining duplication in the pipeline, and wrapping them in a composite action is
the obvious cleanup. Verified 2026-09-18: harden-runner's `action.yml` declares `pre: dist/pre/index.js`, and that is
where the agent installs itself. Nested **post** steps do run inside a composite action (setup-toolchain relies on
`actions/cache`'s post-step save), but nested **pre** steps are not confirmed to, and a silently skipped `pre` would
leave the egress policy unenforced while every job still went green.

**Why:** the failure mode is a security control quietly doing nothing, not a red build - nothing would surface it.

**How to apply:** leave the blocks inline until someone confirms nested `pre:` steps run, or StepSecurity documents
composite use. Related: [[gha-parallel-steps-limits]].
