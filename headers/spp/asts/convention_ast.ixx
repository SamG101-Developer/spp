module;
#include <spp/macros.hpp>

export module spp.asts.convention_ast;
import spp.asts.ast;

SPP_AST_COMMON_FWD_DECL(ConventionAst);

namespace spp::asts {
  SPP_EXP_CLS enum class ConventionTag { MOV, MUT, REF };
}

/// A convention for a function parameter, function argument, or
/// a value generated from a coroutine. The second class model
/// restricts where borrows can be used, and the convention shows
/// that a value is borrowed (and how), or moved (no borrow).
///
/// MOV is defined as a tag because when a convention is not
/// present but needs to be compared, it is semantically
/// equivalent to a "move" convention.
SPP_EXP_CLS struct spp::asts::ConventionAst : Ast {
private:
  ConventionTag _Tag;

public:
  explicit ConventionAst(ConventionTag tag);

  ~ConventionAst() override;

  auto operator==(ConventionAst const *that) const -> bool;

  auto operator==(ConventionTag that_tag) const -> bool;

  SPP_ATTR_NODISCARD auto Tag() const -> ConventionTag { return _Tag; }
};
