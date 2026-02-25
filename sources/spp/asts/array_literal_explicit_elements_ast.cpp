module spp.asts.array_literal_explicit_elements_ast;


spp::asts::ArrayLiteralExplicitElementsAst::ArrayLiteralExplicitElementsAst(
    decltype(elems) &&elements) :
    elems(std::move(elements)) {
}


auto spp::asts::ArrayLiteralExplicitElementsAst::to_rust() const
    -> std::string {
    auto rust_elems = elems | std::views::transform([](const auto &elem) { return elem->to_rust(); });
    return std::format("[{}]", std::views::join(rust_elems, ", "));
}
