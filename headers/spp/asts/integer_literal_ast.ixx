module;
#include <spp/macros.hpp>

export module spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.lex.tokens;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IntegerLiteralAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::IntegerLiteralAst final : LiteralAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The optionally provided sign token. This can be either a @c + or @c - sign, indicating the sign of the integer
     * literal. No sign means the integer is positive by default.
     */
    Unique<TokenAst> TokSign;

    /**
     * The token that represents the integer literal. This is the actual integer value in the source code.
     */
    Unique<TokenAst> Val;

    /**
     * The raw type of the integer literal. This is from the postfix tag to the literal, like "i32" or "u64".
     */
    Str Type;

    /**
     * Construct the IntegerLiteralAst with the arguments matching the members.
     * @param[in] tok_sign The optionally provided sign token.
     * @param[in] val The token that represents the integer literal.
     * @param[in] type The type of the integer literal.
     */
    IntegerLiteralAst(
        decltype(TokSign) &&tok_sign,
        decltype(Val) &&val,
        Str &&type);

    ~IntegerLiteralAst() override;

    SPP_ATTR_NODISCARD auto EqualsIntegerLiteral(IntegerLiteralAst const &) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

    template <typename T> requires std::integral<T>
    auto CppVal() const -> T;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::IntegerLiteralAst)
