module;
#include <spp/macros.hpp>

export module spp.abstract:abstract_sym;
import std;

namespace spp {
    SPP_EXP_CLS struct AbstractSymbol;
}


SPP_EXP_CLS struct spp::AbstractSymbol {
    AbstractSymbol();

    virtual ~AbstractSymbol();

    SPP_ATTR_NODISCARD virtual auto clone() const -> std::shared_ptr<AbstractSymbol> = 0;
};
