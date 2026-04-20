module;
#include <spp/macros.hpp>

export module spp.utils.uid;
import std;

namespace spp::utils {
    SPP_EXP_FUN auto generate_uid(void const *)
        -> std::string;

    SPP_EXP_FUN auto generate_uid()
        -> std::string;
}
