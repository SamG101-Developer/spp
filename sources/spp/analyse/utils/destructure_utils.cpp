module spp.analyse.utils.destructure_utils;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import genex;


auto spp::analyse::utils::destructure_utils::GetNestedBindingIdentifiers(
    Vec<Unique<asts::LocalVariableAst>> const &elems)
    -> Vec<Shared<asts::IdentifierAst>> {
    // Recursively walk the destructure pattern to extract all identifiers.
    return elems
        | genex::views::transform(&asts::LocalVariableAst::ExtractNames)
        | genex::views::join
        | genex::to<Vec>();
}


auto spp::analyse::utils::destructure_utils::UnmatchableSingleIdentifier(
    const std::size_t pos)
    -> Shared<asts::IdentifierAst> {
    // No single identifier represents a binding destructuring.
    return MakeShared<asts::IdentifierAst>(pos, kUnmatchableTag);
}
