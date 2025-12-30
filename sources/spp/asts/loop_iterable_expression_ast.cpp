module;
#include <spp/macros.hpp>

module spp.asts.loop_iterable_expression_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.asts.boolean_literal_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.iter_expression_ast;
import spp.asts.iter_expression_branch_ast;
import spp.asts.iter_pattern_variant_else_ast;
import spp.asts.iter_pattern_variant_exhausted_ast;
import spp.asts.iter_pattern_variant_variable_ast;
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


auto spp::asts::LoopIterableExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_loop).append(" ");
    SPP_PRINT_APPEND(var).append(" ");
    SPP_PRINT_APPEND(tok_in).append(" ");
    SPP_PRINT_APPEND(iterable).append(" ");
    SPP_PRINT_APPEND(body).append("\n");
    SPP_PRINT_APPEND(else_block);
    SPP_PRINT_END;
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

    // Create the initial let statement to materialize the condition being iterated.
    auto iterable_let = ( {
        auto iterable_var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, iterable_name, nullptr);
        auto iterable_val = std::move(iterable);
        std::make_unique<LetStatementInitializedAst>(nullptr, std::move(iterable_var), nullptr, nullptr, std::move(iterable_val));
    });

    // Create the "let" statement that increments the iterator.
    auto resume_let = ( {
        auto resume_var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, resume_name, nullptr);
        auto resume_val_op = std::make_unique<PostfixExpressionOperatorKeywordResAst>(nullptr, nullptr, nullptr);
        auto resume_val = std::make_unique<PostfixExpressionAst>(ast_clone(iterable_name), std::move(resume_val_op));
        std::make_unique<LetStatementInitializedAst>(nullptr, std::move(resume_var), nullptr, nullptr, std::move(resume_val));
    });

    // Iter:Else => skip over GenOpt no-value and GenRes error yields.
    auto iter_else_branch = ( {
        auto iter_else_pattern = std::make_unique<IterPatternVariantElseAst>(nullptr);
        auto iter_else_body = InnerScopeExpressionAst<std::unique_ptr<StatementAst>>::new_empty();
        iter_else_body->members.emplace_back(std::move(skip_stmt));
        std::make_unique<IterExpressionBranchAst>(std::move(iter_else_pattern), nullptr, std::move(iter_else_body));
    });

    // Iter:Exhausted AND first => run the else block once, then exit the loop.
    // TODO

    // Iter:Exhausted => exit the loop, as iteration has finished.
    auto iter_exhausted_branch = ( {
        auto iter_exhausted_pattern = std::make_unique<IterPatternVariantExhaustedAst>(nullptr);
        auto iter_exhausted_body = InnerScopeExpressionAst<std::unique_ptr<StatementAst>>::new_empty();
        iter_exhausted_body->members.emplace_back(std::move(exit_stmt));
        std::make_unique<IterExpressionBranchAst>(std::move(iter_exhausted_pattern), nullptr, std::move(iter_exhausted_body));
    });

    // Iter:Variable => bind to the same variable as in the original loop.
    auto iter_var_branch = ( {
        auto iter_var_pattern = std::make_unique<IterPatternVariantVariableAst>(std::move(var));
        auto iter_var_body = std::move(body);
        std::make_unique<IterExpressionBranchAst>(std::move(iter_var_pattern), nullptr, std::move(iter_var_body));
    });

    // Iter block to handle the resume value.
    auto iter_expr = ( {
        auto iter_branches = std::vector<std::unique_ptr<IterExpressionBranchAst>>();
        iter_branches.emplace_back(std::move(iter_var_branch));
        iter_branches.emplace_back(std::move(iter_exhausted_branch));
        iter_branches.emplace_back(std::move(iter_else_branch));
        std::make_unique<IterExpressionAst>(nullptr, ast_clone(resume_name), nullptr, std::move(iter_branches));
    });

    // New boolean loop that manually iterates the iterable.
    auto loop_new = ( {
        auto cond = BooleanLiteralAst::True(pos_start());
        std::make_unique<LoopConditionalExpressionAst>(nullptr, std::move(cond), nullptr, nullptr);
    });
    loop_new->body->members.emplace_back(std::move(resume_let));
    loop_new->body->members.emplace_back(std::move(iter_expr));
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


auto spp::asts::LoopIterableExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the code for the transformed loop.
    m_transform_let->stage_10_code_gen_2(sm, meta, ctx);
    return m_transform_loop->stage_10_code_gen_2(sm, meta, ctx);
}
