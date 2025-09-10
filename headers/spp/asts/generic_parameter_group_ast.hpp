#pragma once
#include <spp/asts/ast.hpp>


struct spp::asts::GenericParameterGroupAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

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

    auto get_required_params() const -> std::vector<GenericParameterAst*>;

    auto get_optional_params() const -> std::vector<GenericParameterAst*>;

    auto get_variadic_param() const -> GenericParameterAst*;

    auto get_comp_params() const -> std::vector<GenericParameterCompAst*>;

    auto get_type_params() const -> std::vector<GenericParameterTypeAst*>;

    auto get_all_params() const -> std::vector<GenericParameterAst*>;

    auto opt_to_req() const -> std::unique_ptr<GenericParameterGroupAst>;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
