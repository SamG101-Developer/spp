module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_iterable_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.loop_conditional_expression_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::LoopIterableExpressionAst::LoopIterableExpressionAst(
    decltype(TokLoop) &&tok_loop,
    decltype(Var) &&var,
    decltype(TokIn) &&tok_in,
    decltype(Iterable) &&iterable,
    decltype(Body) &&body,
    decltype(ElseBlock) &&else_block) :
    LoopExpressionAst(std::move(tok_loop), std::move(body), std::move(else_block)),
    Var(std::move(var)),
    TokIn(std::move(tok_in)),
    Iterable(std::move(iterable)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokIn, lex::SppTokenType::KW_IN, "in");
}

spp::asts::LoopIterableExpressionAst::~LoopIterableExpressionAst() = default;

auto spp::asts::LoopIterableExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "loop" token or the transformed loop.
    return TokLoop != nullptr ? TokLoop->PosStart() : _TransformedLoop->PosStart();
}

auto spp::asts::LoopIterableExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the iterable or the transformed loop.
    return Iterable != nullptr ? Iterable->PosEnd() : _TransformedLoop->PosEnd();
}

auto spp::asts::LoopIterableExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LoopIterableExpressionAst>(
        AstClone(TokLoop), AstClone(Var), AstClone(TokIn), AstClone(Iterable), AstClone(Body), AstClone(ElseBlock));
}

auto spp::asts::LoopIterableExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokLoop).append(" ");
    SPP_STRING_APPEND(Var).append(" ");
    SPP_STRING_APPEND(TokIn).append(" ");
    SPP_STRING_APPEND(Iterable).append(" ");
    SPP_STRING_APPEND(Body).append("\n");
    SPP_STRING_APPEND(ElseBlock);
    SPP_STRING_END;
}

auto spp::asts::LoopIterableExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::GetGenAndYieldTypes;

    // Exp check.
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Iterable),
        {sm->CurrentScope}, ERR_ARGS(*Iterable));

    // Simple statements to move from.
    const auto uid = spp::utils::Uid(this);
    auto skip_stmt = LoopControlFlowStatementAst::Skip(PosStart());
    auto exit_stmt = LoopControlFlowStatementAst::Exit(PosStart(), 1);
    auto iterable_name = MakeShared<IdentifierAst>(PosStart(), "$_iter_" + uid);
    auto resume_name = MakeShared<IdentifierAst>(PosStart(), "$_res_" + uid);

    // Grab the generator's inner type.
    auto [_, yield_type, _] = ( {
        auto clone_expr = AstClone(Iterable);
        auto tm = ScopeManager(sm->GlobalScope, sm->CurrentScope);
        tm.Reset(sm->CurrentScope, sm->CurrentIterator());
        clone_expr->Stage7_AnalyseSemantics(&tm, meta);
        auto iterable_type = clone_expr->InferType(&tm, meta);
        GetGenAndYieldTypes(
            *iterable_type, *tm.CurrentScope, *Iterable, "loop iterable");
    });

    // Create the initial let statement to materialize the condition being iterated.
    // Translated: "let $_iter = <iterable>".
    auto iterable_let = ( {
        auto iterable_var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, iterable_name, nullptr);
        auto iterable_val = std::move(Iterable);
        MakeUnique<LetStatementInitializedAst>(nullptr, std::move(iterable_var), nullptr, nullptr, std::move(iterable_val));
    });

    // Create the "let" statement that increments the iterator.
    // Translated: "let $_res = $_iter.resume()".
    auto resume_let = ( {
        auto resume_fn_call = MakeUnique<PostfixExpressionOperatorKeywordResAst>(nullptr, nullptr, nullptr);
        auto resume_val = MakeUnique<PostfixExpressionAst>(AstClone(iterable_name), std::move(resume_fn_call));
        auto resume_var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, resume_name, nullptr);
        MakeUnique<LetStatementInitializedAst>(nullptr, AstClone(resume_var), nullptr, nullptr, std::move(resume_val));
    });

    // If the value is the iterable type, then use the inner body.
    // Translated: "<case-of-expr> is &Str::Str(..) { ... }".
    auto case_valid_branch = ( {
        auto ignore_fields = MakeUnique<CasePatternVariantDestructureSkipMultipleArgumentsAst>(nullptr, nullptr);
        auto case_type_pattern = CasePatternVariantDestructureObjectAst::FromType(yield_type);
        case_type_pattern->Elems.EmplaceBack(std::move(ignore_fields));

        auto is_tok = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_IS, "is");
        auto patterns = Vec<Unique<CasePatternVariantAst>>();
        patterns.EmplaceBack(std::move(case_type_pattern));

        // Destructure the "$_res" variable into parts defined in "loop (v1, v2) in ..."
        auto let_stmt = MakeUnique<LetStatementInitializedAst>(nullptr, std::move(Var), nullptr, nullptr, AstClone(resume_name));
        Body->Members.Insert(Body->Members.begin(), std::move(let_stmt));
        MakeUnique<CaseExpressionBranchAst>(std::move(is_tok), std::move(patterns), nullptr, std::move(Body));
    });

    // Otherwise, exit the loop.
    // Translated: "<case-of-expr> else { exit }".
    auto case_else_branch = ( {
        auto case_else_pattern = MakeUnique<CasePatternVariantElseAst>(nullptr);
        auto case_else_body = InnerScopeExpressionAst::NewEmpty();
        case_else_pattern->MarkForIterLoopExit();

        auto is_tok = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_IS, "is");
        auto patterns = Vec<Unique<CasePatternVariantAst>>();
        patterns.EmplaceBack(std::move(case_else_pattern));
        case_else_body->Members.EmplaceBack(std::move(exit_stmt));
        MakeUnique<CaseExpressionBranchAst>(std::move(is_tok), std::move(patterns), nullptr, std::move(case_else_body));
    });

    // Case block to handle the resume value.
    auto case_expr = ( {
        auto case_branches = Vec<Unique<CaseExpressionBranchAst>>();
        case_branches.EmplaceBack(std::move(case_valid_branch));
        case_branches.EmplaceBack(std::move(case_else_branch));
        MakeUnique<CaseExpressionAst>(nullptr, AstClone(resume_name), nullptr, std::move(case_branches));
    });

    // New boolean loop that manually iterates the iterable.
    auto loop_new = ( {
        auto cond = BooleanLiteralAst::True(PosStart());
        MakeUnique<LoopConditionalExpressionAst>(nullptr, std::move(cond), nullptr, nullptr);
    });
    loop_new->Body->Members.EmplaceBack(std::move(resume_let));
    loop_new->Body->Members.EmplaceBack(std::move(case_expr));
    _TransformedLoop = std::move(loop_new);

    // Analyse the initial let statement.
    iterable_let->Stage7_AnalyseSemantics(sm, meta);
    _TransformedLet = std::move(iterable_let);

    // Analyse the transformed loop (makes its own scope).
    meta->Save();
    meta->LoopCurrentDepth += 1;
    meta->LoopCurrentAst = this;
    _TransformedLoop->Stage7_AnalyseSemantics(sm, meta);
    if (meta->LoopReturnTypes->contains(meta->LoopCurrentDepth - 1)) {
        m_loop_exit_type_info = (*meta->LoopReturnTypes)[meta->LoopCurrentDepth - 1];
    }
    meta->Restore();
}

auto spp::asts::LoopIterableExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Call the memory check on the transformed loop.
    _TransformedLet->Stage8_CheckMemory(sm, meta);
    _TransformedLoop->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::LoopIterableExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the code for the transformed loop.
    _TransformedLet->Stage11_CodeGen(sm, meta, ctx);
    return _TransformedLoop->Stage11_CodeGen(sm, meta, ctx);
}

SPP_MOD_END
