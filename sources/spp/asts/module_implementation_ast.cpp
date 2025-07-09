#include <algorithm>

#include <spp/asts/module_implementation_ast.hpp>


spp::asts::ModuleImplementationAst::ModuleImplementationAst(
    decltype(members) &&members):
    members(std::move(members)) {
}


auto spp::asts::ModuleImplementationAst::pos_start() const -> std::size_t {
    return members.empty() ? 0 : members.front()->pos_start();
}


spp::asts::ModuleImplementationAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(members);
    SPP_STRING_END;
}


auto spp::asts::ModuleImplementationAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(members);
    SPP_PRINT_END;
}
