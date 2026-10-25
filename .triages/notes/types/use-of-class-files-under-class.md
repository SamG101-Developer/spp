---
name: use-of-class-files-under-class
description: "A `use` of a class must key instances, FindOrMakeClsInstance and the template stamp on the class (TypeSymbol::InstanceTemplate), or sup blocks register on the use symbol; FindTypeSymbol with args is now identity-only"
---

Fixed 2026-10-06. `use std::function::FunMut` filed `FunMut[...]` instances under the use symbol, not the class.
Spelling lookups in FindTypeSymbol found them; identity lookups didn't. That needed a guard plus a FromId workaround.

The fix that first looked complete (key the instances on the class) broke std: `Str::from(1_u32)` hit E51. The cause
was that the merged instance's `_StampedTemplateId` still named the use symbol. FindHeadSymbol follows that stamp, so
the instance read `NormalSupBlocks[use symbol]` (4 blocks) instead of the class's entry (5 blocks).
`InstanceTemplate()` (the UseTarget for a class; a type alias stays itself) now feeds all three: InstanceIdOf,
FindOrMakeClsInstance and the stamp.

**Why:** any spot that still names the use symbol for an instance silently loses sup blocks. It surfaces as E51/E62
far from the cause.
**How to apply:** a name with generic arguments is answered by identity only (NamedInstance → tmpl->Instances). Don't
reintroduce a spelling fallback; if a lookup misses, find which symbol the instance was filed or stamped under. See
[[sup-attach-fixed-point]], [[identity-renames-2026-10-06]].
