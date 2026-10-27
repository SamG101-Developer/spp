---
name: markers-only-from-own-package
description: "Copy, Drop, FwdRef and FwdMut may only be superimposed by the package declaring the type (E111); a blanket sup [T] T ext <marker> is refused"
updated: 2026-10-25
---

Only the package that declares a type may superimpose a marker over it: `Copy`, `Drop` (how every value of the type is
copied or destroyed) and, since N7.4, `FwdRef` / `FwdMut` (which methods a call on it finds, through forwarding). From
any other package it is `SppSuperimpositionExternalMarkerExtensionError` (E111). A blanket `sup [T] T ext Copy` names
no type, so no package owns what it marks, and is refused too. The check is in
`SupPrototypeExtensionAst::Stage5_LoadSupScopes`, through `marker_sups::IsCopyMarker` / `IsDropMarker` /
`IsFwdMarker`; `$` types' blocks are not checked.

**Why:** a marker superimposed from outside would silently change the analysis of code that cannot see it, the
declaring package's own included, which is analysed as if it were absent.

**How to apply:** std's forwarding (`Vec`, `Arr`, `Str` to `View` / `StrView`) is inside std, so it is fine; a test or
user project forwarding a type it declares itself (`Wrapper ext FwdRef[Inner]`) is fine. To give a std type a marker
from outside, wrap it in a type the package owns. A new marker that changes how a type is treated belongs in this
check.
