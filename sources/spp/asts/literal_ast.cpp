module;
#include <spp/macros.hpp>

module spp.asts.literal_ast;

SPP_MOD_BEGIN
spp::asts::LiteralAst::LiteralAst() = default;
spp::asts::LiteralAst::~LiteralAst() = default;

auto spp::asts::LiteralAst::IsAllowedInDefault() const
  -> bool {
  // A literal is a value. The array and tuple literals
  // answer for their elements instead, but as a baseline
  // for literals, they can be used as defaults.
  return true;
}

SPP_MOD_END
