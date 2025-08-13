#ifndef FUNCTION_CALL_ARGUMENT_GROUP_AST_HPP
#define FUNCTION_CALL_ARGUMENT_GROUP_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The FunctionCallArgumentGroupAst represents a group of function call arguments. It is used to group multiple
 * positional or keyword arguments together in a function call.
 */
struct spp::asts::FunctionCallArgumentGroupAst final : virtual Ast {
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

    auto get_keyword_args() const -> std::vector<FunctionCallArgumentKeywordAst*>;

    auto get_positional_args() const -> std::vector<FunctionCallArgumentPositionalAst*>;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //FUNCTION_CALL_ARGUMENT_GROUP_AST_HPP
