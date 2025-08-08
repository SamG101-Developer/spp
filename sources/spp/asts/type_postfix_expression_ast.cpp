#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_postfix_expression_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_nested_type_ast.hpp>


spp::asts::TypePostfixExpressionAst::TypePostfixExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op):
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)) {
}


auto spp::asts::TypePostfixExpressionAst::pos_start() const -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::TypePostfixExpressionAst::pos_end() const -> std::size_t {
    return tok_op->pos_end();
}


spp::asts::TypePostfixExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_END;
}


auto spp::asts::TypePostfixExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_END;
}


auto spp::asts::TypePostfixExpressionAst::is_never_type() const -> bool {
    return false;
}


auto spp::asts::TypePostfixExpressionAst::ns_parts() const -> std::vector<IdentifierAst const*> {
    auto lhs_parts = lhs->ns_parts();
    auto rhs_parts = tok_op->ns_parts();
    lhs_parts.insert(lhs_parts.end(), rhs_parts.begin(), rhs_parts.end());
    return lhs_parts;
}


auto spp::asts::TypePostfixExpressionAst::type_parts() const -> std::vector<TypeIdentifierAst const*> {
    auto lhs_parts = lhs->type_parts();
    auto rhs_parts = tok_op->type_parts();
    lhs_parts.insert(lhs_parts.end(), rhs_parts.begin(), rhs_parts.end());
    return lhs_parts;
}


auto spp::asts::TypePostfixExpressionAst::without_convention() const -> TypeAst const* {
    return ast_cast<TypeAst>(this);
}


auto spp::asts::TypePostfixExpressionAst::get_convention() const -> ConventionAst* {
    return nullptr;
}


auto spp::asts::TypePostfixExpressionAst::with_convention(std::unique_ptr<ConventionAst> &&) const -> std::unique_ptr<TypeAst> {
    return nullptr;
}


auto spp::asts::TypePostfixExpressionAst::without_generics() const -> std::unique_ptr<TypeAst> {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    auto new_lhs = lhs->without_generics();
    auto new_rhs = std::make_unique<TypePostfixExpressionOperatorNestedTypeAst>(nullptr, ast_cast<TypeIdentifierAst>(rhs->name->without_generics()));
    return std::make_unique<TypePostfixExpressionAst>(std::move(new_lhs), std::move(new_rhs));
}


auto spp::asts::TypePostfixExpressionAst::substitute_generics(std::vector<GenericArgumentAst*> args) const -> std::unique_ptr<TypeAst> {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    auto new_lhs = lhs->substitute_generics(args);
    auto new_rhs = std::make_unique<TypePostfixExpressionOperatorNestedTypeAst>(nullptr, ast_cast<TypeIdentifierAst>(rhs->name->substitute_generics(std::move(args))));
    return std::make_unique<TypePostfixExpressionAst>(std::move(new_lhs), std::move(new_rhs));
}


auto spp::asts::TypePostfixExpressionAst::contains_generic(TypeAst const *generic) const -> bool {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    return rhs->name->contains_generic(generic);
}


auto spp::asts::TypePostfixExpressionAst::match_generic(TypeAst const *other, TypeAst const *generic) -> TypeAst* {
    if (static_cast<std::string>(*other) == static_cast<std::string>(*this)) { return this; }
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    return rhs->name->match_generic(other, generic);
}


auto spp::asts::TypePostfixExpressionAst::with_generics(std::unique_ptr<GenericArgumentGroupAst> &&) -> std::unique_ptr<TypeAst> {
    return nullptr;
}

