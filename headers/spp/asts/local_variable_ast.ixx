module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct LocalVariableAst;
}

SPP_EXP_CLS struct spp::asts::LocalVariableAst : Ast {
  SPP_GCC_VTABLE_FIX

  LocalVariableAst();

  ~LocalVariableAst() override;

  SPP_ATTR_NODISCARD virtual auto ExtractName() const -> Shared<IdentifierAst>;

  SPP_ATTR_NODISCARD virtual auto ExtractNames() const -> Vec<Shared<IdentifierAst>>;

  /**
   * Whether this pattern takes a value out of what it is matched against, rather than only testing it. A name binds
   * what it stands for unless it asks for it through a borrow, which leaves the value where it was; a literal, an
   * expression, a skip and an "else" all only look. A destructure binds if any of its elements does, so one made only
   * of skips is a shape test and takes nothing.
   * @return Whether it takes anything.
   */
  SPP_ATTR_NODISCARD virtual auto BindsByMove() const -> bool;

  /**
   * Whether this element binds the rest of what is being destructured into a name of its own - the @c other of
   * @c {let (f, ..other) = a} - which takes everything the named elements did not, leaving nothing unaccounted for.
   * @return Whether it binds the rest.
   */
  SPP_ATTR_NODISCARD virtual auto TakesRest() const -> bool;

  auto MarkFromCasePattern() -> void;

protected:
  bool _FromCasePattern;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::LocalVariableAst)
