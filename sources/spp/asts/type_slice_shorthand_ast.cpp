module;
#include <spp/macros.hpp>

module spp.asts.type_slice_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::TypeSliceShorthandAst::TypeSliceShorthandAst(
    decltype(TokL) &&tok_l,
    decltype(ElemType) &&elem_type,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    ElemType(std::move(elem_type)),
    TokR(std::move(tok_r)) {
}

spp::asts::TypeSliceShorthandAst::~TypeSliceShorthandAst() = default;

auto spp::asts::TypeSliceShorthandAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::TypeSliceShorthandAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::TypeSliceShorthandAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypeSliceShorthandAst>(
        AstClone(TokL), AstClone(ElemType), AstClone(TokR));
}

auto spp::asts::TypeSliceShorthandAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_APPEND(ElemType);
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::TypeSliceShorthandAst::Convert()
    -> Unique<TypeAst> {
    // Convert to the compiler-known array type.
    using generate::common_types::SliceType;
    const auto type = SliceType(PosStart(), std::move(ElemType));
    return AstClone(type);
}

SPP_MOD_END
