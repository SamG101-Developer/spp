module;
#include <spp/macros.hpp>

export module spp.utils.ptr;
import std;


namespace spp::utils::ptr {
    SPP_EXP_FUN template <typename Out, typename In>
    auto unique_cast(std::unique_ptr<In> &&ptr) -> std::unique_ptr<Out> {
        if (ptr == nullptr) { return nullptr; }
        return std::unique_ptr<Out>(dynamic_cast<Out*>(ptr.release()));
    }
}
