module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_single_identifier_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import llvm;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantSingleIdentifierAst::CasePatternVariantSingleIdentifierAst(
    decltype(Conv) &&conv,
    decltype(TokMut) &&tok_mut,
    decltype(Name) &&name,
    decltype(Alias) &&alias) :
    Conv(std::move(conv)),
    TokMut(std::move(tok_mut)),
    Name(std::move(name)),
    Alias(std::move(alias)) {
}

spp::asts::CasePatternVariantSingleIdentifierAst::~CasePatternVariantSingleIdentifierAst() = default;

auto spp::asts::CasePatternVariantSingleIdentifierAst::PosStart() const
    -> std::size_t {
    // Use the "mut" token.
    return TokMut->PosStart();
}

auto spp::asts::CasePatternVariantSingleIdentifierAst::PosEnd() const
    -> std::size_t {
    // Use the alias.
    return Alias->PosEnd();
}

auto spp::asts::CasePatternVariantSingleIdentifierAst::Clone() const
    -> Unique<Ast> {
    auto a = MakeUnique<CasePatternVariantSingleIdentifierAst>(
        AstClone(Conv), AstClone(TokMut), AstCloneShared(Name), AstClone(Alias));
    a->_MappedLet = AstClone(_MappedLet);
    return a;
}

auto spp::asts::CasePatternVariantSingleIdentifierAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokMut);
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(Alias);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantSingleIdentifierAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Get the variable name.
    auto var = ConvToVar(meta);

    // Forward analysis into the name and alias.
    _MappedLet = MakeUnique<LetStatementInitializedAst>(
        nullptr, std::move(var), nullptr, nullptr, AstClone(meta->CaseCondition));
    _MappedLet->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::CasePatternVariantSingleIdentifierAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checks into the name and alias.
    _MappedLet->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::CasePatternVariantSingleIdentifierAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Emit the binding, then report a constant "true" match so the branch is always taken.
    _MappedLet->Stage11_CodeGen(sm, meta, ctx);
    return llvm::ConstantInt::getTrue(*ctx->Context);
}

auto spp::asts::CasePatternVariantSingleIdentifierAst::ConvToVar(
    CompilerMetaData *)
    -> Unique<LocalVariableAst> {
    // Create the local variable single identifier binding AST. (Note no convention is propagated into the variable,
    // as conventions are only relevant at the pattern matching site, not the variable declaration site).
    auto var = MakeUnique<LocalVariableSingleIdentifierAst>(
        AstClone(TokMut), AstCloneShared(Name), AstClone(Alias));
    var->Conv = AstClone(Conv);
    var->MarkFromCasePattern();
    return var;
}

SPP_MOD_END
