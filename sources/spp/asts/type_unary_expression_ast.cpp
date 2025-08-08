#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_unary_expression_ast.hpp>
#include <spp/asts/type_unary_expression_operator_ast.hpp>
#include <spp/asts/type_unary_expression_operator_borrow_ast.hpp>


spp::asts::TypeUnaryExpressionAst::TypeUnaryExpressionAst(
    decltype(op) &&op,
    decltype(rhs) &&rhs):
    op(std::move(op)),
    rhs(std::move(rhs)) {
}


auto spp::asts::TypeUnaryExpressionAst::pos_start() const -> std::size_t {
    return op->pos_start();
}


auto spp::asts::TypeUnaryExpressionAst::pos_end() const -> std::size_t {
    return rhs->pos_end();
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


auto spp::asts::TypeUnaryExpressionAst::is_never_type() const -> bool {
    return false;
}


auto spp::asts::TypeUnaryExpressionAst::ns_parts() const -> std::vector<IdentifierAst const*> {
    auto lhs_parts = op->ns_parts();
    auto rhs_parts = rhs->ns_parts();
    lhs_parts.insert(lhs_parts.end(), rhs_parts.begin(), rhs_parts.end());
    return lhs_parts;
}


auto spp::asts::TypeUnaryExpressionAst::type_parts() const -> std::vector<TypeIdentifierAst const*> {
    auto lhs_parts = op->type_parts();
    auto rhs_parts = rhs->type_parts();
    lhs_parts.insert(lhs_parts.end(), rhs_parts.begin(), rhs_parts.end());
    return lhs_parts;
}


auto spp::asts::TypeUnaryExpressionAst::without_convention() const -> TypeAst const* {
    return rhs.get();
}


auto spp::asts::TypeUnaryExpressionAst::get_convention() const -> ConventionAst* {
    if (auto const *borrow_op = ast_cast<TypeUnaryExpressionOperatorBorrowAst>(op.get())) {
        return borrow_op->conv.get();
    }
    return nullptr;
}


auto spp::asts::TypeUnaryExpressionAst::with_convention(std::unique_ptr<ConventionAst> &&) const -> std::unique_ptr<TypeAst> {
    // TODO
    return nullptr;
}


auto spp::asts::TypeUnaryExpressionAst::without_generics() const -> std::unique_ptr<TypeAst> {
    // TODO
    return nullptr;
}


auto spp::asts::TypeUnaryExpressionAst::substitute_generics(std::vector<GenericArgumentAst*>) const -> std::unique_ptr<TypeAst> {
    // TODO
    return nullptr;
}


auto spp::asts::TypeUnaryExpressionAst::contains_generic(TypeAst const *generic) const -> bool {
    return rhs->contains_generic(generic);
}


auto spp::asts::TypeUnaryExpressionAst::match_generic(TypeAst const *other, TypeAst const *generic) -> TypeAst* {
    return rhs->match_generic(other, generic);
}


auto spp::asts::TypeUnaryExpressionAst::with_generics(std::unique_ptr<GenericArgumentGroupAst> &&) -> std::unique_ptr<TypeAst> {
    // TODO
    return nullptr;
}
