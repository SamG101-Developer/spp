module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_control_flow_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.loop_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.asts.generate.common_types;
import spp.lex.tokens;
import genex;


spp::asts::LoopControlFlowStatementAst::LoopControlFlowStatementAst(
    decltype(tok_seq_exit) &&tok_seq_exit,
    decltype(tok_skip) &&tok_skip,
    decltype(expr) &&expr) :
    StatementAst(),
    tok_seq_exit(std::move(tok_seq_exit)),
    tok_skip(std::move(tok_skip)),
    expr(std::move(expr)) {
}


spp::asts::LoopControlFlowStatementAst::~LoopControlFlowStatementAst() = default;


auto spp::asts::LoopControlFlowStatementAst::Skip(
    std::size_t pos)
    -> std::unique_ptr<LoopControlFlowStatementAst> {
    // No exit statements, one skip statement.
    auto exits = std::vector<std::unique_ptr<TokenAst>>();
    auto skip = std::make_unique<TokenAst>(pos, lex::SppTokenType::KW_SKIP, "skip");
    return std::make_unique<LoopControlFlowStatementAst>(std::move(exits), std::move(skip), nullptr);
}


auto spp::asts::LoopControlFlowStatementAst::Exit(
    std::size_t pos)
    -> std::unique_ptr<LoopControlFlowStatementAst> {
    // One exit statement, no skip statements.
    auto exits = std::vector<std::unique_ptr<TokenAst>>();
    exits.emplace_back(std::make_unique<TokenAst>(pos, lex::SppTokenType::KW_EXIT, "exit"));
    return std::make_unique<LoopControlFlowStatementAst>(std::move(exits), nullptr, nullptr);
}


auto spp::asts::LoopControlFlowStatementAst::pos_start() const
    -> std::size_t {
    return tok_seq_exit.empty() ? tok_skip->pos_start() : tok_seq_exit.front()->pos_start();
}


auto spp::asts::LoopControlFlowStatementAst::pos_end() const
    -> std::size_t {
    return expr ? expr->pos_end() : tok_skip ? tok_skip->pos_end() : tok_seq_exit.back()->pos_end();
}


auto spp::asts::LoopControlFlowStatementAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LoopControlFlowStatementAst>(
        ast_clone_vec(tok_seq_exit),
        ast_clone(tok_skip),
        ast_clone(expr));
}


spp::asts::LoopControlFlowStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(tok_seq_exit).append(" ");
    SPP_STRING_APPEND(tok_skip).append(" ");
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::LoopControlFlowStatementAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(tok_seq_exit).append(" ");
    SPP_PRINT_APPEND(tok_skip).append(" ");
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::LoopControlFlowStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Get the number of control flow statements, and the loop's nesting level.
    const auto has_skip = tok_skip != nullptr;
    const auto num_controls = tok_seq_exit.size() + (has_skip ? 1 : 0);
    const auto nested_loop_depth = meta->current_loop_depth;

    // Analyse the expression if it is present.
    if (expr != nullptr) {
        SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    }

    // Check the depth of the loop is greater than or equal to the number of control statements.
    if (num_controls > nested_loop_depth) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppLoopTooManyControlFlowStatementsError>()
            .with_args(*meta->current_loop_ast->tok_loop, *this, num_controls, nested_loop_depth)
            .raises_from(sm->current_scope);
    }

    // Save and compare the loop's "exiting" type against other nested loop's exit statement types.
    if (not has_skip) {
        auto expr_type = generate::common_types::void_type(pos_start());
        if (expr != nullptr) {
            expr->stage_7_analyse_semantics(sm, meta);
            expr_type = expr->infer_type(sm, meta);
        }

        // Insert or check the depth's corresponding exit type.
        const auto depth = nested_loop_depth - num_controls;
        if (meta->loop_return_types->contains(depth)) {
            // If the type is already set, check it matches the current expression's type.
            auto [that_expr, that_expr_type, that_scope] = meta->loop_return_types->at(depth);
            if (not analyse::utils::type_utils::symbolic_eq(*expr_type, *that_expr_type, *sm->current_scope, *that_scope)) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
                    .with_args(*expr, *expr_type, *that_expr, *that_expr_type)
                    .raises_from(sm->current_scope, that_scope);
            }
        }
        else {
            auto stored_expr = expr ? expr.get() : nullptr;
            (*meta->loop_return_types)[depth] = std::make_tuple(stored_expr, expr_type, sm->current_scope);
        }
    }
}


auto spp::asts::LoopControlFlowStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the expression if it is present. Expression is being moved into outer context, so
    // strict memory checks.
    if (expr != nullptr) {
        expr->stage_8_check_memory(sm, meta);
        analyse::utils::mem_utils::validate_symbol_memory(
            *expr, *tok_seq_exit.back(), *sm, true, true, true, true, true, true, meta);
    }
}


auto spp::asts::LoopControlFlowStatementAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // For "exit" statements, we need to branch to the end bb of N loops, N being the number of exit tokens.
    // TODO

    // For "skip" statements, we need to branch to the condition check bb of the innermost loop.
    // TODO

    // If there is an attached expression, code generate it.
    return expr ? expr->stage_10_code_gen_2(sm, meta, ctx) : nullptr;
}


auto spp::asts::LoopControlFlowStatementAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // If there is an attached expression, return its type.
    if (expr != nullptr) {
        return expr->infer_type(sm, meta);
    }

    // Otherwise, return the void type.
    return generate::common_types::void_type(pos_start());
}
