module;
#include <spp/macros.hpp>

module spp.asts.iter_pattern_variant_no_value_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::IterPatternVariantNoValueAst::IterPatternVariantNoValueAst(
    decltype(tok_underscore) &&tok_underscore) :
    IterPatternVariantAst(),
    tok_underscore(std::move(tok_underscore)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_underscore, lex::SppTokenType::TK_UNDERSCORE, "_");
}


spp::asts::IterPatternVariantNoValueAst::~IterPatternVariantNoValueAst() = default;


auto spp::asts::IterPatternVariantNoValueAst::pos_start() const
    -> std::size_t {
    return tok_underscore->pos_start();
}


auto spp::asts::IterPatternVariantNoValueAst::pos_end() const
    -> std::size_t {
    return tok_underscore->pos_end();
}


auto spp::asts::IterPatternVariantNoValueAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IterPatternVariantNoValueAst>(
        ast_clone(tok_underscore));
}


spp::asts::IterPatternVariantNoValueAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_underscore).append(" ");
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantNoValueAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_underscore).append(" ");
    SPP_PRINT_END;
}
