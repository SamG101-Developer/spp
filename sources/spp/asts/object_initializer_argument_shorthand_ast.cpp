#include <spp/asts/expression_ast.hpp>
#include <spp/asts/object_initializer_argument_shorthand_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ObjectInitializerArgumentShorthandAst::ObjectInitializerArgumentShorthandAst(
    std::unique_ptr<TokenAst> tok_ellipsis,
    std::unique_ptr<ExpressionAst> &&val):
    ObjectInitializerArgumentAst(std::move(val)),
    tok_ellipsis(std::move(tok_ellipsis)) {
}


spp::asts::ObjectInitializerArgumentShorthandAst::~ObjectInitializerArgumentShorthandAst() = default;


auto spp::asts::ObjectInitializerArgumentShorthandAst::pos_start() const -> std::size_t {
    return tok_ellipsis ? tok_ellipsis->pos_start() : val->pos_start();
}


auto spp::asts::ObjectInitializerArgumentShorthandAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


spp::asts::ObjectInitializerArgumentShorthandAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::ObjectInitializerArgumentShorthandAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
