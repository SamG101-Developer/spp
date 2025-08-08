#include <spp/asts/object_initializer_argument_keyword_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ObjectInitializerArgumentKeywordAst::ObjectInitializerArgumentKeywordAst(
    decltype(name) &&name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val):
    ObjectInitializerArgumentAst(),
    name(std::move(name)),
    tok_assign(std::move(tok_assign)) {
    this->val = std::move(val);
}


spp::asts::ObjectInitializerArgumentKeywordAst::~ObjectInitializerArgumentKeywordAst() = default;


auto spp::asts::ObjectInitializerArgumentKeywordAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::ObjectInitializerArgumentKeywordAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


spp::asts::ObjectInitializerArgumentKeywordAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::ObjectInitializerArgumentKeywordAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
