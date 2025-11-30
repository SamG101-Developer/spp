module;
#include <spp/macros.hpp>

module spp.asts.type_unary_expression_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_unary_expression_operator_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::TypeUnaryExpressionAst::TypeUnaryExpressionAst(
    decltype(op) op,
    decltype(rhs) rhs) :
    op(std::move(op)),
    rhs(std::move(rhs)) {
}


spp::asts::TypeUnaryExpressionAst::~TypeUnaryExpressionAst() = default;


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


auto spp::asts::TypeUnaryExpressionAst::pos_start() const
    -> std::size_t {
    return op->pos_start();
}


auto spp::asts::TypeUnaryExpressionAst::pos_end() const
    -> std::size_t {
    return rhs->pos_end();
}


auto spp::asts::TypeUnaryExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
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


auto spp::asts::TypeUnaryExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(op);
    SPP_PRINT_APPEND(rhs);
    SPP_PRINT_END;
}


auto spp::asts::TypeUnaryExpressionAst::iterator() const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    // Iterate from the right-hand-side.
    return rhs->iterator();
}


auto spp::asts::TypeUnaryExpressionAst::is_never_type() const
    -> bool {
    return false;
}


auto spp::asts::TypeUnaryExpressionAst::ns_parts() const
    -> std::vector<std::shared_ptr<const IdentifierAst>> {
    auto parts = std::const_pointer_cast<const TypeUnaryExpressionOperatorAst>(op)->ns_parts();
    parts.append_range(rhs->ns_parts());
    return parts;
}


auto spp::asts::TypeUnaryExpressionAst::ns_parts()
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    auto parts = op->ns_parts();
    parts.append_range(rhs->ns_parts());
    return parts;
}


auto spp::asts::TypeUnaryExpressionAst::type_parts() const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    auto parts = std::const_pointer_cast<const TypeUnaryExpressionOperatorAst>(op)->type_parts();
    parts.append_range(rhs->type_parts());
    return parts;
}


auto spp::asts::TypeUnaryExpressionAst::type_parts()
    -> std::vector<std::shared_ptr<TypeIdentifierAst>> {
    auto parts = op->type_parts();
    parts.append_range(rhs->type_parts());
    return parts;
}


auto spp::asts::TypeUnaryExpressionAst::without_convention() const
    -> std::shared_ptr<const TypeAst> {
    if (op->to<TypeUnaryExpressionOperatorBorrowAst>() != nullptr) {
        return rhs;
    }
    return std::dynamic_pointer_cast<const TypeAst>(shared_from_this());
}


auto spp::asts::TypeUnaryExpressionAst::get_convention() const
    -> ConventionAst* {
    if (auto const *op_borrow = op->to<TypeUnaryExpressionOperatorBorrowAst>()) {
        return op_borrow->conv.get();
    }
    return nullptr;
}


auto spp::asts::TypeUnaryExpressionAst::with_convention(
    std::unique_ptr<ConventionAst> &&conv) const
    -> std::shared_ptr<TypeAst> {
    if (conv == nullptr) {
        return std::make_shared<TypeUnaryExpressionAst>(op, rhs);
    }
    if (op->to<TypeUnaryExpressionOperatorBorrowAst>()) {
        return std::make_shared<TypeUnaryExpressionAst>(
            std::make_unique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv)),
            rhs);
    }
    return std::make_shared<TypeUnaryExpressionAst>(
        std::make_unique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv)),
        std::make_shared<TypeUnaryExpressionAst>(op, rhs));
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
    CompilerMetaData *meta)
    -> void {
    // Qualify the RHS type.
    if (const auto op_ns = op->to<TypeUnaryExpressionOperatorNamespaceAst>()) {
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
    CompilerMetaData *meta)
    -> void {
    // Analyse the RHS type.
    if (const auto op_ns = op->to<TypeUnaryExpressionOperatorNamespaceAst>()) {
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
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the RHS type.
    const auto type_scope = meta->type_analysis_type_scope ? meta->type_analysis_type_scope : sm->current_scope;
    const auto type_sym = type_scope->get_type_symbol(shared_from_this());
    return type_sym->fq_name()->with_convention(ast_clone(get_convention()));
}
