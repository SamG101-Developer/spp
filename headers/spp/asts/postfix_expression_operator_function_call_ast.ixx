module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionCallArgumentPositionalAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FoldExpressionAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorAsyncAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorFunctionCallAst final : PostfixExpressionOperatorAst {
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

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

    auto mark_as_async(Ast *async_token) -> void;

    SPP_ATTR_NODISCARD auto target() const -> FunctionPrototypeAst*;

    auto set_closure_dummy_proto(std::unique_ptr<FunctionPrototypeAst> &&proto) -> void;

    auto set_transformed_ast(std::unique_ptr<PostfixExpressionAst> &&ast) -> void;

    SPP_ATTR_NODISCARD auto transformed_ast() const -> PostfixExpressionAst*;

private:
    struct OverloadInfo {
        analyse::scopes::Scope const *scope;
        FunctionPrototypeAst *proto;
        std::vector<GenericArgumentAst*> generic_args;
    };

    std::optional<OverloadInfo> m_overload_info;

    std::unique_ptr<PostfixExpressionAst> m_transformed_ast;

    std::unique_ptr<FunctionCallArgumentGroupAst> m_closure_dummy_arg_group;

    std::unique_ptr<FunctionCallArgumentPositionalAst> m_closure_dummy_arg;

    std::unique_ptr<FunctionPrototypeAst> m_closure_dummy_proto;

    std::vector<std::unique_ptr<PostfixExpressionOperatorFunctionCallAst>> m_folded_asts;

    Ast *m_is_async;

    bool m_is_coro_and_auto_resume;

    std::shared_ptr<IdentifierAst> m_self_comptime;

    auto m_handle_function_folding(
        ScopeManager *sm,
        CompilerMetaData *meta)
        -> std::vector<std::unique_ptr<PostfixExpressionOperatorFunctionCallAst>>;
};
