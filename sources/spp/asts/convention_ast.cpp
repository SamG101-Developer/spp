module spp.asts.convention_ast;


spp::asts::ConventionAst::ConventionAst(
    const ConventionTag tag) :
    tag(tag) {
}


spp::asts::ConventionAst::~ConventionAst() = default;


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
