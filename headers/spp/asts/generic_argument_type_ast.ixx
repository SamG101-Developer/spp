module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The GenericArgumentTypeAst represents a generic argument that accepts a type (not a compile time value).
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypeAst : GenericArgumentAst {
    std::unique_ptr<TypeAst> val;

    explicit GenericArgumentTypeAst(
        decltype(val) &&val);
    ~GenericArgumentTypeAst() override;
};
