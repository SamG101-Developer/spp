module;
#include <spp/macros.hpp>

module spp.asts.type_tuple_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
spp::asts::TypeTupleShorthandAst::TypeTupleShorthandAst(
    decltype(TokL) &&tok_l,
    decltype(ElemTypes) &&element_types,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    ElemTypes(std::move(element_types)),
    TokR(std::move(tok_r)) {
}

spp::asts::TypeTupleShorthandAst::~TypeTupleShorthandAst() = default;

auto spp::asts::TypeTupleShorthandAst::PosStart() const
    -> std::size_t {
    // Use the "(" token.
    return TokL->PosStart();
}

auto spp::asts::TypeTupleShorthandAst::PosEnd() const
    -> std::size_t {
    // Use the ")" token.
    return TokR->PosEnd();
}

auto spp::asts::TypeTupleShorthandAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypeTupleShorthandAst>(
        AstClone(TokL), AstCloneVecShared(ElemTypes), AstClone(TokR));
}

auto spp::asts::TypeTupleShorthandAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(ElemTypes, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::TypeTupleShorthandAst::Convert()
    -> Unique<TypeAst> {
    using generate::common_types::TupleType;
    const auto type = TupleType(
        PosStart(), std::move(ElemTypes));
    return AstClone(type);
}

SPP_MOD_END
