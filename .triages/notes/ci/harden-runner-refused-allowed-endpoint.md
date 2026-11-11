---
name: harden-runner-refused-allowed-endpoint
description: "A one-off: Detect Code Changes' checkout was refused github.com:443 (allowed) in ~5 ms under egress block; every job skipped and the run showed cancelled 10 min later"
updated: 2026-11-11
---

Run 42947833085 attempt 1 (2026-11-11 02:42): in `Detect Code Changes` (`pr.yaml`), `actions/checkout` failed three
times with "Failed to connect to github.com port 443 after 5 ms: Couldn't connect to server", although `github.com:443`
is in that job's `allowed-endpoints` and Doxygen and pre-commit, also running Harden Runner, checked out fine at the
same time. A refusal in a few milliseconds is a local firewall reject, not the network, so Harden Runner's block rules
did not allow github.com's address on that one runner. The steps ended at 02:43:46, but the job is recorded as
`cancelled` at 02:53:00, about 10 minutes later: it seemingly never reported its result. Every job needs that job's
outputs, so all of them show `skipped`, and `CI Success` fails. No one cancelled it, and the concurrency group did not
either: the next attempt was a manual re-run at 09:50.

**Why:** a run where only `Detect Code Changes` is red and everything else is skipped looks like a cancellation or a
workflow bug.

**How to apply:** check that job's checkout step for an instant "Couldn't connect" before changing anything, and
re-run it. If it recurs, consider `egress-policy: audit` for this job only (it runs no third-party code), not a wider
allow-list. See [[harden-runner-not-composable]].
