module;
#include <spp/macros.hpp>

module spp.asts.convention_mut_ast;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::ConventionMutAst::ConventionMutAst(
    decltype(TokBorrow) &&tok_borrow,
    decltype(TokMut) &&tok_mut) :
    ConventionAst(ConventionTag::MUT),
    TokBorrow(std::move(tok_borrow)),
    TokMut(std::move(tok_mut)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokBorrow, lex::SppTokenType::TK_BORROW, "&");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokMut, lex::SppTokenType::KW_MUT, "mut");
}

spp::asts::ConventionMutAst::~ConventionMutAst() = default;

auto spp::asts::ConventionMutAst::PosStart() const
    -> std::size_t {
    // Use the "&" token.
    return TokBorrow->PosStart();
}

auto spp::asts::ConventionMutAst::PosEnd() const
    -> std::size_t {
    // Use the "mut" token.
    return TokMut->PosEnd();
}

auto spp::asts::ConventionMutAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ConventionMutAst>(
        AstClone(TokBorrow), AstClone(TokMut));
}

auto spp::asts::ConventionMutAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokBorrow);
    SPP_STRING_APPEND(TokMut).append(" ");
    SPP_STRING_END;
}

SPP_MOD_END
