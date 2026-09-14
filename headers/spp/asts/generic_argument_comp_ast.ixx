module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericArgumentCompAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentCompKeywordAst);
use(spp::asts, struct GenericArgumentCompPositionalAst);

/// A generic argument that accepts a compile time value (not a
/// type). Any type is allowed, as any type can be represented
/// at compile time.
SPP_EXP_CLS struct spp::asts::GenericArgumentCompAst : GenericArgumentAst {
  /// The value of the generic comp argument, passed like
  /// "func[123]()" or "std::Arr[Str, 100_uz]".
  Unique<ExpressionAst> Val;

  explicit GenericArgumentCompAst(
    decltype(Val) &&val,
    utils::OrderableTag order_tag);

  ~GenericArgumentCompAst() override;
};
