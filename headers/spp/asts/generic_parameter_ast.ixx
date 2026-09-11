module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_ast;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterAst) {
  SPP_EXP_CLS struct GenericParameterCompOptionalAst;
  SPP_EXP_CLS struct GenericParameterCompVariadicAst;
  SPP_EXP_CLS struct GenericParameterTypeOptionalAst;
  SPP_EXP_CLS struct GenericParameterTypeVariadicAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::asts::detail {
  SPP_EXP_CLS template <typename GenericParameterType>
  struct make_required_param {
    using type = GenericParameterType;
  };

  SPP_EXP_CLS
  template <typename GenericParameterType>
  using make_required_param_t = typename make_required_param<GenericParameterType>::type;

  SPP_EXP_CLS template <typename GenericParameterType>
  struct make_optional_param {
    using type = GenericParameterType;
  };

  SPP_EXP_CLS
  template <typename GenericParameterType>
  using make_optional_param_t = typename make_optional_param<GenericParameterType>::type;

  SPP_EXP_CLS template <typename GenericParameterType>
  struct make_variadic_param {
    using type = GenericParameterType;
  };

  SPP_EXP_CLS
  template <typename GenericParameterType>
  using make_variadic_param_t = typename make_variadic_param<GenericParameterType>::type;

  SPP_EXP_CLS
  template <typename GenericParameterType>
  struct generic_param_value_type;

  SPP_EXP_CLS
  template <typename GenericParameterType>
  using value_type_t = typename generic_param_value_type<GenericParameterType>::type;
}

/**
 * The GenericParameterAst is the base class for all generic parameters. It is inherited by the GenericParameterCompAst
 * and GenericParameterTypeAst, which represent the two types of generic parameters in the language.
 */
SPP_EXP_CLS struct spp::asts::GenericParameterAst : Ast, mixins::OrderableAst {
  /**
   * The name of the generic type parameter. This is the name that will be used to refer to the type parameter in the
   * generic type.
   */
  Shared<TypeAst> Name;

  explicit GenericParameterAst(
    Shared<TypeAst> name,
    utils::OrderableTag order_tag);

  ~GenericParameterAst() override;
};
