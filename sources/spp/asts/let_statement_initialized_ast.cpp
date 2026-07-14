module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.let_statement_initialized_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::LetStatementInitializedAst::LetStatementInitializedAst(
    decltype(TokLet) &&tok_let,
    decltype(Var) &&var,
    decltype(Type) type,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val) :
    TokLet(std::move(tok_let)),
    Var(std::move(var)),
    Type(std::move(type)),
    TokAssign(std::move(tok_assign)),
    Val(std::move(val)) {
    //
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokLet, lex::SppTokenType::KW_LET, "let");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
    Source.OriginalType = AstClone(Type);
}

spp::asts::LetStatementInitializedAst::~LetStatementInitializedAst() = default;

auto spp::asts::LetStatementInitializedAst::PosStart() const
    -> std::size_t {
    // Use the "let" token.
    return TokLet->PosStart();
}

auto spp::asts::LetStatementInitializedAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::LetStatementInitializedAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LetStatementInitializedAst>(
        AstClone(TokLet), AstClone(Var), AstClone(Type), AstClone(TokAssign), AstClone(Val));
}

auto spp::asts::LetStatementInitializedAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokLet).append(" ");
    SPP_STRING_APPEND(Var);
    SPP_STRING_APPEND(Type).append(" ");
    SPP_STRING_APPEND(TokAssign).append(" ");
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::LetStatementInitializedAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Todo: Test preventing "let x = void_type()" + same for "let x: Void"
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppInvalidLocalVariableTypeAnnotationError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::TypeEq;

    // An explicit type can only be applied if the left-hand-side is a single identifier.
    RaiseIf<SppInvalidLocalVariableTypeAnnotationError>(
        Type != nullptr and Var->To<LocalVariableSingleIdentifierAst>() == nullptr,
        {sm->CurrentScope}, ERR_ARGS(*Type, *Var));

    // Analyse the type if it has been given.
    if (Type != nullptr) {
        Type->Stage7_AnalyseSemantics(sm, meta);
        Type = sm->CurrentScope->GetTypeSymbol(Type.Get())->FqName()->WithConvention(AstClone(Type->GetConvention()));
    }

    // Add the type into the return type overload resolver.
    meta->Save();
    meta->ReturnTypeOverloadResolverType = Type.Get();

    // Check the value is a valid expression type.
    Val->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Val, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Val.Get()));

    meta->AssignmentTarget = Var->ExtractName();

    // Ensure the value's type matches the type (if given), including variant matching.
    if (Type != nullptr) {
        meta->AssignmentTargetType = Type.Get();
        const auto val_type = Val->InferType(sm, meta);
        RaiseIf<analyse::errors::SppTypeMismatchError>(
            not TypeEq(*Type, *val_type, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*Source.OriginalType, *Type, *Val, *val_type));
    }

    meta->LetStatementExplicitType = Type.Get();
    meta->LetStatementValue = Val.Get();
    Var->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();
}

auto spp::asts::LetStatementInitializedAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Check the variable's memory (which in turn checks the values memory - must be done this way for destructuring).
    meta->Save();
    meta->AssignmentTarget = Var->ExtractName();
    meta->LetStatementExplicitType = Type.Get();
    meta->LetStatementValue = Val.Get();
    Var->Stage8_CheckMemory(sm, meta);
    meta->Restore();
}

auto spp::asts::LetStatementInitializedAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Comptime resolve the value.
    Val->Stage9_CompTimeResolve(sm, meta);

    // Assign the comptime value to the variable.
    meta->Save();
    meta->AssignmentTarget = Var->ExtractName();
    meta->LetStatementExplicitType = Type.Get();
    meta->LetStatementValue = Val.Get();
    Var->Stage9_CompTimeResolve(sm, meta);
    meta->Restore();
}

auto spp::asts::LetStatementInitializedAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    const auto inferred = Val->InferType(sm, meta);

    // Delegate the code generation to the variable, after setting up the meta.
    meta->Save();
    meta->AssignmentTarget = Var->ExtractName();
    meta->AssignmentTargetType = Type != nullptr ? Type.Get() : inferred;
    meta->LetStatementExplicitType = Type != nullptr ? Type.Get() : inferred;
    meta->LetStatementValue = Val.Get();
    const auto alloca = Var->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    return alloca;
}

SPP_MOD_END
