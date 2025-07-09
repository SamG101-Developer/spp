#include <algorithm>

#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionPrototypeAst::FunctionPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(tok_fun) &&tok_fun,
    decltype(name) &&name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(param_group) &&param_group,
    decltype(tok_arrow) &&tok_arrow,
    decltype(return_type) &&return_type,
    decltype(implementation) &&implementation):
    annotations(std::move(annotations)),
    tok_fun(std::move(tok_fun)),
    name(std::move(name)),
    generic_param_group(std::move(generic_param_group)),
    param_group(std::move(param_group)),
    tok_arrow(std::move(tok_arrow)),
    return_type(std::move(return_type)),
    implementation(std::move(implementation)) {
}


auto spp::asts::FunctionPrototypeAst::pos_end() -> std::size_t {
    return implementation->pos_end();
}


spp::asts::FunctionPrototypeAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_fun);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(param_group);
    SPP_STRING_APPEND(tok_arrow);
    SPP_STRING_APPEND(return_type);
    SPP_STRING_APPEND(implementation);
    SPP_STRING_END;
}


auto spp::asts::FunctionPrototypeAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_fun);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(param_group);
    SPP_PRINT_APPEND(tok_arrow);
    SPP_PRINT_APPEND(return_type);
    SPP_PRINT_APPEND(implementation);
    SPP_PRINT_END;
}
