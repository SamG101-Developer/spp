---
name: cmake-import-std-two-std-cc-entries
description: "compile_commands.json lists bits/std.cc twice: the @cmake_cxx_std object with no -std (gnu++20) and the imported synth BMI with gnu++26; an IDE on the first greys out C++23 exports"
updated: 2026-10-30
---

With CMake 4.4's `import std`, `compile_commands.json` has two entries for `bits/std.cc`. `@cmake_cxx_std` (the object
in `lib@cmake_cxx_std.a`) has no `-std=`, so GCC's default gnu++20 applies. `@cmake_cxx_std@synth_0` (the `.bmi`) has
`-std=gnu++26`, and that BMI is what the `spp_lib` modmaps import.

`__cpp_lib_*` macros for C++23 and later are undefined under the first entry, so guards such as
`#if __cpp_lib_unreachable` in `std.cc` are false there and true in the BMI.

**Why:** On GCC 17 this made `using std::unreachable` look unexported in CLion while the build compiled fine. That CLion
reads the first entry fits the symptom but was not confirmed from CLion itself.

**How to apply:** Do not judge what `import std` exports from the IDE's view of `std.cc`; evaluate the guard with the
`synth_0` entry's flags, or check the modmap to see which BMI a target imports.
