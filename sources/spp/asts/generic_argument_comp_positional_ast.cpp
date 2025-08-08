#include <spp/asts/generic_argument_comp_positional_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericArgumentCompPositionalAst::GenericArgumentCompPositionalAst(
    decltype(val) &&val):
    GenericArgumentCompAst(std::move(val)) {
}


spp::asts::GenericArgumentCompPositionalAst::~GenericArgumentCompPositionalAst() = default;


auto spp::asts::GenericArgumentCompPositionalAst::pos_start() const -> std::size_t {
    return val->pos_start();
}


auto spp::asts::GenericArgumentCompPositionalAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


spp::asts::GenericArgumentCompPositionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentCompPositionalAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
