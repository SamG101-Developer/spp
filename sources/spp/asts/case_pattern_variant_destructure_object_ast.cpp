#include <algorithm>

#include <spp/asts/case_pattern_variant_destructure_object_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::CasePatternVariantDestructureObjectAst::CasePatternVariantDestructureObjectAst(
    decltype(type) &&type,
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r):
    type(std::move(type)),
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::CasePatternVariantDestructureObjectAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::CasePatternVariantDestructureObjectAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::CasePatternVariantDestructureObjectAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureObjectAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
