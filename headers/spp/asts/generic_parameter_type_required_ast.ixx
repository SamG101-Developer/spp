module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_required_ast;
import spp.asts.ast_kind;
import spp.asts.generic_parameter_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterTypeRequiredAst);

SPP_EXP_CLS struct spp::asts::GenericParameterTypeRequiredAst final : GenericParameterTypeAst {
  SPP_AST_KEY_FUNCTIONS(GenericParameterTypeRequiredAst);

  GenericParameterTypeRequiredAst(
    decltype(Name) name,
    decltype(Constraints) &&constraints);

  ~GenericParameterTypeRequiredAst() override;
};
