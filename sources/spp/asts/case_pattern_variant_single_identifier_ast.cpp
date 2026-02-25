module spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;


spp::asts::CasePatternVariantSingleIdentifierAst::CasePatternVariantSingleIdentifierAst(
    decltype(conv) &&conv,
    decltype(tok_mut) &&tok_mut,
    decltype(name) &&name,
    decltype(alias) &&alias) :
    conv(std::move(conv)),
    tok_mut(std::move(tok_mut)),
    name(std::move(name)),
    alias(std::move(alias)) {
}


spp::asts::CasePatternVariantSingleIdentifierAst::~CasePatternVariantSingleIdentifierAst() = default;


auto spp::asts::CasePatternVariantSingleIdentifierAst::to_rust() const
    -> std::string {
    auto out = std::string();
    if (conv != nullptr) {
        out.append(conv->to_rust_binding()).append(" ");
    }
    if (tok_mut != nullptr) {
        out.append(tok_mut->to_rust()).append(" ");
    }
    out.append(name->to_rust());
    if (alias != nullptr) {
        out.append(" @ ").append(alias->to_rust());
    }
    return out;
}
