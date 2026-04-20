module;
#include <spp/macros.hpp>

export module spp.asts.primary_expression_ast;
import spp.asts.expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PrimaryExpressionAst;
    SPP_EXP_CLS struct BooleanLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct CharLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct FloatLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct IdentifierAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct IntegerLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct StringLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct TupleLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct TypeAst; // TODO: GCC BUG REQUIRES THIS
}


SPP_EXP_CLS struct spp::asts::PrimaryExpressionAst : ExpressionAst {
    PrimaryExpressionAst();

    ~PrimaryExpressionAst() override;
};
