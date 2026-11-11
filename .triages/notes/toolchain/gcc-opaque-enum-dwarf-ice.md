---
name: gcc-opaque-enum-dwarf-ice
description: "An enum opaque-declared in a module that doesn't define it: GCC ICEs in gen_enumeration_type_die under -g, MSVC mangles it attached to the declarer (undefined symbol at link); import the definer"
updated: 2026-11-01
---

An opaque enum declaration (`use(ns, enum class E : std::uint8_t);`) in one module, with the full definition in another, makes GCC ICE with `-g` (`internal compiler error: in gen_enumeration_type_die, at dwarf2out.cc`, assert `!ENUM_IS_OPAQUE (type)`) in any TU that imports both and needs a debug DIE for the enum (for example, it takes the enum by value). The merged type keeps the opaque flag but has enumerators. Release builds (no `-g`) never reach that code, so only Debug builds (CodeQL, local Debug) fail. Reproduced on Homebrew GCC 16.2 and GCC 17 trunk (2026-10-18). It was first hit in `annotation_ast.cpp` (`BuiltinAnnotation`) and fixed by importing `spp.analyse.utils.annotation_utils` in `annotation_ast.ixx`.

MSVC 19.51 breaks on the same shape at link time, in Release. Despite `extern "C++"`, it mangles the enum as attached to the declaring module (`W4CompilerStage@$$_Aspp.compiler.compiler_boot@@...`), so a function taking the enum by value, called from that module and defined in another, is an undefined symbol (2026-11-01, `CompilerStage` in `compiler_boot.ixx`, `StartInstantiatingOnRead`). GCC and clang build it fine in Release.

A self-declaration in the defining module (opaque decl, then `enum class ns::E : T { ... }`) is fine and required: a qualified-name definition needs a prior declaration.

**Why:** the CI matrix builds Release, so the GCC ICE only showed up in CodeQL's Debug build, and the MSVC error only on the Windows MSVC job.

**How to apply:** don't opaque-declare an enum in a module other than the one that defines it. Import the defining module (check that it doesn't create a cycle). For an ICE in `dwarf2out.cc` during `lazy_load_binding`, or an MSVC undefined symbol with `$$_A<module>` inside an enum's name, look for a cross-module opaque enum first.
