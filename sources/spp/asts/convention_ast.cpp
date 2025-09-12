#include <spp/asts/convention_ast.hpp>


spp::asts::ConventionAst::ConventionAst(
    const ConventionTag tag) :
    tag(tag) {
}


auto spp::asts::ConventionAst::operator==(
    ConventionAst const *that) const
    -> bool {
    return tag == (that ? that->tag : ConventionTag::MOV);
}


auto spp::asts::ConventionAst::operator==(
    const ConventionTag that_tag) const
    -> bool {
    return tag == that_tag;
}
