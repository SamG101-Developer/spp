module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_deref_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


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


auto spp::asts::PostfixExpressionOperatorDerefAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_deref);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorDerefAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Get the right-hand-side expression's type for constraint checks.
    const auto lhs = meta->postfix_expression_lhs;
    const auto lhs_type = lhs->infer_type(sm, meta);

    // Check the right-hand-side expression is a borrowable type.
    if (lhs_type->get_convention() == nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppDereferenceInvalidExpressionNonBorrowedTypeError>()
            .with_args(*tok_deref, *lhs, *lhs_type)
            .raises_from(sm->current_scope);
    }

    // Check the right-hand-side expression is a "Copy" type.
    if (not sm->current_scope->get_type_symbol(lhs_type)->is_copyable() and not meta->allow_move_deref) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidExpressionNonCopyableTypeError>()
            .with_args(*lhs, *lhs_type)
            .raises_from(sm->current_scope);
    }
}


auto spp::asts::PostfixExpressionOperatorDerefAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Not implemented yet.
    (void)sm;
    (void)meta;
    (void)ctx;
    throw std::runtime_error("PostfixExpressionOperatorDerefAst::stage_10_code_gen_2 not implemented yet");
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
