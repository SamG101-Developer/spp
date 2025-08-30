#ifndef FUNCTION_CALL_ARGUMENT_KEYWORD_AST_HPP
#define FUNCTION_CALL_ARGUMENT_KEYWORD_AST_HPP

#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The FunctionCallArgumentKeywordAst represents a keyword argument in a function call. It is forces the argument
 * to be matched by a keyword rather than an index.
 */
struct spp::asts::FunctionCallArgumentKeywordAst final : FunctionCallArgumentAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name of the keyword argument. This is the identifier that is used to refer to the argument in the function
     * call.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * The token that represents the assignment operator \c = in the keyword argument. This separates the name of the
     * argument from the expression that is being passed as the argument's value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * Construct the FunctionCallArgumentKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator \c = in the keyword argument.
     * @param conv The convention on the argument being passed into the function call.
     * @param val The expression that is being passed as the argument to the function call.
     */
    FunctionCallArgumentKeywordAst(
        decltype(name) &&name,
        decltype(tok_assign) &&tok_assign,
        decltype(conv) &&conv,
        decltype(val) &&val);

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


#endif //FUNCTION_CALL_ARGUMENT_KEYWORD_AST_HPP
