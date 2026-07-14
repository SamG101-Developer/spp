module spp.analyse.utils.destructure_utils;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;

static const auto UNMATCHABLE = spp::MakeUnique<spp::asts::IdentifierAst>(
    0uz, spp::analyse::utils::destructure_utils::kUnmatchableTag);

auto spp::analyse::utils::destructure_utils::GetNestedBindingIdentifiers(
    Vec<Unique<asts::LocalVariableAst>> const &elems)
    -> Vec<asts::IdentifierAst*> {
    // Recursively walk the destructure pattern to extract all identifiers.
    return elems
        | std::views::transform(&asts::LocalVariableAst::ExtractNames)
        | std::views::join
        | std::ranges::to<Vec>();
}

auto spp::analyse::utils::destructure_utils::UnmatchableSingleIdentifier()
    -> asts::IdentifierAst* {
    // No single identifier represents a binding destructuring.
    return UNMATCHABLE.Get();
}
