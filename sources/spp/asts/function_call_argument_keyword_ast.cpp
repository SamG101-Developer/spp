#include <spp/asts/convention_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/function_call_argument_keyword_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionCallArgumentKeywordAst::FunctionCallArgumentKeywordAst(
    decltype(name) &&name,
    decltype(tok_assign) &&tok_assign,
    decltype(conv) &&conv,
    decltype(val) &&val):
    FunctionCallArgumentAst(std::move(conv), std::move(val)),
    name(std::move(name)),
    tok_assign(std::move(tok_assign)) {
}


auto spp::asts::FunctionCallArgumentKeywordAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::FunctionCallArgumentKeywordAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


spp::asts::FunctionCallArgumentKeywordAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(conv);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::FunctionCallArgumentKeywordAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
