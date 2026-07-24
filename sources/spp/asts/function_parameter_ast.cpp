module;
#include <spp/macros.hpp>

module spp.asts.function_parameter_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.let_statement_uninitialized_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::FunctionParameterAst::FunctionParameterAst(
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type,
    const utils::OrderableTag order_tag) :
    OrderableAst(order_tag),
    Var(std::move(var)),
    TokColon(std::move(tok_colon)),
    Type(std::move(type)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokColon, lex::SppTokenType::TK_COLON, ":", var ? var->PosEnd() : 0);
    if (this->Var == nullptr) {
        const auto uid = spp::utils::Uid(this);
        const auto pos = this->Type ? this->Type->PosStart() : 0uz;
        auto var_name = MakeShared<IdentifierAst>(pos, uid);
        this->Var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);
    }
    Source.OriginalType = AstClone(Type);
}

spp::asts::FunctionParameterAst::~FunctionParameterAst() = default;

auto spp::asts::FunctionParameterAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the type.
    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type.get())->FqName()->WithConvention(AstClone(Type->GetConvention()));

    // Create the variable for the parameter (use temp copies and put them back).
    const auto ast = MakeUnique<LetStatementUninitializedAst>(nullptr, std::move(Var), nullptr, Type);
    ast->Stage7_AnalyseSemantics(sm, meta);
    Var = std::move(ast->Var);

    // Mark the symbol as initialized.
    const auto conv = Type->GetConvention();
    for (auto const &name : ExtractNames()) {
        const auto sym = sm->CurrentScope->GetVarSymbol(name.get());
        sym->MemInfo->InitializedBy(*this, sm->CurrentScope);
        sym->MemInfo->AstBorrowed = {conv, sm->CurrentScope};
    }
}

auto spp::asts::FunctionParameterAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Check the memory of each name.
    for (auto const &name : ExtractNames()) {
        const auto sym = sm->CurrentScope->GetVarSymbol(name.get());
        sym->MemInfo->InitializedBy(*this, sm->CurrentScope);
    }
}

auto spp::asts::FunctionParameterAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the local variable so that the symbol table receives the alloca.
    meta->Save();
    meta->LetStatementExplicitType = Type;
    meta->LetStatementFromUninitialized = true; // It's not uninitialized but as the value is external we need this behaviour
    Var->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    return nullptr;
}

auto spp::asts::FunctionParameterAst::ExtractNames() const
    -> Vec<Shared<IdentifierAst>> {
    // Forward to the variable declaration.
    return Var->ExtractNames();
}

auto spp::asts::FunctionParameterAst::ExtractName() const
    -> Shared<IdentifierAst> {
    // Forward to the variable declaration.
    return Var->ExtractName();
}

SPP_MOD_END
