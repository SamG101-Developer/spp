module;
#include <spp/macros.hpp>

module spp.asts.type_binary_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.lex.tokens;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::TypeBinaryExpressionAst::TypeBinaryExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(TokOp) &&tok_op,
    decltype(Rhs) &&rhs) :
    Lhs(std::move(lhs)),
    TokOp(std::move(tok_op)),
    Rhs(std::move(rhs)) {
}

spp::asts::TypeBinaryExpressionAst::~TypeBinaryExpressionAst() = default;

auto spp::asts::TypeBinaryExpressionAst::PosStart() const
    -> std::size_t {
    // Use the lhs.
    return Lhs->PosStart();
}

auto spp::asts::TypeBinaryExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the rhs.
    return Rhs->PosEnd();
}

auto spp::asts::TypeBinaryExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypeBinaryExpressionAst>(
        AstCloneShared(Lhs), AstClone(TokOp), AstCloneShared(Rhs));
}

auto spp::asts::TypeBinaryExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Lhs);
    SPP_STRING_APPEND(TokOp);
    SPP_STRING_APPEND(Rhs);
    SPP_STRING_END;
}

auto spp::asts::TypeBinaryExpressionAst::Convert()
    -> Unique<TypeAst> {
    //
    using generate::common_types::VariantType;

    if (TokOp->TokenType == lex::SppTokenType::KW_OR) {
        auto inner_types = Vec<Shared<TypeAst>>(2);
        const auto pos = PosStart();
        inner_types[0] = std::move(Lhs);
        inner_types[1] = std::move(Rhs);
        const auto type = VariantType(pos, std::move(inner_types));
        return AstClone(type);
    }

    // todo: unsupported feature error for intersection ("and") types.

    std::unreachable();
}

SPP_MOD_END
