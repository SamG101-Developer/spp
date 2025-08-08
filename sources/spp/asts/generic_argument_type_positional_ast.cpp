#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericArgumentTypePositionalAst::GenericArgumentTypePositionalAst(
    decltype(val) &&val):
    GenericArgumentTypeAst(std::move(val)) {
}


spp::asts::GenericArgumentTypePositionalAst::~GenericArgumentTypePositionalAst() = default;


auto spp::asts::GenericArgumentTypePositionalAst::pos_start() const -> std::size_t {
    return val->pos_start();
}


auto spp::asts::GenericArgumentTypePositionalAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


spp::asts::GenericArgumentTypePositionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentTypePositionalAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
