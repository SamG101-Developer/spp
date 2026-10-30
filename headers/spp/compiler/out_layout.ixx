module;
#include <spp/macros.hpp>

export module spp.compiler.out_layout;
import spp.utils.types;
import std;

namespace spp::compiler {
  SPP_EXP_CLS struct OutLayout;
}

/// Utility class for the layout that an s++ compilation writes to. This class
/// stored a few pieces of metadata, and can derive a number of paths needed for
/// different areas of the compilation. Prevents drift in path derivation.
SPP_EXP_CLS struct spp::compiler::OutLayout {
  /// The project root: the directory holding "src", "out", "vcs" and so on.
  Path Root;

  /// The normalised target triple, which names the folder each mode sits under.
  Str Target;

  /// The build mode, "dev" or "rel", which names the folder everything below is
  /// written into.
  Str Mode;

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

  /// "<root>/out/<target>/<mode>/<name>" : the linked executable.
  SPP_ATTR_NODISCARD auto ExecutablePath() const -> Path;

  /// The name the linked executable is given: the project's own folder name, so
  /// a project in "foo/" builds "foo".
  SPP_ATTR_NODISCARD auto ExecutableName() const -> Str;

  /// The modes a project can be built in, which is what "clean --mode all" has
  /// to sweep.
  SPP_ATTR_NODISCARD static auto AllModes() -> Vec<Str>;
};
