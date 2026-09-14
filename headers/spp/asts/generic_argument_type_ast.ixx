module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_ast;
import spp.asts.ast;
import spp.asts.generic_argument_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericArgumentTypeAst);
use(spp::asts, struct TypeAst);

/// A generic argument that accepts a type (not a compile time
/// value).
SPP_EXP_CLS struct spp::asts::GenericArgumentTypeAst : GenericArgumentAst {
  /// The value of the generic type argument, passed like
  /// "func[T]()" or "std::Vec[Str]".
  Shared<TypeAst> Val;

  explicit GenericArgumentTypeAst(
    decltype(Val) val,
    utils::OrderableTag order_tag);

  ~GenericArgumentTypeAst() override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
