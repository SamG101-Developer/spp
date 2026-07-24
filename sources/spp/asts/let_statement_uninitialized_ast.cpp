module;
#include <spp/macros.hpp>

module spp.asts.let_statement_uninitialized_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.asts.local_variable_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::LetStatementUninitializedAst::LetStatementUninitializedAst(
    decltype(TokLet) &&tok_let,
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type) :
    TokLet(std::move(tok_let)),
    Var(std::move(var)),
    TokColon(std::move(tok_colon)),
    Type(std::move(type)) {
    //
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokLet, lex::SppTokenType::KW_LET, "let");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokColon, lex::SppTokenType::TK_COLON, ":");
    Source.OriginalType = AstClone(Type);
}

spp::asts::LetStatementUninitializedAst::~LetStatementUninitializedAst() = default;

auto spp::asts::LetStatementUninitializedAst::PosStart() const
    -> std::size_t {
    // Use the "let" token.
    return TokLet->PosStart();
}

auto spp::asts::LetStatementUninitializedAst::PosEnd() const
    -> std::size_t {
    // Use the type.
    return Type->PosEnd();
}

auto spp::asts::LetStatementUninitializedAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LetStatementUninitializedAst>(
        AstClone(TokLet), AstClone(Var), AstClone(TokColon), AstCloneShared(Type));
}

auto spp::asts::LetStatementUninitializedAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokLet).append(" ");
    SPP_STRING_APPEND(Var);
    SPP_STRING_APPEND(TokColon).append(" ");
    SPP_STRING_APPEND(Type);
    SPP_STRING_END;
}

auto spp::asts::LetStatementUninitializedAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the type.
    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type.get())->FqName()->WithConvention(AstClone(Type->GetConvention()));

    // Create a mock value for analysis.
    const auto mock_init = MakeUnique<ObjectInitializerAst>(Type, nullptr);

    // Update the meta arguments.
    meta->Save();
    meta->LetStatementValue = mock_init.get(); // Safe, because only used within inner frame, then reset.
    meta->LetStatementExplicitType = Type;
    meta->LetStatementFromUninitialized = true;
    Var->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();
}

auto spp::asts::LetStatementUninitializedAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the variable for memory issues.
    meta->Save();
    meta->LetStatementValue = nullptr;
    meta->LetStatementExplicitType = Type;
    meta->LetStatementFromUninitialized = true;
    Var->Stage8_CheckMemory(sm, meta);
    for (auto const &v : Var->ExtractNames()) {
        sm->CurrentScope->GetVarSymbol(v.get())->MemInfo->MovedBy(*this, sm->CurrentScope);
    }
    meta->Restore();
}

auto spp::asts::LetStatementUninitializedAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Delegate the code generation to the variable, after setting up the meta.
    meta->Save();
    meta->LetStatementValue = nullptr;
    meta->LetStatementExplicitType = Type;
    meta->LetStatementFromUninitialized = true;
    const auto alloca = Var->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    return alloca;
}

SPP_MOD_END
