module;
#include <spp/macros.hpp>

module spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::ObjectInitializerArgumentKeywordAst::ObjectInitializerArgumentKeywordAst(
    decltype(name) name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val) :
    ObjectInitializerArgumentAst(std::move(name), std::move(val)),
    tok_assign(std::move(tok_assign)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_assign, lex::SppTokenType::TK_ASSIGN, "=");
}


spp::asts::ObjectInitializerArgumentKeywordAst::~ObjectInitializerArgumentKeywordAst() = default;


auto spp::asts::ObjectInitializerArgumentKeywordAst::pos_start() const
    -> std::size_t {
    return name->pos_start();
}


auto spp::asts::ObjectInitializerArgumentKeywordAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::ObjectInitializerArgumentKeywordAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ObjectInitializerArgumentKeywordAst>(
        ast_clone(name),
        ast_clone(tok_assign),
        ast_clone(val));
}


spp::asts::ObjectInitializerArgumentKeywordAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}
