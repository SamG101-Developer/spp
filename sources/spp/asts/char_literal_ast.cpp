module;
#include <spp/macros.hpp>

module spp.asts.char_literal_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import spp.utils.strings;
import llvm;

SPP_MOD_BEGIN
spp::asts::CharLiteralAst::CharLiteralAst(
    decltype(BytePrefix) &&byte_prefix,
    decltype(Val) &&val) :
    BytePrefix(std::move(byte_prefix)),
    Val(std::move(val)) {
}

spp::asts::CharLiteralAst::~CharLiteralAst() = default;

auto spp::asts::CharLiteralAst::EqualsCharLiteral(
    CharLiteralAst const &other) const
    -> Ordering {
    // Equality based on the token data.
    const auto matching_byte_prefix = static_cast<bool>(BytePrefix) == static_cast<bool>(other.BytePrefix);
    return matching_byte_prefix and Val->TokenData == other.Val->TokenData ? Ordering::equal : Ordering::less;
}

auto spp::asts::CharLiteralAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsCharLiteral(*this);
}

auto spp::asts::CharLiteralAst::PosStart() const
    -> std::size_t {
    // Use the value.
    return Val->PosStart();
}

auto spp::asts::CharLiteralAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::CharLiteralAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CharLiteralAst>(
        AstClone(BytePrefix), AstClone(Val));
}

auto spp::asts::CharLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(BytePrefix);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::CharLiteralAst::Stage9_CompTimeResolve(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    // Clone and return the char literal as is for compile-time resolution.
    meta->CmpResult = AstClone(this);
}

auto spp::asts::CharLiteralAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Decode the char literal token (which includes its surrounding single quotes) into its code point.
    const auto code_point = spp::utils::strings::DecodeCharLiteral(Val->TokenData);

    // Resolve the llvm type from the inferred spp type (U8 for a byte-prefixed literal, else Char).
    const auto type_sym = sm->CurrentScope->GetTypeSymbol(InferType(sm, meta));
    const auto llvm_type = codegen::GetLlvmType(*type_sym, ctx);

    // "b'a'" lowers to a raw U8 byte; a plain "'a'" lowers to the Char aggregate (a struct wrapping a U32
    // unicode scalar), so build a constant struct with the code point as its single field.
    if (BytePrefix != nullptr) {
        return llvm::ConstantInt::get(llvm_type, code_point & 0xFFu);
    }
    const auto struct_type = llvm::cast<llvm::StructType>(llvm_type);
    llvm::Constant *inner = llvm::ConstantInt::get(struct_type->getElementType(0), code_point);
    return llvm::ConstantStruct::get(struct_type, {inner});
}

auto spp::asts::CharLiteralAst::InferType(
    ScopeManager *,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // A char literal is either a Char or U8 type, depending on the "b" byte prefix.
    using generate::common_types::U8;
    using generate::common_types::CharType;
    return BytePrefix != nullptr
        ? U8(Val->PosStart())
        : CharType(Val->PosStart());
}

SPP_MOD_END
