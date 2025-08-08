#include <spp/asts/object_initializer_ast.hpp>
#include <spp/asts/object_initializer_argument_group_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::ObjectInitializerAst::ObjectInitializerAst(
    decltype(type) &&type,
    decltype(arg_group) &&arg_group):
    PrimaryExpressionAst(),
    type(std::move(type)),
    arg_group(std::move(arg_group)) {
}


spp::asts::ObjectInitializerAst::~ObjectInitializerAst() = default;


auto spp::asts::ObjectInitializerAst::pos_start() const -> std::size_t {
    return type->pos_start();
}


auto spp::asts::ObjectInitializerAst::pos_end() const -> std::size_t {
    return arg_group->pos_end();
}


spp::asts::ObjectInitializerAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(arg_group);
    SPP_STRING_END;
}


auto spp::asts::ObjectInitializerAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(arg_group);
    SPP_PRINT_END;
}
