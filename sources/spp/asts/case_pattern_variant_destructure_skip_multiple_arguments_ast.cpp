module;
#include <spp/macros.hpp>

module spp.asts;
import spp.asts.utils;
import spp.lex;


spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::CasePatternVariantDestructureSkipMultipleArgumentsAst(
    decltype(tok_ellipsis) &&tok_ellipsis,
    std::unique_ptr<CasePatternVariantAst> &&binding) :
    tok_ellipsis(std::move(tok_ellipsis)),
    binding(std::unique_ptr<CasePatternVariantSingleIdentifierAst>(binding ? binding.release()->to<CasePatternVariantSingleIdentifierAst>() : nullptr)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_ellipsis, lex::SppTokenType::TK_DOUBLE_DOT, "..");
}


spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::~CasePatternVariantDestructureSkipMultipleArgumentsAst() = default;


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::pos_start() const
    -> std::size_t {
    return tok_ellipsis->pos_start();
}


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::pos_end() const
    -> std::size_t {
    return binding ? binding->pos_end() : tok_ellipsis->pos_end();
}


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantDestructureSkipMultipleArgumentsAst>(
        ast_clone(tok_ellipsis),
        ast_clone(binding));
}


spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_APPEND(binding);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::convert_to_variable(
    CompilerMetaData *meta)
    -> std::unique_ptr<LocalVariableAst> {
    // Create the local variable destructure attribute binding AST.
    auto var = std::make_unique<LocalVariableDestructureSkipMultipleArgumentsAst>(
        nullptr, binding ? binding->convert_to_variable(meta) : nullptr);
    var->mark_from_case_pattern();
    return var;
}
