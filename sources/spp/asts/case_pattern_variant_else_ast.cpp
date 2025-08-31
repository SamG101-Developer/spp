#include <spp/asts/case_pattern_variant_else_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantElseAst::CasePatternVariantElseAst(
    decltype(tok_else) &&tok_else) :
    tok_else(std::move(tok_else)) {
}


spp::asts::CasePatternVariantElseAst::~CasePatternVariantElseAst() = default;


auto spp::asts::CasePatternVariantElseAst::pos_start() const -> std::size_t {
    return tok_else->pos_start();
}


auto spp::asts::CasePatternVariantElseAst::pos_end() const -> std::size_t {
    return tok_else->pos_end();
}


auto spp::asts::CasePatternVariantElseAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantElseAst>(
        ast_clone(tok_else));
}


spp::asts::CasePatternVariantElseAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_else);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantElseAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_else);
    SPP_PRINT_END;
}
