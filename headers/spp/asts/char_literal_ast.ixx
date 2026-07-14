module;
#include <spp/macros.hpp>

export module spp.asts.char_literal_ast;
import spp.asts.literal_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CharLiteralAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::CharLiteralAst final : LiteralAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The optional "b" prefix, converting the char into a U8 byte type.
     */
    Unique<TokenAst> BytePrefix;

    /**
     * The char value of the char literal. This is the actual char that is represented by the literal.
     */
    Unique<TokenAst> Val;

    /**
     * Construct the CharLiteralAst with the arguments matching the members.
     * @param[in] byte_prefix The optional byte prefix of the char literal (e.g., 'b' for byte literals).
     * @param[in] val The char value of the char literal.
     */
    explicit CharLiteralAst(
        decltype(BytePrefix) &&byte_prefix,
        decltype(Val) &&val);

    ~CharLiteralAst() override;

    SPP_ATTR_NODISCARD auto EqualsCharLiteral(
        CharLiteralAst const &) const
        -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(
        ExpressionAst const &other) const
        -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::CharLiteralAst)
