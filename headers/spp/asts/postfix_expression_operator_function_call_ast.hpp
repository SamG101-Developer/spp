#pragma once
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/postfix_expression_operator_ast.hpp>


struct spp::asts::PostfixExpressionOperatorFunctionCallAst final : PostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;
    friend struct UnaryExpressionOperatorAsyncAst;

private:
    std::optional<std::tuple<analyse::scopes::Scope const*, FunctionPrototypeAst*, std::vector<GenericArgumentAst*>>> m_overload_info;
    Ast *m_is_async;
    std::vector<FunctionCallArgumentAst*> m_folded_args;
    std::unique_ptr<FunctionCallArgumentGroupAst> m_closure_dummy_arg_group;
    std::unique_ptr<FunctionCallArgumentPositionalAst> m_closure_dummy_arg;
    std::unique_ptr<FunctionPrototypeAst> m_closure_dummy_proto;
    bool m_is_coro_and_auto_resume;

public:
    /**
     * The generic argument group that contains the generic arguments for the function call.
     */
    std::unique_ptr<GenericArgumentGroupAst> generic_arg_group;

    /**
     * The function call argument group that contains the arguments for the function call.
     */
    std::unique_ptr<FunctionCallArgumentGroupAst> arg_group;

    /**
     * The optional @c .. fold token that indicates a fold operation in a function call. This will fold all tuples in
     * the argument group into a single argument and call the function multiples times with each element of the tuple as
     * the argument for the non-tuple parameters it has mapped to.
     */
    std::unique_ptr<FoldExpressionAst> fold;

    /**
     * Construct the PostfixExpressionOperatorFunctionCallAst with the arguments matching the members.
     * @param[in] generic_arg_group The generic argument group that contains the generic arguments for the function
     * call.
     * @param[in] arg_group The function call argument group that contains the arguments for the function call.
     * @param[in] fold The optional @c .. fold token that indicates a fold operation in a function call.
     */
    explicit PostfixExpressionOperatorFunctionCallAst(
        decltype(generic_arg_group) &&generic_arg_group,
        decltype(arg_group) &&arg_group,
        decltype(fold) &&fold);

    ~PostfixExpressionOperatorFunctionCallAst() override;

    auto determine_overload(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
