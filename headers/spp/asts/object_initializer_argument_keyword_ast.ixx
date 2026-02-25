module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ObjectInitializerArgumentKeywordAst;
}


/**
 * The ObjectInitializerArgumentKeywordAst represents a keyword argument in a object initializer. It is forces the
 * argument to be matched by a keyword rather than shorthand value.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentKeywordAst final : ObjectInitializerArgumentAst {
    ObjectInitializerArgumentKeywordAst(
        decltype(name) &&name,
        decltype(val) &&val);
    ~ObjectInitializerArgumentKeywordAst() override;
};
