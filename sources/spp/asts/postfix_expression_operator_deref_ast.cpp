module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.asts.utils;


spp::asts::PostfixExpressionOperatorDerefAst::PostfixExpressionOperatorDerefAst(
    decltype(tok_deref) &&tok_deref) :
    tok_deref(std::move(tok_deref)) {
}


spp::asts::PostfixExpressionOperatorDerefAst::~PostfixExpressionOperatorDerefAst() = default;


auto spp::asts::PostfixExpressionOperatorDerefAst::pos_start() const
    -> std::size_t {
    return tok_deref->pos_start();
}


auto spp::asts::PostfixExpressionOperatorDerefAst::pos_end() const
    -> std::size_t {
    return tok_deref->pos_end();
}


auto spp::asts::PostfixExpressionOperatorDerefAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionOperatorDerefAst>(
        ast_clone(tok_deref));
}


spp::asts::PostfixExpressionOperatorDerefAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_deref);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorDerefAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Get the right-hand-side expression's type for constraint checks.
    const auto lhs = meta->postfix_expression_lhs;
    const auto lhs_type = lhs->infer_type(sm, meta);

    // Check the right-hand-side expression is a borrowable type.
    raise_if<analyse::errors::SppDereferenceInvalidExpressionNonBorrowedTypeError>(
        lhs_type->get_convention() == nullptr,
        {sm->current_scope}, ERR_ARGS(*tok_deref, *lhs, *lhs_type));

    // Check the right-hand-side expression is a "Copy" type.
    raise_if<analyse::errors::SppInvalidExpressionNonCopyableTypeError>(
        not sm->current_scope->get_type_symbol(lhs_type)->is_copyable() and not meta->allow_move_deref,
        {sm->current_scope}, ERR_ARGS(*lhs, *lhs_type));
}


auto spp::asts::PostfixExpressionOperatorDerefAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // As this is cmp context, just return the "lhs" generation.
    meta->postfix_expression_lhs->stage_9_comptime_resolution(sm, meta);
}


auto spp::asts::PostfixExpressionOperatorDerefAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // Get the value underlying the borrow.
    const auto borrow_val = meta->postfix_expression_lhs->stage_11_code_gen_2(sm, meta, ctx);
    SPP_ASSERT(borrow_val != nullptr);

    // Dereference the borrow to get the underlying value.
    const auto deref_val = ctx->builder.CreateLoad(borrow_val->getType(), borrow_val, "deref");
    return deref_val;
}


auto spp::asts::PostfixExpressionOperatorDerefAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the right-hand-side expression's type.
    const auto lhs = meta->postfix_expression_lhs;
    const auto lhs_type = lhs->infer_type(sm, meta);

    // Return the dereferenced type.
    return ast_clone(lhs_type->without_convention());
}
