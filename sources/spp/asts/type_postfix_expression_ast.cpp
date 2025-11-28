module;
#include <genex/to_container.hpp>
#include <genex/algorithms/min_element.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/transform.hpp>

#include <spp/macros.hpp>

module spp.asts.type_postfix_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_postfix_expression_operator_ast;
import spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;


spp::asts::TypePostfixExpressionAst::TypePostfixExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op) :
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)) {
}


spp::asts::TypePostfixExpressionAst::~TypePostfixExpressionAst() = default;


auto spp::asts::TypePostfixExpressionAst::equals(
    const ExpressionAst &other) const
    -> std::strong_ordering {
    // Double dispatch to the appropriate equals method.
    return other.equals_type_postfix_expression(*this);
}


auto spp::asts::TypePostfixExpressionAst::equals_type_postfix_expression(
    TypePostfixExpressionAst const &other) const
    -> std::strong_ordering {
    // Check the lhs and operator are the same.
    if (*lhs == *other.lhs && *tok_op == *other.tok_op) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::TypePostfixExpressionAst::pos_start() const
    -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::TypePostfixExpressionAst::pos_end() const
    -> std::size_t {
    return tok_op->pos_end();
}


auto spp::asts::TypePostfixExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
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


auto spp::asts::TypePostfixExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_END;
}


auto spp::asts::TypePostfixExpressionAst::iterator() const
    -> genex::generator<std::shared_ptr<const TypeIdentifierAst>> {
    // Iterate from the left-hand-side.
    return lhs->iterator();
}


auto spp::asts::TypePostfixExpressionAst::is_never_type() const
    -> bool {
    return false;
}


auto spp::asts::TypePostfixExpressionAst::ns_parts() const
    -> std::vector<std::shared_ptr<const IdentifierAst>> {
    // Concatenate the lhs and rhs namespace parts.
    auto parts = std::const_pointer_cast<const TypeAst>(lhs)->ns_parts();
    parts.append_range(tok_op->ns_parts());
    return parts;
}


auto spp::asts::TypePostfixExpressionAst::ns_parts()
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    // Concatenate the lhs and rhs namespace parts.
    auto parts = lhs->ns_parts();
    parts.append_range(tok_op->ns_parts());
    return parts;
}


auto spp::asts::TypePostfixExpressionAst::type_parts() const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    // Concatenate the lhs and rhs type parts.
    auto parts = std::const_pointer_cast<const TypeAst>(lhs)->type_parts();
    parts.append_range(tok_op->type_parts());
    return parts;
}


auto spp::asts::TypePostfixExpressionAst::type_parts()
    -> std::vector<std::shared_ptr<TypeIdentifierAst>> {
    // Concatenate the lhs and rhs type parts.
    auto parts = lhs->type_parts();
    parts.append_range(tok_op->type_parts());
    return parts;
}


auto spp::asts::TypePostfixExpressionAst::without_convention() const
    -> std::shared_ptr<const TypeAst> {
    return shared_from_this();
}


auto spp::asts::TypePostfixExpressionAst::get_convention() const
    -> ConventionAst* {
    // This type AST will never have a convention directly applied to it.
    return nullptr;
}


auto spp::asts::TypePostfixExpressionAst::with_convention(
    std::unique_ptr<ConventionAst> &&conv) const
    -> std::shared_ptr<TypeAst> {
    if (conv == nullptr) { return const_cast<TypePostfixExpressionAst*>(this)->shared_from_this(); }
    auto borrow_op = std::make_unique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv));
    auto wrapped = std::make_shared<TypeUnaryExpressionAst>(std::move(borrow_op), ast_clone(this));
    return wrapped;
}


auto spp::asts::TypePostfixExpressionAst::without_generics() const
    -> std::shared_ptr<TypeAst> {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    auto new_lhs = ast_clone(lhs); // Todo: clone needed?
    auto new_rhs = std::make_unique<TypePostfixExpressionOperatorNestedTypeAst>(nullptr, std::dynamic_pointer_cast<TypeIdentifierAst>(rhs->name->without_generics()));
    return std::make_shared<TypePostfixExpressionAst>(std::move(new_lhs), std::move(new_rhs));
}


auto spp::asts::TypePostfixExpressionAst::substitute_generics(
    std::vector<GenericArgumentAst*> const &args) const
    -> std::shared_ptr<TypeAst> {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    auto new_lhs = lhs->substitute_generics(args);
    auto new_rhs = std::make_unique<TypePostfixExpressionOperatorNestedTypeAst>(
        nullptr, std::dynamic_pointer_cast<TypeIdentifierAst>(rhs->name->substitute_generics(std::move(args))));
    return std::make_shared<TypePostfixExpressionAst>(std::move(new_lhs), std::move(new_rhs));
}


auto spp::asts::TypePostfixExpressionAst::contains_generic(
    GenericParameterAst const &generic) const
    -> bool {
    const auto rhs = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    return rhs->name->contains_generic(generic);
}


auto spp::asts::TypePostfixExpressionAst::with_generics(
    std::unique_ptr<GenericArgumentGroupAst> &&arg_group) const
    -> std::shared_ptr<TypeAst> {
    // Clone this type and add the generics to the right most part.
    auto type_clone = ast_clone(this);
    type_clone->type_parts().back()->generic_arg_group = std::move(arg_group);
    return type_clone;
}


auto spp::asts::TypePostfixExpressionAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    (void)sm;
    (void)meta;
    // TODO ?
}


auto spp::asts::TypePostfixExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move through the left-hand-side type.
    lhs->stage_7_analyse_semantics(sm, meta);
    const auto scope = meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope;
    const auto lhs_type = lhs->infer_type(sm, meta);
    const auto lhs_type_sym = scope->get_type_symbol(lhs_type);

    // Check the left-hand-side isn't a generic type. Todo: until constraints
    if (lhs_type_sym->is_generic) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppGenericTypeInvalidUsageError>().with_args(
            *lhs, *lhs_type, "postfix type").with_scopes({sm->current_scope}).raise();
    }

    const auto lhs_type_scope = lhs_type_sym->scope;

    // Check there is only 1 target field on the lhs at the highest level.
    const auto op_nested = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    auto scopes_and_syms = std::vector{lhs_type_sym->scope}
        | genex::views::concat(lhs_type_sym->scope->sup_scopes())
        | genex::views::transform([name=op_nested->name.get()](auto &&x) { return std::make_pair(x, x->table.type_tbl.get(ast_clone(name))); })
        | genex::views::filter([](auto &&x) { return x.second != nullptr; })
        | genex::views::transform([lhs_type_sym](auto &&x) { return std::make_tuple(lhs_type_sym->scope->depth_difference(x.first), x.first, x.second); })
        | genex::to<std::vector>();

    auto min_depth = scopes_and_syms.empty() ? 0 : genex::algorithms::min_element(scopes_and_syms
                             | genex::views::transform([](auto &&x) { return std::get<0>(x); })
                             | genex::to<std::vector>());

    auto closest = scopes_and_syms
        | genex::views::filter([min_depth](auto &&x) { return std::get<0>(x) == min_depth; })
        | genex::views::transform([](auto &&x) { return std::make_pair(std::get<1>(x), std::get<2>(x)); })
        | genex::to<std::vector>();

    if (closest.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppAmbiguousMemberAccessError>().with_args(
            *closest[0].second->name, *closest[1].second->name, *op_nested->name).with_scopes({closest[0].first, closest[1].first, sm->current_scope}).raise();
    }

    // Ensure the type exists on the "lhs" part.

    meta->save();
    meta->type_analysis_type_scope = lhs_type_scope;
    op_nested->name->stage_7_analyse_semantics(sm, meta);
    meta->restore();
}


auto spp::asts::TypePostfixExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type of the left-hand-side.
    lhs->stage_7_analyse_semantics(sm, meta);
    const auto lhs_type = lhs->infer_type(sm, meta);
    const auto lhs_type_sym = sm->current_scope->get_type_symbol(lhs_type);
    const auto lhs_type_scope = lhs_type_sym->scope;

    // Infer the type of the postfix operation.
    const auto op_nested = ast_cast<TypePostfixExpressionOperatorNestedTypeAst>(tok_op.get());
    const auto part = analyse::utils::type_utils::get_type_part_symbol_with_error(*lhs_type_scope, *op_nested->name, *sm, meta)->fq_name();
    const auto sym = lhs_type_scope->get_type_symbol(part);
    return sym->fq_name();
}
