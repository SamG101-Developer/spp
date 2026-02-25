module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentAst;
}


/**
 * The GenericArgumentAst is the base class for all generic arguments. It is inherited by the @c GenericArgumentCompAst
 * and @c GenericArgumentTypeAst, which represent the two types of generic arguments in the language. These in turn are
 * inherited for the positional and keyword variants.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentAst : Ast {
    explicit GenericArgumentAst();
    ~GenericArgumentAst() override;
};
