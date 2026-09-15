module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_required_ast;
import spp.asts.ast_kind;
import spp.asts.generic_parameter_comp_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterCompRequiredAst);

/// A required generic "cmp" parameter on a class, function,
/// superimposition etc, like "cls MyClass[cmp n: USize] { ... }".
SPP_EXP_CLS struct spp::asts::GenericParameterCompRequiredAst final : GenericParameterCompAst {
  SPP_AST_KEY_FUNCTIONS(GenericParameterCompRequiredAst);

  GenericParameterCompRequiredAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(Name) name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type);

  ~GenericParameterCompRequiredAst() override;
};
