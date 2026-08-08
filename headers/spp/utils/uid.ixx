module;
#include <spp/macros.hpp>

export module spp.utils.uid;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
}

namespace spp::utils {
  SPP_EXP_FUN SPP_ATTR_HOT
  auto Uid(asts::Ast const *) -> Str;

  SPP_EXP_FUN SPP_ATTR_HOT
  auto Uid() -> Str;
}
