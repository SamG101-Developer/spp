module;
#include <spp/macros.hpp>

export module spp.abstract:abstract_sym;

namespace spp {
    SPP_EXP_CLS struct AbstractSymbol;
}


SPP_EXP_CLS struct spp::AbstractSymbol {
    AbstractSymbol();

    virtual ~AbstractSymbol() = default;
};
