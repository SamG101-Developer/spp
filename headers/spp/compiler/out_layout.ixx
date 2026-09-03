module;
#include <spp/macros.hpp>

export module spp.compiler.out_layout;
import spp.utils.types;
import std;

namespace spp::compiler {
  SPP_EXP_CLS struct OutLayout;
}

/**
 * Every path a build writes to, derived from the project root and the mode it was built in. This is the only place
 * that knows the shape of the "out" tree: the module tree, the boot's code generation and link steps, and the cli all
 * ask here rather than spelling the layout out again, so the four of them cannot drift apart.
 */
SPP_EXP_CLS struct spp::compiler::OutLayout {
  /** The project root: the directory holding "src", "out", "vcs" and so on. */
  std::filesystem::path Root;

  /** The normalised target triple, which names the folder each mode sits under. */
  Str Target;

  /** The build mode, "dev" or "rel", which names the folder everything below is written into. */
  Str Mode;

  /** @c \<root\>/out : every target's tree, which is what a @c clean with no target named has to sweep. */
  SPP_ATTR_NODISCARD auto AllTargetsRoot() const
    -> std::filesystem::path;

  /** @c \<root\>/out/\<target\>/\<mode\> : the root of this build's artifacts, and all a @c clean has to remove. */
  SPP_ATTR_NODISCARD auto OutRoot() const
    -> std::filesystem::path;

  /** @c \<root\>/out/\<target\>/\<mode\>/llvm : the per-module ir tree, with the combined module and object beside. */
  SPP_ATTR_NODISCARD auto LlvmRoot() const
    -> std::filesystem::path;

  /** @c \<root\>/out/\<target\>/\<mode\>/lib : where an ffi library is staged so it travels with the executable. */
  SPP_ATTR_NODISCARD auto LibRoot() const
    -> std::filesystem::path;

  /** @c \<root\>/out/\<target\>/\<mode\>/llvm/lto.ll : every module linked into one, as text. */
  SPP_ATTR_NODISCARD auto LtoIrFile() const
    -> std::filesystem::path;

  /** @c \<root\>/out/\<target\>/\<mode\>/llvm/spp.o : the object emitted from the combined module. */
  SPP_ATTR_NODISCARD auto ObjectFile() const
    -> std::filesystem::path;

  /** @c \<root\>/out/\<target\>/\<mode\>/\<name\> : the linked executable. */
  SPP_ATTR_NODISCARD auto ExecutablePath() const
    -> std::filesystem::path;

  /**
   * The name the linked executable is given: the project's own folder name, so a project in "foo/" builds "foo".
   */
  SPP_ATTR_NODISCARD auto ExecutableName() const
    -> Str;

  /** The modes a project can be built in, which is what @c "clean --mode all" has to sweep. */
  SPP_ATTR_NODISCARD static auto AllModes()
    -> Vec<Str>;
};
