module;
#include <spp/macros.hpp>

module spp.asts.convention_ref_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::ConventionRefAst::ConventionRefAst(
    decltype(TokBorrow) &&tok_borrow) :
    ConventionAst(ConventionTag::REF),
    TokBorrow(std::move(tok_borrow)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokBorrow, lex::SppTokenType::TK_BORROW, "&");
}

spp::asts::ConventionRefAst::~ConventionRefAst() = default;

auto spp::asts::ConventionRefAst::PosStart() const
    -> std::size_t {
    // Use the "&" token.
    return TokBorrow->PosStart();
}

auto spp::asts::ConventionRefAst::PosEnd() const
    -> std::size_t {
    // Use the "&" token.
    return TokBorrow->PosEnd();
}

auto spp::asts::ConventionRefAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ConventionRefAst>(
        AstClone(TokBorrow));
}

auto spp::asts::ConventionRefAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokBorrow);
    SPP_STRING_END;
}

SPP_MOD_END
