module;
#include <spp/macros.hpp>

export module spp.asts.literal_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LiteralAst;
    SPP_EXP_CLS struct BooleanLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct CharLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct FloatLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct IdentifierAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct IntegerLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct StringLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct TupleLiteralAst; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS struct TypeAst; // TODO: GCC BUG REQUIRES THIS
}


SPP_EXP_CLS struct spp::asts::LiteralAst : PrimaryExpressionAst {
    LiteralAst();

    ~LiteralAst() override;
};
