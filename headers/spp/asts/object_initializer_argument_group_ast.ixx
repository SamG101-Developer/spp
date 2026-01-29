module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_group_ast;
import spp.asts.ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ObjectInitializerArgumentAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentKeywordAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentShorthandAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentGroupAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The ObjectInitializerArgumentGroupAst represents a group of object initializer arguments. It is used to group
 * multiple shorthand or keyword arguments together in a object initializer.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentGroupAst final : virtual Ast {
    /**
     * The token that represents the left parenthesis @code (@endcode in the object initializer argument group. This
     * introduces the object initializer argument group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of arguments in the object initializer argument group. This can contain both positional and keyword
     * arguments.
     */
    std::vector<std::unique_ptr<ObjectInitializerArgumentAst>> args;

    /**
     * The token that represents the right parenthesis @code )@endcode in the object initializer argument group. This
     * closes the object initializer argument group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the ObjectInitializerArgumentGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left parenthesis @code (@endcode in the object initializer argument
     * group.
     * @param args The list of arguments in the object initializer argument group.
     * @param tok_r The token that represents the right parenthesis @code )@endcode in the object initializer argument
     * group.
     */
    ObjectInitializerArgumentGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(args) &&args,
        decltype(tok_r) &&tok_r);

    ~ObjectInitializerArgumentGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    static auto new_empty() -> std::unique_ptr<ObjectInitializerArgumentGroupAst>;

    auto get_all_args() -> std::vector<ObjectInitializerArgumentAst*>;

    auto get_autofill_arg() -> ObjectInitializerArgumentShorthandAst*;

    auto get_non_autofill_args() -> std::vector<ObjectInitializerArgumentAst*>;

    auto get_shorthand_args() -> std::vector<ObjectInitializerArgumentShorthandAst*>;

    auto get_keyword_args() -> std::vector<ObjectInitializerArgumentKeywordAst*>;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
