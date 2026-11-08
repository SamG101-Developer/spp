---
name: tickets-one-folder-each
description: "Upstream bug tickets live in .triages/tickets/<slug>/ (README.md + repro files), one folder per ticket"
updated: 2026-11-08
---

A ticket drafted for an upstream project (a compiler or tool bug) goes in its own folder,
`.triages/tickets/<short-kebab-slug>/`, holding `README.md` (the report as it would be filed: environment,
description, files, expected, actual, workaround, and a status line saying whether it is filed and where) and any
repro files and build scripts next to it.

**Why:** a repro often needs several files (a module interface, an importer, a build script per compiler), and the
maintainer asked for this layout on 2026-11-08. The older flat files (the qemu `.md` and `.patch` files) predate it.

**How to apply:** new tickets get a folder. Write the README and repro as the upstream report itself: no S++ names, files, errors or workarounds, only the reduced repro's. Once filed, put the upstream URL in the README's status line and in the
`.triages/upstream-watch.md` entry that tracks it.
