module;
#include <spp/macros.hpp>

export module spp.asts.char_literal_ast;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CharLiteralAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TokenAst;}


SPP_EXP_CLS struct spp::asts::CharLiteralAst final : LiteralAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The char value of the char literal. This is the actual char that is represented by the literal.
     */
    Unique<TokenAst> Val;

    /**
     * Construct the CharLiteralAst with the arguments matching the members.
     * @param[in] val The char value of the char literal.
     */
    explicit CharLiteralAst(
        decltype(Val) &&val);

    ~CharLiteralAst() override;

    SPP_ATTR_NODISCARD auto EqualsCharLiteral(
        CharLiteralAst const &) const
        -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(
        ExpressionAst const &other) const
        -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::CharLiteralAst)
