module;
#include <spp/macros.hpp>

export module spp.asts.literal_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
    SPP_EXP_CLS struct BooleanLiteralAst;
    SPP_EXP_CLS struct CharLiteralAst;
    SPP_EXP_CLS struct FloatLiteralAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct IntegerLiteralAst;
    SPP_EXP_CLS struct LiteralAst;
    SPP_EXP_CLS struct StringLiteralAst;
    SPP_EXP_CLS struct TupleLiteralAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LiteralAst : PrimaryExpressionAst {
    LiteralAst() = default;

    ~LiteralAst() override;
};
