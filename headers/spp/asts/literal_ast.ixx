module;
#include <spp/macros.hpp>

export module spp.asts:literal_ast;
import :primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LiteralAst;
}


SPP_EXP_CLS struct spp::asts::LiteralAst : PrimaryExpressionAst {
    LiteralAst();

    ~LiteralAst() override;
};
