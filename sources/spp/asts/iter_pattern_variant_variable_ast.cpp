#include <spp/asts/iter_pattern_variant_variable_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::IterPatternVariantVariableAst::IterPatternVariantVariableAst(
    decltype(var) &&var):
    IterPatternVariantAst(),
    var(std::move(var)) {
}


spp::asts::IterPatternVariantVariableAst::~IterPatternVariantVariableAst() = default;


auto spp::asts::IterPatternVariantVariableAst::pos_start() const -> std::size_t {
    return var->pos_start();
}


auto spp::asts::IterPatternVariantVariableAst::pos_end() const -> std::size_t {
    return var->pos_end();
}


spp::asts::IterPatternVariantVariableAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantVariableAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_END;
}
