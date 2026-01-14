module;
#include <spp/macros.hpp>

module spp.asts.convention_mut_ast;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::ConventionMutAst::ConventionMutAst(
    decltype(tok_borrow) &&tok_borrow,
    decltype(tok_mut) &&tok_mut) :
    ConventionAst(ConventionTag::MUT),
    tok_borrow(std::move(tok_borrow)),
    tok_mut(std::move(tok_mut)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_borrow, lex::SppTokenType::TK_BORROW, "&");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_mut, lex::SppTokenType::KW_MUT, "mut");
}


spp::asts::ConventionMutAst::~ConventionMutAst() = default;


auto spp::asts::ConventionMutAst::pos_start() const
    -> std::size_t {
    return tok_borrow->pos_start();
}


auto spp::asts::ConventionMutAst::pos_end() const
    -> std::size_t {
    return tok_mut->pos_end();
}


auto spp::asts::ConventionMutAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ConventionMutAst>(
        ast_clone(tok_borrow),
        ast_clone(tok_mut));
}


spp::asts::ConventionMutAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_borrow);
    SPP_STRING_APPEND(tok_mut).append(" ");
    SPP_STRING_END;
}
