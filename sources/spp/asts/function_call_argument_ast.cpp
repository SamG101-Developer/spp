#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/convention_ast.hpp>


spp::asts::FunctionCallArgumentAst::FunctionCallArgumentAst(
    decltype(conv) &&conv,
    decltype(val) &&val):
    conv(std::move(conv)),
    val(std::move(val)),
    m_self_type(nullptr) {
}
