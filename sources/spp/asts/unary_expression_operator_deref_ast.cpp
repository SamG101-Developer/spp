#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/unary_expression_operator_deref_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::UnaryExpressionOperatorDerefAst::UnaryExpressionOperatorDerefAst(
    decltype(tok_deref) &&tok_deref) :
    tok_deref(std::move(tok_deref)) {
}


spp::asts::UnaryExpressionOperatorDerefAst::~UnaryExpressionOperatorDerefAst() = default;


auto spp::asts::UnaryExpressionOperatorDerefAst::pos_start() const -> std::size_t {
    return tok_deref->pos_start();
}


auto spp::asts::UnaryExpressionOperatorDerefAst::pos_end() const -> std::size_t {
    return tok_deref->pos_end();
}


auto spp::asts::UnaryExpressionOperatorDerefAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<UnaryExpressionOperatorDerefAst>(
        ast_clone(tok_deref));
}


spp::asts::UnaryExpressionOperatorDerefAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_deref);
    SPP_STRING_END;
}


auto spp::asts::UnaryExpressionOperatorDerefAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_deref);
    SPP_PRINT_END;
}


auto spp::asts::UnaryExpressionOperatorDerefAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Get the right-hand-side expression's type for constraint checks.
    const auto rhs = meta->unary_expression_rhs;
    const auto rhs_type = rhs->infer_type(sm, meta);

    // Check the right-hand-side expression is a borrowable type.
    if (rhs_type->get_convention() == nullptr) {
        analyse::errors::SppDereferenceInvalidExpressionNonBorrowableTypeError(*tok_deref, *rhs, *rhs_type).scopes({sm->current_scope}).raise();
    }

    // Check the right-hand-side expression is a "Copy" type.
    if (not sm->current_scope->get_type_symbol(*rhs_type)->is_copyable()) {
        analyse::errors::SppDereferenceInvalidExpressionNonCopyableTypeError(*tok_deref, *rhs, *rhs_type).scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::UnaryExpressionOperatorDerefAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the right-hand-side expression's type.
    const auto rhs = meta->unary_expression_rhs;
    const auto rhs_type = rhs->infer_type(sm, meta);

    // Return the dereferenced type.
    return ast_clone(rhs_type->without_convention());
}
