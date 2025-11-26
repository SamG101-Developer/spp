module;
#include <spp/macros.hpp>

module spp.asts.function_call_argument_positional_ast;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.mixins.orderable_ast;


spp::asts::FunctionCallArgumentPositionalAst::FunctionCallArgumentPositionalAst(
    decltype(conv) &&conv,
    decltype(tok_unpack) &&tok_unpack,
    decltype(val) &&val) :
    FunctionCallArgumentAst(std::move(conv), std::move(val), mixins::OrderableTag::POSITIONAL_ARG),
    tok_unpack(std::move(tok_unpack)) {
}


spp::asts::FunctionCallArgumentPositionalAst::~FunctionCallArgumentPositionalAst() = default;


auto spp::asts::FunctionCallArgumentPositionalAst::pos_start() const
    -> std::size_t {
    return tok_unpack ? tok_unpack->pos_start() : val->pos_start();
}


auto spp::asts::FunctionCallArgumentPositionalAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::FunctionCallArgumentPositionalAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionCallArgumentPositionalAst>(
        ast_clone(conv),
        ast_clone(tok_unpack),
        ast_clone(val));
}


spp::asts::FunctionCallArgumentPositionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_unpack);
    SPP_STRING_APPEND(conv);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::FunctionCallArgumentPositionalAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_unpack);
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
