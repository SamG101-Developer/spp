module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::CasePatternVariantDestructureSkipMultipleArgumentsAst(
    decltype(tok_ellipsis) &&tok_ellipsis,
    std::unique_ptr<CasePatternVariantAst> &&binding) :
    tok_ellipsis(std::move(tok_ellipsis)),
    binding(std::unique_ptr<CasePatternVariantSingleIdentifierAst>(binding.release()->to<CasePatternVariantSingleIdentifierAst>())) {
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


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_APPEND(binding);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::convert_to_variable(
    CompilerMetaData *meta)
    -> std::unique_ptr<LocalVariableAst> {
    // Create the local variable destructure attribute binding AST.
    auto var = std::make_unique<LocalVariableDestructureSkipMultipleArgumentsAst>(
        nullptr, binding ? binding->convert_to_variable(meta) : nullptr);
    var->m_from_pattern = true;
    return var;
}
