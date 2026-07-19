module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorKeywordResAst::PostfixExpressionOperatorKeywordResAst(
    decltype(TokDot) &&tok_dot,
    decltype(TokRes) &&tok_res,
    decltype(FnArgGroup) &&arg_group) :
    TokDot(std::move(tok_dot)),
    TokRes(std::move(tok_res)),
    FnArgGroup(std::move(arg_group)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokDot, lex::SppTokenType::TK_DOT, ".");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokRes, lex::SppTokenType::KW_RES, "res");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
}

spp::asts::PostfixExpressionOperatorKeywordResAst::~PostfixExpressionOperatorKeywordResAst() = default;

auto spp::asts::PostfixExpressionOperatorKeywordResAst::PosStart() const
    -> std::size_t {
    // Use the "." token.
    return TokDot->PosStart();
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::PosEnd() const
    -> std::size_t {
    // Use the argument group if it exists, otherwise use the "res" token.
    return FnArgGroup ? FnArgGroup->PosEnd() : TokRes->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<PostfixExpressionOperatorKeywordResAst>(
        AstClone(TokDot), AstClone(TokRes), AstClone(FnArgGroup));
    ast->_MappedFunc = _MappedFunc;
    return ast;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokDot);
    SPP_STRING_APPEND(TokRes);
    SPP_STRING_APPEND(FnArgGroup);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Already analysed => return early.
    using analyse::utils::type_utils::GetGenAndYieldTypes;
    if (_MappedFunc != nullptr) { return; }

    // Check the left-hand-side is a generator type (for specific errors).
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
    GetGenAndYieldTypes(
        *lhs_type, *sm->CurrentScope, *meta->PostfixExpressionLhs, "resume expression");

    // Check the argument (send value) is valid, by passing it into the ".send" function call.
    auto send = MakeUnique<IdentifierAst>(PosStart(), "send");
    auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(send));
    auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), std::move(field));
    auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(FnArgGroup), nullptr);
    func_call->Source.OriginalExpr = this;
    _MappedFunc = MakeUnique<PostfixExpressionAst>(std::move(member_access), std::move(func_call));
    _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward the memory check to the mapped function, which will check the arguments, and the function call.
    _MappedFunc->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // The left-hand side is a generator value: the { resume_fn, env } fat pointer. Resuming advances the state machine
    // that "env" points at.
    const auto uid = "." + spp::utils::Uid(this);
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    const auto llvm_gen = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);

    // Extract the resume function pointer and the env pointer. The lhs is the fat pointer value directly, or a
    // borrowed generator, so read the two fields accordingly.
    auto resume_fn = static_cast<llvm::Value*>(nullptr);
    auto env_ptr = static_cast<llvm::Value*>(nullptr);
    if (llvm_gen->getType()->isPointerTy()) {
        const auto lhs_ty = meta->PostfixExpressionLhs->InferType(sm, meta)->WithConvention(nullptr);
        const auto gen_ty = llvm::cast<llvm::StructType>(codegen::GetLlvmType(*sm->CurrentScope->GetTypeSymbol(lhs_ty), ctx));
        resume_fn = ctx->Builder.CreateLoad(ptr_ty, ctx->Builder.CreateStructGEP(gen_ty, llvm_gen, 0), "gen.resume.fn" + uid);
        env_ptr = ctx->Builder.CreateLoad(ptr_ty, ctx->Builder.CreateStructGEP(gen_ty, llvm_gen, 1), "gen.env" + uid);
    }
    else {
        resume_fn = ctx->Builder.CreateExtractValue(llvm_gen, {0u}, "gen.resume.fn" + uid);
        env_ptr = ctx->Builder.CreateExtractValue(llvm_gen, {1u}, "gen.env" + uid);
    }

    // The send value, if any. When there is no ".res(x)" argument (Send == Void), the env's send slot is a dummy i8,
    // so pass a matching zero.
    const auto llvm_send_value = FnArgGroup != nullptr and not FnArgGroup->Args.IsEmpty()
        ? FnArgGroup->Args[0]->Stage11_CodeGen(sm, meta, ctx)
        : static_cast<llvm::Value*>(llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), 0));

    // Call the resume function "(env*, send) -> void" through the pointer.
    ctx->Builder.CreateCall(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->Context), {ptr_ty, llvm_send_value->getType()}, false),
        resume_fn, {env_ptr, llvm_send_value});

    // Return the generator value (unchanged fat pointer; its env has now advanced). Reading the yielded value is a
    // separate access off the yield slot.
    return llvm_gen;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Get the generator type.
    using analyse::utils::type_utils::GetGenAndYieldTypes;
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
    auto [_, yield_type, _] = GetGenAndYieldTypes(
        *lhs_type, *sm->CurrentScope, *meta->PostfixExpressionLhs, "resume expression");
    return yield_type;
}

SPP_MOD_END
