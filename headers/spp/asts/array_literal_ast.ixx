module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_ast;
import spp.asts.literal_ast;


/**
 * The ArrayLiteralAst is the base class for the two array literals: @c ArrayLiteral0Elements and
 * @c ArrayLiteralNElements. The common base class is for type checking only.
 */
SPP_EXP struct spp::asts::ArrayLiteralAst : LiteralAst {
    using LiteralAst::LiteralAst;

    ~ArrayLiteralAst() override;
};
