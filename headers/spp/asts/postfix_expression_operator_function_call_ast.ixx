module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
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

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorFunctionCallAst final : PostfixExpressionOperatorAst {
    /**
     * The generic argument group that contains the generic arguments for the function call.
     */
    Unique<GenericArgumentGroupAst> GnArgGroup;

    /**
     * The function call argument group that contains the arguments for the function call.
     */
    Unique<FunctionCallArgumentGroupAst> FnArgGroup;

    /**
     * The optional @c .. fold token that indicates a fold operation in a function call. This will fold all tuples in
     * the argument group into a single argument and call the function multiples times with each element of the tuple as
     * the argument for the non-tuple parameters it has mapped to.
     */
    Unique<FoldExpressionAst> Fold;

    struct {
        Ast *OriginalExpr;
    } Source;

    /**
     * Construct the PostfixExpressionOperatorFunctionCallAst with the arguments matching the members.
     * @param[in] generic_arg_group The generic argument group that contains the generic arguments for the function
     * call.
     * @param[in] arg_group The function call argument group that contains the arguments for the function call.
     * @param[in] fold The optional @c .. fold token that indicates a fold operation in a function call.
     */
    explicit PostfixExpressionOperatorFunctionCallAst(
        decltype(GnArgGroup) &&generic_arg_group,
        decltype(FnArgGroup) &&arg_group,
        decltype(Fold) &&fold);

    ~PostfixExpressionOperatorFunctionCallAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

    auto MarkAsAsync(Ast *async_token) -> void;

    SPP_ATTR_NODISCARD auto Target() const -> FunctionPrototypeAst*;

    auto SetClosureDummyProto(Unique<FunctionPrototypeAst> &&proto) -> void;

    auto SetTransformedAst(Unique<PostfixExpressionAst> &&ast) -> void;

    SPP_ATTR_NODISCARD auto GetTransformedAst() const -> PostfixExpressionAst*;

private:
    struct _OInfo {
        analyse::scopes::Scope const *OverloadScope;
        FunctionPrototypeAst *Proto;
        Vec<GenericArgumentAst*> GnArgs;
    };

    std::optional<_OInfo> _OverloadInfo;

    Unique<PostfixExpressionAst> _TransformedAst;

    Unique<FunctionCallArgumentGroupAst> _ClosureDummyArgGroup;
    Unique<FunctionCallArgumentPositionalAst> _ClosureDummyArg;
    Unique<FunctionPrototypeAst> _ClosureDummyProto;

    Vec<Unique<PostfixExpressionOperatorFunctionCallAst>> _FoldedAsts;

    Ast *_IsAsync;
    bool _IsCoroAndAutoResume;

    auto _HandleFunctionFolding(
        analyse::scopes::ScopeManager *sm,
        meta::CompilerMetaData *meta)
        -> Vec<Unique<PostfixExpressionOperatorFunctionCallAst>>;
};
