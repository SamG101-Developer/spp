module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_group_ast;
import spp.asts.ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionCallArgumentKeywordAst;
    SPP_EXP_CLS struct FunctionCallArgumentPositionalAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The FunctionCallArgumentGroupAst represents a group of function call arguments. It is used to group multiple
 * positional or keyword arguments together in a function call.
 */
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentGroupAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the left parenthesis @code (@endcode in the function call argument group. This
     * introduces the function call argument group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of arguments in the function call argument group. This can contain both positional and keyword
     * arguments.
     */
    std::vector<std::unique_ptr<FunctionCallArgumentAst>> args;

    /**
     * The token that represents the right parenthesis @code )@endcode in the function call argument group. This closes
     * the function call argument group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the FunctionCallArgumentGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left parenthesis @code (@endcode in the function call argument group.
     * @param args The list of arguments in the function call argument group.
     * @param tok_r The token that represents the right parenthesis @code )@endcode in the function call argument group.
     */
    FunctionCallArgumentGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(args) &&args,
        decltype(tok_r) &&tok_r);

    ~FunctionCallArgumentGroupAst() override;

    static auto new_empty() -> std::unique_ptr<FunctionCallArgumentGroupAst>;

    SPP_ATTR_NODISCARD auto get_all_args() const -> std::vector<FunctionCallArgumentAst*>;

    SPP_ATTR_NODISCARD auto get_keyword_args() const -> std::vector<FunctionCallArgumentKeywordAst*>;

    SPP_ATTR_NODISCARD auto get_positional_args() const -> std::vector<FunctionCallArgumentPositionalAst*>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
