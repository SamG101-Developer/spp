module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeInlineConstraintsAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::GenericParameterTypeInlineConstraintsAst final : Ast {
    /**
     * The @code :@endcode token that introduces the inline constraints.
     */
    Unique<TokenAst> TokColon;

    /**
     * The constraints for the generic type parameter. Any generic argument passed into the generic parameter must
     * satisfy these constraints.
     */
    Vec<Unique<TypeAst>> Constraints;

    static auto NewEmpty() -> Unique<GenericParameterTypeInlineConstraintsAst>;

    /**
     * Construct the GenericParameterTypeInlineConstraintsAst with the arguments matching the members.
     * @param tok_colon The @c : token that introduces the inline constraints.
     * @param constraints The constraints for the generic type parameter.
     */
    GenericParameterTypeInlineConstraintsAst(
        decltype(TokColon) &&tok_colon,
        Vec<Unique<TypeAst>> &&constraints);

    ~GenericParameterTypeInlineConstraintsAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    SPP_ATTR_NODISCARD auto GetAllConstraints() const -> Vec<TypeAst*>;
};
