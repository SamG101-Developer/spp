#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_unary_expression_ast.hpp>
#include <spp/asts/type_unary_expression_operator_ast.hpp>
#include <spp/asts/type_unary_expression_operator_borrow_ast.hpp>
#include <spp/asts/type_unary_expression_operator_namespace_ast.hpp>

#include <genex/views/concat.hpp>

// static auto COUNTER = 0;

spp::asts::TypeUnaryExpressionAst::TypeUnaryExpressionAst(
    decltype(op) op,
    decltype(rhs) rhs) :
    op(std::move(op)),
    rhs(std::move(rhs)) {
}


spp::asts::TypeUnaryExpressionAst::~TypeUnaryExpressionAst() = default;


auto spp::asts::TypeUnaryExpressionAst::operator<=>(
    TypeUnaryExpressionAst const &other) const
    -> std::strong_ordering {
    return equals(other);
}


auto spp::asts::TypeUnaryExpressionAst::operator==(
    TypeUnaryExpressionAst const &other) const
    -> bool {
    return equals(other) == std::strong_ordering::equal;
}


auto spp::asts::TypeUnaryExpressionAst::pos_start() const -> std::size_t {
    return op->pos_start();
}


auto spp::asts::TypeUnaryExpressionAst::pos_end() const -> std::size_t {
    return rhs->pos_end();
}


auto spp::asts::TypeUnaryExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<TypeUnaryExpressionAst>(
        ast_clone(op),
        ast_clone(rhs));
}


spp::asts::TypeUnaryExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(op);
    SPP_STRING_APPEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::TypeUnaryExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(op);
    SPP_PRINT_APPEND(rhs);
    SPP_PRINT_END;
}


auto spp::asts::TypeUnaryExpressionAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    // Double dispatch to the appropriate equals method.
    return other.equals_type_unary_expression(*this);
}


auto spp::asts::TypeUnaryExpressionAst::equals_type_unary_expression(
    TypeUnaryExpressionAst const &other) const
    -> std::strong_ordering {
    // Check if the operators and right-hand-sides are the same.
    if (*op == *other.op && *rhs == *other.rhs) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::TypeUnaryExpressionAst::iterator() const
    -> genex::generator<std::shared_ptr<const TypeIdentifierAst>> {
    // Iterate from the right-hand-side.
    return rhs->iterator();
}


auto spp::asts::TypeUnaryExpressionAst::is_never_type() const
    -> bool {
    return false;
}


auto spp::asts::TypeUnaryExpressionAst::ns_parts() const
    -> std::vector<std::shared_ptr<const IdentifierAst>> {
    auto a = const_cast<const TypeUnaryExpressionOperatorAst*>(op.get())->ns_parts();
    auto b = const_cast<const TypeAst*>(rhs.get())->ns_parts();
    return genex::views::concat(std::move(a), std::move(b)) | genex::views::to<std::vector>();
}


auto spp::asts::TypeUnaryExpressionAst::ns_parts()
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return genex::views::concat(op->ns_parts(), rhs->ns_parts()) | genex::views::to<std::vector>();
}


auto spp::asts::TypeUnaryExpressionAst::type_parts() const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    auto a = const_cast<const TypeUnaryExpressionOperatorAst*>(op.get())->type_parts();
    auto b = const_cast<const TypeAst*>(rhs.get())->type_parts();
    return genex::views::concat(std::move(a), std::move(b)) | genex::views::to<std::vector>();
}


auto spp::asts::TypeUnaryExpressionAst::type_parts()
    -> std::vector<std::shared_ptr<TypeIdentifierAst>> {
    return genex::views::concat(op->type_parts(), rhs->type_parts()) | genex::views::to<std::vector>();
}


auto spp::asts::TypeUnaryExpressionAst::without_convention() const
    -> std::shared_ptr<const TypeAst> {
    if (ast_cast<TypeUnaryExpressionOperatorBorrowAst>(op.get())) {
        return rhs;
    }
    return std::dynamic_pointer_cast<const TypeAst>(shared_from_this());
}


auto spp::asts::TypeUnaryExpressionAst::get_convention() const
    -> ConventionAst* {
    if (auto const *op_borrow = ast_cast<TypeUnaryExpressionOperatorBorrowAst>(op.get())) {
        return op_borrow->conv.get();
    }
    return nullptr;
}


auto spp::asts::TypeUnaryExpressionAst::with_convention(
    std::unique_ptr<ConventionAst> &&conv) const
    -> std::shared_ptr<TypeAst> {
    if (conv == nullptr) {
        return std::make_shared<TypeUnaryExpressionAst>(ast_clone(op), ast_clone(rhs));
    }
    if (ast_cast<TypeUnaryExpressionOperatorBorrowAst>(op.get())) {
        return std::make_shared<TypeUnaryExpressionAst>(
            std::make_unique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv)),
            ast_clone(rhs));
    }
    return std::make_shared<TypeUnaryExpressionAst>(
        std::make_unique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv)),
        ast_clone(this));
}


auto spp::asts::TypeUnaryExpressionAst::without_generics() const
    -> std::shared_ptr<TypeAst> {
    return std::make_shared<TypeUnaryExpressionAst>(op, rhs->without_generics());
}


auto spp::asts::TypeUnaryExpressionAst::substitute_generics(
    std::vector<GenericArgumentAst*> const &args) const
    -> std::shared_ptr<TypeAst> {
    return std::make_shared<TypeUnaryExpressionAst>(ast_clone(op), rhs->substitute_generics(args));
}


auto spp::asts::TypeUnaryExpressionAst::contains_generic(
    GenericParameterAst const &generic) const
    -> bool {
    return rhs->contains_generic(generic);
}


auto spp::asts::TypeUnaryExpressionAst::with_generics(
    std::unique_ptr<GenericArgumentGroupAst> &&arg_group) const
    -> std::shared_ptr<TypeAst> {
    // Clone this type and add the generics to the right most part.
    auto type_clone = ast_clone(this);
    type_clone->type_parts().back()->generic_arg_group = std::move(arg_group);
    return type_clone;
}


auto spp::asts::TypeUnaryExpressionAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Qualify the RHS type.
    if (const auto op_ns = ast_cast<TypeUnaryExpressionOperatorNamespaceAst>(op.get())) {
        const auto tm = ScopeManager(sm->global_scope, meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope);
        const auto type_scope = analyse::utils::type_utils::get_namespaced_scope_with_error(tm, *op_ns->ns);
        meta->save();
        meta->type_analysis_type_scope = type_scope;
        rhs->stage_4_qualify_types(sm, meta);
        meta->restore();
    }
    else {
        rhs->stage_4_qualify_types(sm, meta);
    }
}


auto spp::asts::TypeUnaryExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the RHS type.
    if (const auto op_ns = ast_cast<TypeUnaryExpressionOperatorNamespaceAst>(op.get())) {
        const auto tm = ScopeManager(sm->global_scope, meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope);
        const auto type_scope = analyse::utils::type_utils::get_namespaced_scope_with_error(tm, *op_ns->ns);
        meta->save();
        meta->type_analysis_type_scope = type_scope;
        rhs->stage_7_analyse_semantics(sm, meta);
        meta->restore();
    }
    else {
        rhs->stage_7_analyse_semantics(sm, meta);
    }
}


auto spp::asts::TypeUnaryExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the RHS type.
    const auto type_scope = meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope;
    const auto type_sym = type_scope->get_type_symbol(shared_from_this());
    return type_sym->fq_name()->with_convention(ast_clone(get_convention()));
}
