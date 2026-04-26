module spp.analyse.utils.destructure_utils;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import genex;


auto spp::analyse::utils::destructure_utils::get_nested_binding_identifiers(
    std::vector<std::unique_ptr<asts::LocalVariableAst>> const &elems)
    -> std::vector<std::shared_ptr<asts::IdentifierAst>> {
    // Recursively walk the destructure pattern to extract all identifiers.
    return elems
        | genex::views::transform(&asts::LocalVariableAst::extract_names)
        | genex::views::join
        | genex::to<std::vector>();
}


auto spp::analyse::utils::destructure_utils::unmatchable_single_identifier(
    const std::size_t pos)
    -> std::shared_ptr<asts::IdentifierAst> {
    // No single identifier represents a binding destructuring.
    return std::make_shared<asts::IdentifierAst>(pos, UNMATCHABLE_TAG);
}
