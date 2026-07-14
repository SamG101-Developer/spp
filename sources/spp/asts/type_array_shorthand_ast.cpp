module;
#include <spp/macros.hpp>

module spp.asts.type_array_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::TypeArrayShorthandAst::TypeArrayShorthandAst(
    decltype(TokL) &&tok_l,
    decltype(ElemType) &&element_type,
    decltype(TokSemiColon) &&tok_semicolon,
    decltype(Size) &&size,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    ElemType(std::move(element_type)),
    TokSemiColon(std::move(tok_semicolon)),
    Size(std::move(size)),
    TokR(std::move(tok_r)) {
}

spp::asts::TypeArrayShorthandAst::~TypeArrayShorthandAst() = default;

auto spp::asts::TypeArrayShorthandAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::TypeArrayShorthandAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::TypeArrayShorthandAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypeArrayShorthandAst>(
        AstClone(TokL), AstClone(ElemType), AstClone(TokSemiColon), AstClone(Size), AstClone(TokR));
}

auto spp::asts::TypeArrayShorthandAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_APPEND(ElemType);
    SPP_STRING_APPEND(TokSemiColon);
    SPP_STRING_APPEND(Size);
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::TypeArrayShorthandAst::Convert()
    -> Unique<TypeAst> {
    // Convert to the compiler-known array type.
    using generate::common_types::ArrayType;
    const auto type = ArrayType(PosStart(), std::move(ElemType), std::move(Size));
    return AstClone(type);
}

SPP_MOD_END
