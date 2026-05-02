module;
#include <spp/macros.hpp>

module spp.asts.convention_ast;

SPP_MOD_BEGIN
spp::asts::ConventionAst::ConventionAst(
    const ConventionTag tag) :
    _Tag(tag) {
}

spp::asts::ConventionAst::~ConventionAst() = default;

auto spp::asts::ConventionAst::operator==(
    ConventionAst const *that) const
    -> bool {
    // Equality is based on the tag.
    return _Tag == (that ? that->_Tag : ConventionTag::MOV);
}

auto spp::asts::ConventionAst::operator==(
    const ConventionTag that_tag) const
    -> bool {
    // Match the internal tag.
    return _Tag == that_tag;
}

SPP_MOD_END
