module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_required_ast;
import spp.asts.ast_kind;
import spp.asts.generic_parameter_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterTypeRequiredAst) {
}

SPP_EXP_CLS struct spp::asts::GenericParameterTypeRequiredAst final : GenericParameterTypeAst {
  SPP_AST_KEY_FUNCTIONS(GenericParameterTypeRequiredAst);

  /**
   * Construct the GenericParameterTypeRequiredAst with the arguments matching the members.
   * @param name The name of the generic type parameter.
   * @param constraints The optional inline constraints for the generic type parameter.
   *
   * @note This constructor just calls the GenericParameterTypeAst constructor with the same arguments, but is defined
   * for uniformity with the other parameter variants.
   */
  GenericParameterTypeRequiredAst(
    decltype(Name) name,
    decltype(Constraints) &&constraints);

  ~GenericParameterTypeRequiredAst() override;
};
