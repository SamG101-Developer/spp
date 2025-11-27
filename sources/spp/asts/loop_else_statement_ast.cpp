module;
#include <spp/macros.hpp>

module spp.asts.loop_else_statement_ast;
import spp.analyse.scopes.scope_block_name;
import spp.asts.inner_scope_expression_ast;
import spp.asts.token_ast;


spp::asts::LoopElseStatementAst::LoopElseStatementAst(
    decltype(tok_else) &&tok_else,
    decltype(body) &&body) :
    tok_else(std::move(tok_else)),
    body(std::move(body)) {
}


spp::asts::LoopElseStatementAst::~LoopElseStatementAst() = default;


auto spp::asts::LoopElseStatementAst::pos_start() const
    -> std::size_t {
    return tok_else->pos_start();
}


auto spp::asts::LoopElseStatementAst::pos_end() const
    -> std::size_t {
    return tok_else->pos_end();
}


auto spp::asts::LoopElseStatementAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LoopElseStatementAst>(
        ast_clone(tok_else),
        ast_clone(body));
}


spp::asts::LoopElseStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_else);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::LoopElseStatementAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_else);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}


auto spp::asts::LoopElseStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a scope and analyse the body.
    auto scope_name = analyse::scopes::ScopeBlockName("<case-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    body->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopElseStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the body for memory issues.
    sm->move_out_of_current_scope();
    body->stage_8_check_memory(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopElseStatementAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // The type of an else statement is the type of its body.
    return body->infer_type(sm, meta);
}
