#include <spp/asts/iter_pattern_variant_exception_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::IterPatternVariantExceptionAst::IterPatternVariantExceptionAst(
    decltype(tok_exc) &&tok_exc,
    decltype(var) &&var):
    IterPatternVariantAst(),
    tok_exc(std::move(tok_exc)),
    var(std::move(var)) {
}


spp::asts::IterPatternVariantExceptionAst::~IterPatternVariantExceptionAst() = default;


auto spp::asts::IterPatternVariantExceptionAst::pos_start() const -> std::size_t {
    return tok_exc->pos_start();
}


auto spp::asts::IterPatternVariantExceptionAst::pos_end() const -> std::size_t {
    return var->pos_end();
}


spp::asts::IterPatternVariantExceptionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_exc);
    SPP_STRING_APPEND(var);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantExceptionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_exc);
    SPP_PRINT_APPEND(var);
    SPP_PRINT_END;
}
