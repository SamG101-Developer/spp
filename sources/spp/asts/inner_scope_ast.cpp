module;
#include <spp/macros.hpp>

module spp.asts.inner_scope_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.class_member_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.sup_member_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
template <typename T>
auto spp::asts::InnerScopeAst<T>::NewEmpty()
    -> Unique<InnerScopeAst> {
    return MakeUnique<InnerScopeAst>(
        nullptr, decltype(Members)(), nullptr);
}

template <typename T>
spp::asts::InnerScopeAst<T>::InnerScopeAst() :
    TokL(nullptr),
    Members(),
    TokR(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_CURLY_BRACE, "{");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_CURLY_BRACE, "}");
}

template <typename T>
spp::asts::InnerScopeAst<T>::InnerScopeAst(
    decltype(TokL) &&tok_l,
    decltype(Members) &&members,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Members(std::move(members)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_CURLY_BRACE, "{");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_CURLY_BRACE, "}");
}

template <typename T>
spp::asts::InnerScopeAst<T>::~InnerScopeAst() = default;

template <typename T>
auto spp::asts::InnerScopeAst<T>::PosStart() const
    -> std::size_t {
    // Use the "{" token.
    return TokL->PosStart();
}

template <typename T>
auto spp::asts::InnerScopeAst<T>::PosEnd() const
    -> std::size_t {
    // Use the "}" token.
    return TokR->PosEnd();
}

template <typename T>
auto spp::asts::InnerScopeAst<T>::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<InnerScopeAst>(
        AstClone(TokL), AstCloneVec(Members), AstClone(TokR));
}

template <typename T>
auto spp::asts::InnerScopeAst<T>::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL).append(not Members.IsEmpty() ? "\n" : "");
    SPP_STRING_EXTEND(Members, "\n").append(not Members.IsEmpty() ? "\n" : "");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

template <typename T>
auto spp::asts::InnerScopeAst<T>::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a scope for the InnerScopeAst node.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "inner-scope", {}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    _Scope = sm->CurrentScope;

    // Analyse the members of the inner scope.
    for (auto const &x : this->Members) { x->Stage7_AnalyseSemantics(sm, meta); }
    sm->MoveOutOfCurrentScope();
}

template <typename T>
auto spp::asts::InnerScopeAst<T>::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Move into the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Check the memory of each member.
    for (auto const &m : Members) { m->Stage8_CheckMemory(sm, meta); }

    // If the final expression of the inner scope is being used (ie assigned or outer variable), then memory check it.
    if (const auto move = meta->AssignmentTarget; not Members.IsEmpty() and move != nullptr) {
        if (const auto expr_member = FinalMember()->template To<ExpressionAst>(); expr_member != nullptr) {
            ValidateSymbolMemory(*expr_member, *move, *sm, true, true, true, true, meta);
        }
    }

    sm->MoveOutOfCurrentScope();
}

template <typename T>
auto spp::asts::InnerScopeAst<T>::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Add all the expressions/statements into the current scope.
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);

    for (auto const &m : Members) { m->Stage11_CodeGen(sm, meta, ctx); }

    // Need to add a "return void" if the final member isn't a RetStatementAst.
    if (Members.IsEmpty() or Members.Back()->template To<RetStatementAst>() == nullptr) {
        ctx->Builder.CreateRetVoid();
    }

    // Exit the scope.
    sm->MoveOutOfCurrentScope();
    return nullptr;
}

template <typename T>
auto spp::asts::InnerScopeAst<T>::FinalMember() const
    -> Ast* {
    return Members.IsEmpty() ? TokR->To<Ast>() : Members.Back()->template To<Ast>();
}

template struct spp::asts::InnerScopeAst<spp::Unique<spp::asts::Ast>>;
SPP_MOD_END
