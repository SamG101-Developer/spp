module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_iterable_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.asts.assignment_statement_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.loop_conditional_expression_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

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
    // TODO: Move the translation into "Stage1_PreProcess()" and just call that from here.
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::GetGenAndYieldTypes;

    // Simple statements to move from.
    const auto uid = spp::utils::Uid(this);
    auto iterable_name = MakeShared<IdentifierAst>(PosStart(), "$_iter_" + uid);
    auto resume_name = MakeShared<IdentifierAst>(PosStart(), "$_res_" + uid);
    auto flag_name = MakeShared<IdentifierAst>(PosStart(), "$_ok_" + uid);
    _IterableName = iterable_name;

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
        auto mut = std::make_unique<TokenAst>(PosStart(), lex::SppTokenType::KW_MUT, "mut");
        auto iterable_var = MakeUnique<LocalVariableSingleIdentifierAst>(std::move(mut), iterable_name, nullptr);
        auto iterable_val = std::move(Iterable);
        MakeUnique<LetStatementInitializedAst>(nullptr, std::move(iterable_var), nullptr, nullptr, std::move(iterable_val));
    });

    // Create the let statement holding the loop's continuation flag, to control entering the else block.
    // Translated: "let mut $_ok = true".
    auto flag_let = ( {
        auto mut = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_MUT, "mut");
        auto flag_var = MakeUnique<LocalVariableSingleIdentifierAst>(std::move(mut), flag_name, nullptr);
        auto flag_val = BooleanLiteralAst::True(PosStart());
        MakeUnique<LetStatementInitializedAst>(nullptr, std::move(flag_var), nullptr, nullptr, std::move(flag_val));
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

    // Reaching the branch above is what counts as the loop having taken an iteration.
    case_valid_branch->MarkForIterLoopYield();

    // Otherwise, the generator is exhausted, so clear the continuation flag.
    // Translated: "<case-of-expr> else { $_ok = false }".
    auto case_else_branch = ( {
        auto case_else_pattern = MakeUnique<CasePatternVariantElseAst>(nullptr);
        auto case_else_body = InnerScopeExpressionAst::NewEmpty();
        case_else_pattern->MarkForIterLoopExit();

        auto flag_clear_lhs = UniqueVec<ExpressionAst>();
        auto flag_clear_rhs = UniqueVec<ExpressionAst>();
        flag_clear_lhs.EmplaceBack(AstClone(flag_name));
        flag_clear_rhs.EmplaceBack(BooleanLiteralAst::False(PosStart()));
        auto flag_clear = MakeUnique<AssignmentStatementAst>(
            std::move(flag_clear_lhs), nullptr, std::move(flag_clear_rhs));

        auto is_tok = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_IS, "is");
        auto patterns = Vec<Unique<CasePatternVariantAst>>();
        patterns.EmplaceBack(std::move(case_else_pattern));
        case_else_body->Members.EmplaceBack(std::move(flag_clear));
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
    // Translated: "loop $_ok { ... } else { ... }".
    auto loop_new = ( {
        auto cond = AstClone(flag_name);
        MakeUnique<LoopConditionalExpressionAst>(nullptr, std::move(cond), nullptr, std::move(ElseBlock));
    });
    loop_new->Body->Members.EmplaceBack(std::move(resume_let));
    loop_new->Body->Members.EmplaceBack(std::move(case_expr));
    loop_new->MarkAsIterDesugar();
    _TransformedLoop = std::move(loop_new);

    // Analyse the initial let statements.
    iterable_let->Stage7_AnalyseSemantics(sm, meta);
    _TransformedLet = std::move(iterable_let);
    flag_let->Stage7_AnalyseSemantics(sm, meta);
    _TransformedFlagLet = std::move(flag_let);
    _TransformedLoop->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::LoopIterableExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Call the memory check on the transformed loop.
    _TransformedLet->Stage8_CheckMemory(sm, meta);
    _TransformedFlagLet->Stage8_CheckMemory(sm, meta);
    _TransformedLoop->Stage8_CheckMemory(sm, meta);

    // The desugared iterator ("$_iter") is a hidden temporary whose lifetime ends with the loop. Release any escaping
    // borrows it holds (e.g. the "&mut v" established by "v.iter_mut()").
    const auto iter_sym = sm->CurrentScope->GetVarSymbol(_IterableName);
    for (auto const &ceb : iter_sym->MemInfo->AstContainedEscapingBorrows) {
        const auto b = AstCloneShared(std::get<0>(ceb)->To<IdentifierAst>());
        if (b == nullptr) { continue; }
        sm->CurrentScope->GetVarSymbol(b)->MemInfo->AstContainersOfEscapingBorrows |= genex::actions::remove_if([&](auto info) {
            return *std::get<0>(info)->template To<IdentifierAst>() == *iter_sym->Name;
        });
    }
    iter_sym->MemInfo->AstContainedEscapingBorrows.Clear();
}

auto spp::asts::LoopIterableExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the code for the transformed loop.
    _TransformedLet->Stage11_CodeGen(sm, meta, ctx);
    _TransformedFlagLet->Stage11_CodeGen(sm, meta, ctx);
    return _TransformedLoop->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::LoopIterableExpressionAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Before the desugaring in stage 7 there is no transformed loop to ask, so fall back to the base implementation.
    return _TransformedLoop == nullptr
        ? LoopExpressionAst::InferType(sm, meta)
        : _TransformedLoop->InferType(sm, meta);
}

SPP_MOD_END
