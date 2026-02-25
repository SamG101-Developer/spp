module spp.asts.array_literal_repeated_element_ast;
import spp.asts.token_ast;


spp::asts::ArrayLiteralRepeatedElementAst::ArrayLiteralRepeatedElementAst(
    decltype(elem) &&elem,
    decltype(size) &&size) :
    elem(std::move(elem)),
    size(std::move(size)) {
}


spp::asts::ArrayLiteralRepeatedElementAst::~ArrayLiteralRepeatedElementAst() = default;


auto spp::asts::ArrayLiteralRepeatedElementAst::to_rust() const
    -> std::string {
    return std::format("[{}; {}]", elem->to_rust(), size->to_rust());
}
