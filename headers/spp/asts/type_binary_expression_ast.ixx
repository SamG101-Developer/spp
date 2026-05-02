module;
#include <spp/macros.hpp>

export module spp.asts.type_binary_expression_ast;
import spp.asts.ast;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeBinaryExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeBinaryExpressionAst final : Ast, mixins::TempTypeAst {
    /**
     * The left-hand side expression of the type binary expression. This is the first operand.
     */
    Shared<TypeAst> Lhs;

    /**
     * The operator token that represents the type binary operation. This indicates the type of operation being
     * performed. Either an "or" (union) or "and" (intersection) operation.
     */
    Unique<TokenAst> TokOp;

    /**
     * The right-hand side expression of the type binary expression. This is the second operand.
     */
    Shared<TypeAst> Rhs;

    /**
     * Construct the TypeBinaryExpressionAst with the arguments matching the members.
     * @param lhs The left-hand side expression of the type binary expression.
     * @param tok_op The operator token that represents the type binary operation.
     * @param rhs The right-hand side expression of the type binary expression.
     */
    TypeBinaryExpressionAst(
        decltype(Lhs) &&lhs,
        decltype(TokOp) &&tok_op,
        decltype(Rhs) &&rhs);

    ~TypeBinaryExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Convert() -> Unique<TypeAst> override;
};
