module;
#include <spp/macros.hpp>

module spp.asts.loop_iterable_expression_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
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


spp::asts::LoopIterableExpressionAst::LoopIterableExpressionAst(
    decltype(tok_loop) &&tok_loop,
    decltype(var) &&var,
    decltype(tok_in) &&tok_in,
    decltype(iterable) &&iterable,
    decltype(body) &&body,
    decltype(else_block) &&else_block) :
    LoopExpressionAst(std::move(tok_loop), std::move(body), std::move(else_block)),
    var(std::move(var)),
    tok_in(std::move(tok_in)),
    iterable(std::move(iterable)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_in, lex::SppTokenType::KW_IN, "in");
}


spp::asts::LoopIterableExpressionAst::~LoopIterableExpressionAst() = default;


auto spp::asts::LoopIterableExpressionAst::pos_start() const
    -> std::size_t {
    return tok_loop->pos_start();
}


auto spp::asts::LoopIterableExpressionAst::pos_end() const
    -> std::size_t {
    return iterable != nullptr ? iterable->pos_end() : m_transform_loop->pos_end();
}


auto spp::asts::LoopIterableExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LoopIterableExpressionAst>(
        ast_clone(tok_loop),
        ast_clone(var),
        ast_clone(tok_in),
        ast_clone(iterable),
        ast_clone(body),
        ast_clone(else_block));
}


spp::asts::LoopIterableExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_loop).append(" ");
    SPP_STRING_APPEND(var).append(" ");
    SPP_STRING_APPEND(tok_in).append(" ");
    SPP_STRING_APPEND(iterable).append(" ");
    SPP_STRING_APPEND(body).append("\n");
    SPP_STRING_APPEND(else_block);
    SPP_STRING_END;
}


auto spp::asts::LoopIterableExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Simple statements to move from.
    const auto uid = spp::utils::generate_uid(this);
    auto skip_stmt = LoopControlFlowStatementAst::Skip(pos_start());
    auto exit_stmt = LoopControlFlowStatementAst::Exit(pos_start());
    auto iterable_name = std::make_shared<IdentifierAst>(pos_start(), "$_iter_" + uid);
    auto resume_name = std::make_shared<IdentifierAst>(pos_start(), "$_res_" + uid);

    // Grab the generator's inner type.
    auto [_, yield_type, _] = ( {
        auto clone_expr = ast_clone(iterable);
        auto tm = ScopeManager(sm->global_scope, sm->current_scope);
        tm.reset(sm->current_scope, sm->current_iterator());
        clone_expr->stage_7_analyse_semantics(&tm, meta);
        auto iterable_type = clone_expr->infer_type(&tm, meta);
        analyse::utils::type_utils::get_generator_and_yield_type(
            *iterable_type, *tm.current_scope, *iterable, "loop iterable");
    });

    // Create the initial let statement to materialize the condition being iterated.
    // Translated: "let $_iter = <iterable>".
    auto iterable_let = ( {
        auto iterable_var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, iterable_name, nullptr);
        auto iterable_val = std::move(iterable);
        std::make_unique<LetStatementInitializedAst>(nullptr, std::move(iterable_var), nullptr, nullptr, std::move(iterable_val));
    });

    // Create the "let" statement that increments the iterator.
    // Translated: "let $_res = $_iter.resume()".
    auto resume_let = ( {
        auto resume_fn_call = std::make_unique<PostfixExpressionOperatorKeywordResAst>(nullptr, nullptr, nullptr);
        auto resume_val = std::make_unique<PostfixExpressionAst>(ast_clone(iterable_name), std::move(resume_fn_call));
        auto resume_var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, resume_name, nullptr);
        std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(resume_var), nullptr, nullptr, std::move(resume_val));
    });

    // If the value is the iterable type, then use the inner body.
    // Translated: "<case-of-expr> is &std::string::Str(..) { ... }".
    auto case_valid_branch = ( {
        auto ignore_fields = std::make_unique<CasePatternVariantDestructureSkipMultipleArgumentsAst>(nullptr, nullptr);
        auto case_type_pattern = CasePatternVariantDestructureObjectAst::from_type(yield_type);
        case_type_pattern->elems.emplace_back(std::move(ignore_fields));

        auto is_tok = std::make_unique<TokenAst>(pos_start(), lex::SppTokenType::KW_IS, "is");
        auto patterns = std::vector<std::unique_ptr<CasePatternVariantAst>>();
        patterns.emplace_back(std::move(case_type_pattern));

        // Destructure the "$_res" variable into parts defined in "loop (v1, v2) in ..."
        auto let_stmt = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, ast_clone(resume_name));
        body->members.insert(body->members.begin(), std::move(let_stmt));
        std::make_unique<CaseExpressionBranchAst>(std::move(is_tok), std::move(patterns), nullptr, std::move(body));
    });

    // Otherwise, exit the loop.
    // Translated: "<case-of-expr> else { exit; }".
    auto case_else_branch = ( {
        auto case_else_pattern = std::make_unique<CasePatternVariantElseAst>(nullptr);
        auto case_else_body = InnerScopeExpressionAst<std::unique_ptr<StatementAst>>::new_empty();

        auto is_tok = std::make_unique<TokenAst>(pos_start(), lex::SppTokenType::KW_IS, "is");
        auto patterns = std::vector<std::unique_ptr<CasePatternVariantAst>>();
        patterns.emplace_back(std::move(case_else_pattern));
        case_else_body->members.emplace_back(std::move(exit_stmt));
        std::make_unique<CaseExpressionBranchAst>(std::move(is_tok), std::move(patterns), nullptr, std::move(case_else_body));
    });

    // Case block to handle the resume value.
    auto case_expr = ( {
        auto case_branches = std::vector<std::unique_ptr<CaseExpressionBranchAst>>();
        case_branches.emplace_back(std::move(case_valid_branch));
        case_branches.emplace_back(std::move(case_else_branch));
        std::make_unique<CaseExpressionAst>(nullptr, ast_clone(resume_name), nullptr, std::move(case_branches));
    });

    // New boolean loop that manually iterates the iterable.
    auto loop_new = ( {
        auto cond = BooleanLiteralAst::True(pos_start());
        std::make_unique<LoopConditionalExpressionAst>(nullptr, std::move(cond), nullptr, nullptr);
    });
    loop_new->body->members.emplace_back(std::move(resume_let));
    loop_new->body->members.emplace_back(std::move(case_expr));
    m_transform_loop = std::move(loop_new);

    // Analyse the initial let statement.
    iterable_let->stage_7_analyse_semantics(sm, meta);
    m_transform_let = std::move(iterable_let);

    // Analyse the transformed loop (makes its own scope).
    meta->save();
    meta->current_loop_depth += 1;
    meta->current_loop_ast = this;
    m_transform_loop->stage_7_analyse_semantics(sm, meta);
    if (meta->loop_return_types->contains(meta->current_loop_depth - 1)) {
        m_loop_exit_type_info = (*meta->loop_return_types)[meta->current_loop_depth - 1];
    }
    meta->restore();
}


auto spp::asts::LoopIterableExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Call the memory check on the transformed loop.
    m_transform_let->stage_8_check_memory(sm, meta);
    m_transform_loop->stage_8_check_memory(sm, meta);
}


auto spp::asts::LoopIterableExpressionAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the code for the transformed loop.
    m_transform_let->stage_11_code_gen_2(sm, meta, ctx);
    return m_transform_loop->stage_11_code_gen_2(sm, meta, ctx);
}
