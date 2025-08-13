#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/module_implementation_ast.hpp>


spp::asts::ModulePrototypeAst::ModulePrototypeAst(
    decltype(impl) &&impl) :
    impl(std::move(impl)) {
}


auto spp::asts::ModulePrototypeAst::pos_start() const -> std::size_t {
    return impl->pos_start();
}


auto spp::asts::ModulePrototypeAst::pos_end() const -> std::size_t {
    return impl->pos_end();
}


spp::asts::ModulePrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::ModulePrototypeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}
