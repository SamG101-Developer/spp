module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_ast;
import spp.asts.literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralAst;
    SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The ArrayLiteralAst is the base class for the two array literals: @c ArrayLiteral0Elements and
 * @c ArrayLiteralNElements. The common base class is for type checking only.
 */
SPP_EXP_CLS struct spp::asts::ArrayLiteralAst : LiteralAst {
    ArrayLiteralAst() = default;

    ~ArrayLiteralAst() override;
};
