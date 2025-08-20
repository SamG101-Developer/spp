#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_postfix_expression_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_nested_type_ast.hpp>
#include <spp/asts/type_unary_expression_ast.hpp>
#include <spp/asts/type_unary_expression_operator_borrow_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>

#include <genex/views/concat.hpp>


spp::asts::TypePostfixExpressionAst::TypePostfixExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op) :
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)) {
}


auto spp::asts::TypePostfixExpressionAst::pos_start() const -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::TypePostfixExpressionAst::pos_end() const -> std::size_t {
    return tok_op->pos_end();
}


auto spp::asts::TypePostfixExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<TypePostfixExpressionAst>(
        ast_clone(lhs),
        ast_clone(tok_op));
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


auto spp::asts::TypePostfixExpressionAst::iterator(
    ) const
    -> genex::generator<std::shared_ptr<TypeIdentifierAst>> {
    // Iterate from the left-hand-side.
    return lhs->iterator();
}


auto spp::asts::TypePostfixExpressionAst::is_never_type(
    ) const
    -> bool {
    return false;
}


auto spp::asts::TypePostfixExpressionAst::ns_parts(
    ) const
    -> std::vector<std::shared_ptr<const IdentifierAst>> {
    // Concatenate the lhs and rhs namespace parts.
    return genex::views::concat(lhs->ns_parts(), tok_op->ns_parts()) | genex::views::to<std::vector>();
}


auto spp::asts::TypePostfixExpressionAst::type_parts(
    ) const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    // Concatenate the lhs and rhs type parts.
    return genex::views::concat(lhs->type_parts(), tok_op->type_parts()) | genex::views::to<std::vector>();
}


auto spp::asts::TypePostfixExpressionAst::without_convention(
    ) const
    -> std::shared_ptr<const TypeAst> {
    return shared_from_this();
}


auto spp::asts::TypePostfixExpressionAst::get_convention(
    ) const
    -> ConventionAst* {
    // This type AST will never have a convention directly applied to it.
    return nullptr;
}


auto spp::asts::TypePostfixExpressionAst::with_convention(
    std::unique_ptr<ConventionAst> &&conv) const
    -> std::unique_ptr<TypeAst> {
    auto borrow_op = std::make_unique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv));
    auto wrapped = std::make_unique<TypeUnaryExpressionAst>(std::move(borrow_op), ast_clone(this));
    return wrapped;
}


auto spp::asts::TypePostfixExpressionAst::without_generics(
    ) const
    -> std::unique_ptr<TypeAst> {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    auto new_lhs = lhs->without_generics();
    auto new_rhs = std::make_unique<TypePostfixExpressionOperatorNestedTypeAst>(nullptr, ast_cast<TypeIdentifierAst>(rhs->name->without_generics()));
    return std::make_unique<TypePostfixExpressionAst>(std::move(new_lhs), std::move(new_rhs));
}


auto spp::asts::TypePostfixExpressionAst::substitute_generics(
    std::vector<GenericArgumentAst*> args) const
    -> std::unique_ptr<TypeAst> {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    auto new_lhs = lhs->substitute_generics(args);
    auto new_rhs = std::make_unique<TypePostfixExpressionOperatorNestedTypeAst>(nullptr, ast_cast<TypeIdentifierAst>(rhs->name->substitute_generics(std::move(args))));
    return std::make_unique<TypePostfixExpressionAst>(std::move(new_lhs), std::move(new_rhs));
}


auto spp::asts::TypePostfixExpressionAst::contains_generic(
    GenericParameterAst const &generic) const
    -> bool {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    return rhs->name->contains_generic(generic);
}


auto spp::asts::TypePostfixExpressionAst::match_generic(
    TypeAst const &other,
    TypeIdentifierAst const &generic_name) const
    -> const ExpressionAst* {
    if (static_cast<std::string>(*this) == static_cast<std::string>(other)) { return this; }
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    return rhs->name->match_generic(other, generic_name);
}


auto spp::asts::TypePostfixExpressionAst::with_generics(
    std::shared_ptr<GenericArgumentGroupAst> &&arg_group) const
    -> std::unique_ptr<TypeAst> {
    // Clone this type and add the generics to the right most part.
    auto type_clone = ast_clone(this);
    type_clone->type_parts().back()->generic_arg_group->args = std::move(arg_group->args);
    return type_clone;
}
