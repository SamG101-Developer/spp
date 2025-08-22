#include <spp/asts/type_binary_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/lex/tokens.hpp>

#include <spp/asts/generate/common_types.hpp>


spp::asts::TypeBinaryExpressionAst::TypeBinaryExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    TempTypeAst(),
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::TypeBinaryExpressionAst::~TypeBinaryExpressionAst() = default;


auto spp::asts::TypeBinaryExpressionAst::pos_start() const -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::TypeBinaryExpressionAst::pos_end() const -> std::size_t {
    return rhs->pos_end();
}


auto spp::asts::TypeBinaryExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<TypeBinaryExpressionAst>(
        ast_clone(lhs),
        ast_clone(tok_op),
        ast_clone(rhs));
}


spp::asts::TypeBinaryExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_APPEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::TypeBinaryExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_APPEND(rhs);
    SPP_PRINT_END;
}


auto spp::asts::TypeBinaryExpressionAst::convert() -> std::shared_ptr<TypeAst> {
    using namespace std::string_literals;

    if (tok_op->token_type == lex::SppTokenType::KW_OR) {
        auto inner_types = std::vector<std::shared_ptr<TypeAst>>(2);
        inner_types[0] = std::move(lhs);
        inner_types[1] = std::move(rhs);
        return generate::common_types::variant_type(pos_start(), std::move(inner_types));
    }

    // todo: unsupported feature error for intersection ("and") types.

    std::unreachable();
}
