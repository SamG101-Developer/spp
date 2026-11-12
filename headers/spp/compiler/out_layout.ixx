module;
#include <spp/macros.hpp>

export module spp.compiler.out_layout;
import spp.utils.types;
import std;

namespace spp::compiler {
  /// The modes a project can be built in. Every list of them is derived from
  /// this, so a new mode is added here and nowhere else.
  SPP_EXP_CLS enum class BuildMode { DEV, REL };

  /// The types a project can be built as. Every list of them is derived from
  /// this, so a new type is added here and nowhere else.
  SPP_EXP_CLS enum class BuildType { EXE, LIB };

  SPP_EXP_CLS struct OutLayout;

  /// The name of a mode, as it is written on the command line, in a "!cfg", and
  /// as the folder under "out". For example, "BuildMode::DEV" is "dev".
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto ModeName(BuildMode mode) -> Str;

  /// The name of every mode, in the order they are declared.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto ModeNames() -> Vec<Str>;

  /// The mode a name written by the user means, in any case, or nothing if it
  /// is not the name of a mode.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto ModeFromName(
    StrView name) -> Opt<BuildMode>;

  /// The name of a type, as it is written in "spp.toml". For example,
  /// "BuildType::EXE" is "exe".
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto TypeName(BuildType type) -> Str;

  /// The name of every type, in the order they are declared.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto TypeNames() -> Vec<Str>;

  /// The type a name written by the user means, in any case, or nothing if it
  /// is not the name of a type.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto TypeFromName(
    StrView name) -> Opt<BuildType>;
}

/// Utility class for the layout that an s++ compilation writes to. This class
/// stored a few pieces of metadata, and can derive a number of paths needed for
/// different areas of the compilation. Prevents drift in path derivation.
SPP_EXP_CLS struct spp::compiler::OutLayout {
  /// The project root: the directory holding "src", "out", "vcs" and so on.
  Path Root;

  /// The normalised target triple, which names the folder each mode sits under.
  Str Target;

  /// The build mode, whose name is the folder everything below is written into.
  BuildMode Mode = BuildMode::DEV;

  /// "<root>/out" : Every target's tree, which is what a "clean" with no target
  /// named has to sweep.
  SPP_ATTR_NODISCARD auto AllTargetsRoot() const -> Path;

  /// "<root>/out/<target>/<mode>" : the root of this build's artifacts, and all
  /// a "clean" has to remove.
  SPP_ATTR_NODISCARD auto OutRoot() const -> Path;

  /// "<root>/out/<target>/<mode>/llvm" : the per-module ir tree, with the
  /// combined module and object beside.
  SPP_ATTR_NODISCARD auto LlvmRoot() const -> Path;

  /// "<root>/out/<target>/<mode>/lib" : where an ffi library is staged so it
  /// travels with the executable.
  SPP_ATTR_NODISCARD auto LibRoot() const -> Path;

  /// "<root>/out/<target>/<mode>/llvm/lto.ll" : every module linked into one,
  /// as text.
  SPP_ATTR_NODISCARD auto LtoIrFile() const -> Path;

  /// "<root>/out/<target>/<mode>/llvm/spp.o" : the object emitted from the
  /// combined module.
  SPP_ATTR_NODISCARD auto ObjectFile() const -> Path;

  /// "<root>/out/<target>/<mode>/llvm/spp-lines.o" : the program's source-line
  /// table, built from the object's debug info and linked in beside it.
  SPP_ATTR_NODISCARD auto LineTableFile() const -> Path;

  /// "<root>/out/<target>/<mode>/<name>" : the linked executable.
  SPP_ATTR_NODISCARD auto ExecutablePath() const -> Path;

  /// The name the linked executable is given: the project's own folder name, so
  /// a project in "foo/" builds "foo" ("foo.exe" for a Windows target).
  SPP_ATTR_NODISCARD auto ExecutableName() const -> Str;
};
