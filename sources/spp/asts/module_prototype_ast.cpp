#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/module_implementation_ast.hpp>


spp::asts::ModulePrototypeAst::ModulePrototypeAst(
    std::unique_ptr<ModuleImplementationAst> &&implementation):
    implementation(std::move(implementation)) {
}


auto spp::asts::ModulePrototypeAst::pos_start() const -> std::size_t {
    return implementation->pos_start();
}


auto spp::asts::ModulePrototypeAst::pos_end() const -> std::size_t {
    return implementation->pos_end();
}


spp::asts::ModulePrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(implementation);
    SPP_STRING_END;
}


auto spp::asts::ModulePrototypeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(implementation);
    SPP_PRINT_END;
}
