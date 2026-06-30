module;
#include <spp/macros.hpp>

module spp.asts.inner_scope_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
auto spp::asts::InnerScopeExpressionAst::NewEmpty()
    -> Unique<InnerScopeExpressionAst> {
    // Empty AST.
    return MakeUnique<InnerScopeExpressionAst>(nullptr, decltype(Members)(), nullptr);
}

spp::asts::InnerScopeExpressionAst::InnerScopeExpressionAst(
    decltype(TokL) &&tok_l,
    decltype(Members) &&members,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Members(std::move(members)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_CURLY_BRACE, "{");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_CURLY_BRACE, "}");
}

spp::asts::InnerScopeExpressionAst::~InnerScopeExpressionAst() = default;

auto spp::asts::InnerScopeExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "{" token.
    return TokL->PosStart();
}

auto spp::asts::InnerScopeExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the "}" token.
    return TokR->PosEnd();
}

auto spp::asts::InnerScopeExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<InnerScopeExpressionAst>(
        AstClone(TokL), AstCloneVec(Members), AstClone(TokR));
}

auto spp::asts::InnerScopeExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL).append(not Members.IsEmpty() ? "\n" : "");
    SPP_STRING_EXTEND(Members, "\n").append(not Members.IsEmpty() ? "\n" : "");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::InnerScopeExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::expr_utils::ValidateNoUnreachableCode;

    // Create a scope for the InnerScopeAst node.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "inner-scope-expression", {}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    _Scope = sm->CurrentScope;

    // Check for unreachable code.
    ValidateNoUnreachableCode(
        this->Members | genex::views::ptr | genex::to<Vec>(), *sm);

    // Analyse the members of the inner scope.
    for (auto const &x : this->Members) { x->Stage7_AnalyseSemantics(sm, meta); }
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::InnerScopeExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Move into the next scope.
    sm->MoveToNextScope();

    // Check the memory of each member.
    for (auto const &m : Members) { m->Stage8_CheckMemory(sm, meta); }

    // If the final expression of the inner scope is being used (ie assigned ot outer variable), then memory check it.
    if (const auto move = meta->AssignmentTarget; not Members.IsEmpty() and move != nullptr) {
        if (const auto expr_member = FinalMember()->template To<ExpressionAst>(); expr_member != nullptr) {
            ValidateSymbolMemory(*expr_member, *move, *sm, true, true, true, true, true, meta);
        }
    }

    // Variable symbols' memory info structs contain the escaping borrow containers, and the contained escaping borrows.
    // At the end of a scope, we need to check, for every symbol, if it contains any escaping borrows. If the
    // containment happened in this scope, then we need to free the escaping borrows, as the container is now out of
    // scope.
    for (auto const &sym : sm->CurrentScope->AllVarSymbols()) {
        auto contained_escaping_borrows = sym->MemInfo->AstContainedEscapingBorrows
            | genex::views::filter([&](auto const &x) { return std::get<2>(x) == sm->CurrentScope; })
            | genex::to<Vec>();

        for (auto const &ceb : contained_escaping_borrows) {
            sym->MemInfo->AstContainedEscapingBorrows |= genex::actions::remove(ceb);
            const auto b = AstCloneShared(std::get<0>(ceb)->To<IdentifierAst>());
            if (b == nullptr) { continue; }
            sm->CurrentScope->GetVarSymbol(b)->MemInfo->AstContainersOfEscapingBorrows |= genex::actions::remove_if([&](auto info) {
                return *std::get<0>(info)->template To<IdentifierAst>() == *sym->Name;
            });
        }
    }

    // Any escaping borrows that were defined in this scope need to be freed.
    // for (auto const &sym : sm->CurrentScope->AllVarSymbols()) {
    //     auto escaping_borrows = sym->MemInfo->AstContainedEscapingBorrows; // Copy
    //     for (auto const &eb : escaping_borrows) {
    //         auto const &[ast, _, scope] = eb;
    //         if (scope != sm->CurrentScope) { continue; }
    //         sym->MemInfo->AstContainedEscapingBorrows.erase(
    //             std::ranges::remove(sym->MemInfo->AstContainedEscapingBorrows, eb).begin(),
    //             sym->MemInfo->AstContainedEscapingBorrows.end());
    //     }
    // }

    sm->MoveOutOfCurrentScope();
}

auto spp::asts::InnerScopeExpressionAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve each member of the inner scope.
    sm->MoveToNextScope();
    for (auto const &m : this->Members) {
        const auto did_ret = m->template To<RetStatementAst>() != nullptr;
        m->Stage9_CompTimeResolve(sm, meta);
        if (did_ret) { break; }
    }

    // Exit the scope.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::InnerScopeExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Add all the expressions/statements into the current scope.
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);

    auto ret_val = static_cast<llvm::Value*>(nullptr);
    for (auto const &m : this->Members) {
        ret_val = m->Stage11_CodeGen(sm, meta, ctx);
    }

    // Exit the scope.
    sm->MoveOutOfCurrentScope();

    if (ret_val and meta->LlvmAssignmentTarget) {
        ctx->Builder.CreateStore(ret_val, meta->LlvmAssignmentTarget);
        return ret_val;
    }
    return ret_val;
}

auto spp::asts::InnerScopeExpressionAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // If there are any members, return the last member's inferred type.
    if (not this->Members.IsEmpty()) {
        auto tm = ScopeManager(sm->GlobalScope, GetAstScope());
        return this->Members.Back()->InferType(&tm, meta);
    }

    // Otherwise, return the void type.
    return generate::common_types::VoidType(PosStart());
}

auto spp::asts::InnerScopeExpressionAst::Terminates() const
    -> bool {
    // The inner scope expression only terminates if the last member terminates.
    if (this->Members.IsEmpty()) { return false; }
    return this->Members.Back()->Terminates();
}

auto spp::asts::InnerScopeExpressionAst::FinalMember() const
    -> Ast* {
    return Members.IsEmpty() ? TokR->To<Ast>() : Members.Back()->template To<Ast>();
}

SPP_MOD_END
