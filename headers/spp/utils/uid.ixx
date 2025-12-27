module;
#include <spp/macros.hpp>

export module spp.utils.uid;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
}

namespace spp::utils {
    SPP_EXP_FUN auto generate_uid(asts::Ast const *)
        -> std::string;

    SPP_EXP_FUN auto generate_uid()
        -> std::string;
}
