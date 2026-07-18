module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.cmp_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import llvm;
import genex;

SPP_MOD_BEGIN
spp::asts::CmpStatementAst::CmpStatementAst(
    decltype(Annotations) &&annotations,
    decltype(TokCmp) &&tok_cmp,
    decltype(Name) &&name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type,
    decltype(TokAssign) &&tok_assign,
    decltype(Value) &&value) :
    Annotations(std::move(annotations)),
    TokCmp(std::move(tok_cmp)),
    Name(std::move(name)),
    TokColon(std::move(tok_colon)),
    Type(std::move(type)),
    TokAssign(std::move(tok_assign)),
    Value(std::move(value)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCmp, lex::SppTokenType::KW_CMP, "cmp");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokColon, lex::SppTokenType::TK_COLON, ":");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
    Source.OriginalType = AstClone(Type);
}

spp::asts::CmpStatementAst::~CmpStatementAst() = default;

auto spp::asts::CmpStatementAst::PosStart() const
    -> std::size_t {
    // Use the name.
    return TokCmp->PosStart();
}

auto spp::asts::CmpStatementAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Value->PosEnd();
}

auto spp::asts::CmpStatementAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<CmpStatementAst>(
        AstCloneVec(Annotations), AstClone(TokCmp), AstClone(Name), AstClone(TokColon), AstClone(Type),
        AstClone(TokAssign), AstClone(Value));
    ast->Visibility = Visibility;
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
    return ast;
}

auto spp::asts::CmpStatementAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_EXTEND(Annotations, "\n").append(not Annotations.IsEmpty() ? "\n" : "");
    SPP_STRING_APPEND(TokCmp).append(" ");
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokColon).append(" ");
    SPP_STRING_APPEND(Type).append(" ");
    SPP_STRING_APPEND(TokAssign).append(" ");
    SPP_STRING_APPEND(Value);
    SPP_STRING_END;
}

auto spp::asts::CmpStatementAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // No pre-processing needed for cmp statements.
    Ast::Stage1_PreProcess(ctx);
    for (auto const &a : Annotations) { a->Stage1_PreProcess(this); }
}

auto spp::asts::CmpStatementAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // No top-level scopes needed for cmp statements.
    Ast::Stage2_GenTopLvlScopes(sm, meta);
    for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

    // Create a symbol for this constant declaration, pin to prevent moving.
    _AliasSym = MakeShared<analyse::scopes::VariableSymbol>(
        Name, Type, sm->CurrentScope, false, false, Visibility.First);
    // _AliasSym->MemInfo->AstPins.EmplaceBack(Name.get()); TODO
    _AliasSym->MemInfo->AstCompTime = AstClone(this);
    _AliasSym->MemInfo->InitializedBy(*this, sm->CurrentScope);
    sm->CurrentScope->AddVarSymbolCheckConflict(_AliasSym);
}

auto spp::asts::CmpStatementAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Note: we don't block second class borrows here, because &StrView for example works with the string literal. As it
    // otherwise impossible to assign borrows in the cmp context, unless from other cmp borrows, this is safe (cmp
    // coroutines are not a thing)
    using analyse::utils::type_utils::IsTypeBorrowed;
    for (auto const &a : Annotations) { a->Stage4_QualifyTypes(sm, meta); }

    // Qualify the type.
    Type->Stage4_QualifyTypes(sm, meta);
    Type->Stage7_AnalyseSemantics(sm, meta);

    if (not _FromUseStatement and not Type->IsCompilerGeneratedType() and not Type->IsSelfType()) {
        Type = sm->CurrentScope->GetTypeSymbol(Type)->FqName()->WithConvention(AstClone(Type->GetConvention()));
        _AliasSym->Type = Type;
    }
}

auto spp::asts::CmpStatementAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }

    // Check the type exists before attaching super scopes
    // type->Stage7_AnalyseSemantics(sm, meta);

    if (_AliasSym != nullptr and not Type->IsCompilerGeneratedType()) {
        _AliasSym->Visibility = Visibility.First;
        _AliasSym->VisibilityAnnotation = Visibility.Second;
    }
}

auto spp::asts::CmpStatementAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppTypeMismatchError;
    using analyse::utils::type_utils::TypeEq;
    for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }

    // Analyse the type and value.
    meta->Save();
    meta->ReturnTypeOverloadResolverType = Type; // Todo: Add this to unit tests.
    Value->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();

    // Check the value's type is the same as the given type.
    const auto inferred_type = Value->InferType(sm, meta);

    RaiseIf<SppTypeMismatchError>(
        not IsFromUseStatement()
        and not TypeEq(*Type, *inferred_type, *sm->CurrentScope, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*Source.OriginalType, *Type, *Value, *inferred_type));
}

auto spp::asts::CmpStatementAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the type.
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    Value->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Value, *Value, *sm, true, true, true, true, true, meta);

    // Generate the value and assign it to the variable symbol's compile-time value.
    if (not Type->IsCompilerGeneratedType()) {
        const auto var_sym = sm->CurrentScope->GetVarSymbol(Name);
        var_sym->CompTimeValue = AstClone(Value);
    }
}

auto spp::asts::CmpStatementAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }

    // Generate the value and assign it to the variable symbol's compile-time value.
    if (not Type->IsCompilerGeneratedType()) {
        const auto var_sym = sm->CurrentScope->GetVarSymbol(Name);
        Value->Stage9_CompTimeResolve(sm, meta);
        var_sym->CompTimeValue = std::move(meta->CmpResult);
    }
}

auto spp::asts::CmpStatementAst::Stage10_PreCodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // No generation for $ types.
    if (Type->IsCompilerGeneratedType()) { return nullptr; }

    // Generate the value in a constant context.
    ctx->InConstantContext = true;
    const auto var_sym = sm->CurrentScope->GetVarSymbol(Name);
    const auto val = var_sym->CompTimeValue->Stage11_CodeGen(sm, meta, ctx);
    ctx->InConstantContext = false;

    // Create the global variable for the constant.
    const auto type_sym = sm->CurrentScope->GetTypeSymbol(Type);
    const auto llvm_type = codegen::llvm_type(*type_sym, ctx);
    const auto llvm_global_var = new llvm::GlobalVariable(
        *ctx->Module, llvm_type, true, llvm::GlobalValue::ExternalLinkage, llvm::cast<llvm::Constant>(val),
        codegen::mangle::mangle_cmp_name(*sm->CurrentScope, *this));

    // Register in the llvm info.
    var_sym->LlvmInfo->Alloca = llvm_global_var;
    return nullptr;
}

auto spp::asts::CmpStatementAst::MarkFromUseStatement()
    -> void {
    _FromUseStatement = true;
}

auto spp::asts::CmpStatementAst::IsFromUseStatement() const
    -> bool {
    return _FromUseStatement;
}

SPP_MOD_END
