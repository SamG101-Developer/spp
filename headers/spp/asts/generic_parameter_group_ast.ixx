module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_group_ast;
import spp.asts.ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterGroupAst final : virtual Ast {
    /**
     * The token that represents the left bracket @code [@endcode in the generic parameter group. This introduces the
     * generic parameter group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of parameters in the generic parameter group. This can contain both required and optional parameters.
     */
    std::vector<std::unique_ptr<GenericParameterAst>> params;

    /**
     * The token that represents the right bracket @code ]@endcode in the generic parameter group. This closes the
     * generic parameter group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the GenericParameterGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left bracket @code [@endcode in the generic parameter group.
     * @param params The list of parameters in the generic parameter group.
     * @param tok_r The token that represents the right bracket @code ]@endcode in the generic parameter group.
     */
    GenericParameterGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(params) &&params,
        decltype(tok_r) &&tok_r);

    ~GenericParameterGroupAst() override;

    auto operator+(GenericParameterGroupAst const &other) const -> std::unique_ptr<GenericParameterGroupAst>;

    auto operator +=(GenericParameterGroupAst const &other) -> GenericParameterGroupAst&;

    SPP_AST_KEY_FUNCTIONS;

    static auto new_empty() -> std::unique_ptr<GenericParameterGroupAst>;

    auto get_required_params() const -> std::vector<GenericParameterAst*>;

    auto get_optional_params() const -> std::vector<GenericParameterAst*>;

    auto get_variadic_param() const -> GenericParameterAst*;

    auto get_comp_params() const -> std::vector<GenericParameterCompAst*>;

    auto get_type_params() const -> std::vector<GenericParameterTypeAst*>;

    auto get_all_params() const -> std::vector<GenericParameterAst*>;

    auto opt_to_req() const -> std::unique_ptr<GenericParameterGroupAst>;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
