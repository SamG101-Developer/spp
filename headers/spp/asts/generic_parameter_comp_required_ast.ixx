module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_required_ast;
import spp.asts.generic_parameter_comp_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterCompRequiredAst;
}


/**
 * The GenericParameterCompRequiredAst represents required generic @c cmp parameters in classes, function,
 * superimpositions etc. They look like: @code cls MyClass[cmp n: USize] { ... }@endcode.
 */
SPP_EXP_CLS struct spp::asts::GenericParameterCompRequiredAst final : GenericParameterCompAst {
    explicit GenericParameterCompRequiredAst(
        decltype(name) &&name,
        decltype(type) &&type);
    ~GenericParameterCompRequiredAst() override;
    auto to_rust() const -> std::string override;
};
