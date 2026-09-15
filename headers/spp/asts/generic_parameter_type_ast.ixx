module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterTypeAst);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct GenericParameterTypeInlineConstraintsAst);
use(spp::asts, struct TypeAst);

namespace spp::asts::detail {
  template <>
  struct make_required_param<GenericParameterTypeAst> {
    using type = GenericParameterTypeAst;
  };

  template <>
  struct generic_param_value_type<GenericParameterTypeAst> {
    using type = Shared<TypeAst>;
  };
}

SPP_EXP_CLS struct spp::asts::GenericParameterTypeAst : GenericParameterAst {
  /// The optional inline constraints for the generic type
  /// parameter. In "fun func[T: Copy]()", "T" is the generic
  /// type parameter and "Copy" is the constraint.
  Unique<GenericParameterTypeInlineConstraintsAst> Constraints;

  GenericParameterTypeAst(
    decltype(Name) name,
    decltype(Constraints) &&constraints,
    utils::OrderableTag order_tag);

  ~GenericParameterTypeAst() override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  SPP_ATTR_NODISCARD auto GetDummyScopes() const -> std::span<Scope* const>;

  static auto ClearDummyScopes() -> void;

private:
  inline static Vec<Unique<Ast>> _DummyScopeAsts = {};
  Vec<Scope*> _DummyScopes;
};
