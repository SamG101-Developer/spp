module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_group_ast;
import spp.asts._fwd;
import spp.asts.ast;
import spp.utils.ptr_cmp;

import ankerl;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentGroupAst;
}


SPP_EXP_CLS struct spp::asts::GenericArgumentGroupAst final : virtual Ast {
    /**
     * The token that represents the left bracket @code [@endcode in the generic argument group. This introduces the
     * generic argument group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of arguments in the generic argument group. This can contain both positional and keyword arguments.
     */
    std::vector<std::unique_ptr<GenericArgumentAst>> args;

    /**
     * The token that represents the right parenthesis @code ]@endcode in the generic call argument group. This closes
     * the generic argument group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the GenericArgumentGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left bracket @code [@endcode in the generic argument group.
     * @param args The list of arguments in the generic argument group.
     * @param tok_r The token that represents the right parenthesis @code ]@endcode in the generic call argument group.
     */
    GenericArgumentGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(args) &&args,
        decltype(tok_r) &&tok_r);

    ~GenericArgumentGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    static auto new_empty()
        -> std::unique_ptr<GenericArgumentGroupAst>;

    static auto from_params(
        GenericParameterGroupAst const &generic_params)
        -> std::unique_ptr<GenericArgumentGroupAst>;

    static auto from_map(
        ankerl::unordered_dense::map<std::shared_ptr<TypeIdentifierAst>, ExpressionAst const*, spp::utils::PtrHash<std::shared_ptr<TypeIdentifierAst>>, spp::utils::PtrEq<std::shared_ptr<TypeIdentifierAst>>> &&map)
        -> std::unique_ptr<GenericArgumentGroupAst>;

    static auto from_map(
        ankerl::unordered_dense::map<std::shared_ptr<TypeIdentifierAst>, std::shared_ptr<const TypeAst>> &&map)
        -> std::unique_ptr<GenericArgumentGroupAst>;

    auto operator==(GenericArgumentGroupAst const &other) const -> bool;

    auto type_at(const char *key) const -> GenericArgumentTypeAst const*;

    auto comp_at(const char *key) const -> GenericArgumentCompAst const*;

    auto get_type_args() const -> std::vector<GenericArgumentTypeAst*>;

    auto get_comp_args() const -> std::vector<GenericArgumentCompAst*>;

    auto get_keyword_args() const -> std::vector<GenericArgumentAst*>;

    auto get_positional_args() const -> std::vector<GenericArgumentAst*>;

    auto get_all_args() const -> std::vector<GenericArgumentAst*>;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
