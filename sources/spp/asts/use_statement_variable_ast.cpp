module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.use_statement_variable_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.annotation_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.strings;
import genex;

SPP_MOD_BEGIN
spp::asts::UseStatementVariableAst::UseStatementVariableAst(
    decltype(Annotations) &&annotations,
    decltype(TokUse) &&tok_use,
    decltype(OldVar) old_var) :
    Annotations(std::move(annotations)),
    TokUse(std::move(tok_use)),
    OldVar(std::move(old_var)),
    _Conversion(nullptr) {
}

spp::asts::UseStatementVariableAst::~UseStatementVariableAst() = default;

auto spp::asts::UseStatementVariableAst::PosStart() const
    -> std::size_t {
    // Use the "use" token.
    return TokUse->PosStart();
}

auto spp::asts::UseStatementVariableAst::PosEnd() const
    -> std::size_t {
    // Use the old variable.
    return OldVar->PosEnd();
}

auto spp::asts::UseStatementVariableAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<UseStatementVariableAst>(
        AstCloneVec(Annotations), AstClone(TokUse), AstClone(OldVar));
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
    return ast;
}

auto spp::asts::UseStatementVariableAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_EXTEND(Annotations, "\n").append(not Annotations.IsEmpty() ? "\n" : "");
    SPP_STRING_APPEND(TokUse).append(" ");
    SPP_STRING_APPEND(OldVar);
    SPP_STRING_END;
}

auto spp::asts::UseStatementVariableAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Pre-process the annotations.
    Ast::Stage1_PreProcess(ctx);
    for (auto const &a : Annotations) { a->SetAstCtx(this); }
}

auto spp::asts::UseStatementVariableAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the steps for the annotations.
    Ast::Stage2_GenTopLvlScopes(sm, meta);
    for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

    // Create the conversion.
    // Todo: Error based on ordering. move this into stage 3?
    auto identifier = AstCloneShared(OldVar->ExprParts().Back()->To<IdentifierAst>());
    _Conversion = MakeUnique<CmpStatementAst>(
        std::move(Annotations), nullptr, std::move(identifier), nullptr, nullptr, nullptr, AstClone(OldVar));
    _Conversion->MarkFromUseStatement();
    _Conversion->Stage2_GenTopLvlScopes(sm, meta);
    _Generated = true;
}

auto spp::asts::UseStatementVariableAst::Stage3_GenTopLvlAliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Generate the top-level alias for the converted type statement.
    // const auto scope = sm->CurrentScope->convert_postfix_to_nested_scope(old_var->To<PostfixExpressionAst>()->lhs.get());
    const auto [old_var_sym, scope] = sm->CurrentScope->GetVarSymbolOutermost(*OldVar);
    if (old_var_sym != nullptr) {
        // Cmp statements
        _Conversion->Type = scope->GetTypeSymbol(old_var_sym->Type)->FqName(false);
        old_var_sym->Type = _Conversion->Type;

        _Conversion->_AliasSym->AliasSym = old_var_sym;
        _Conversion->_AliasSym->Type = _Conversion->Type;
        _Conversion->Stage3_GenTopLvlAliases(sm, meta);
        return;
    }

    // const auto old_ns_sym = sm->CurrentScope->convert_postfix_to_nested_scope(old_var.get());
    if (old_var_sym == nullptr) {
        // and old_ns_sym == nullptr) {
        // Todo: alternatives based on lhs of the old var.
        const auto closest_match = spp::utils::strings::ClosestMatch(
            OldVar->ToString(), {});

        Raise<analyse::errors::SppIdentifierUnknownError>(
            {sm->CurrentScope}, ERR_ARGS(*this, "constant variable", closest_match));
    }
}

auto spp::asts::UseStatementVariableAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the types in the conversion AST.
    _Conversion->Stage4_QualifyTypes(sm, meta);
}

auto spp::asts::UseStatementVariableAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load the super scopes for the conversion AST.
    _Conversion->Stage5_LoadSupScopes(sm, meta);
}

auto spp::asts::UseStatementVariableAst::Stage6_PreAnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for the conversion AST.
    _Conversion->Stage6_PreAnalyseSemantics(sm, meta);
}

auto spp::asts::UseStatementVariableAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse semantics for the conversion AST.
    _Conversion->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::UseStatementVariableAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check memory for the conversion AST.
    _Conversion->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::UseStatementVariableAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve the conversion AST.
    _Conversion->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::UseStatementVariableAst::Stage10_PreCodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Code gen for the conversion AST.
    return _Conversion->Stage10_PreCodeGen(sm, meta, ctx);
}

SPP_MOD_END
