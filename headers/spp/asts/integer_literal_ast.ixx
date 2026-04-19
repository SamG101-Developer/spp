module;
#include <spp/macros.hpp>

export module spp.asts:integer_literal_ast;
import :literal_ast;
import spp.codegen.llvm_ctx;
import spp.lex;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IntegerLiteralAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::IntegerLiteralAst final : LiteralAst {
    /**
     * The optionally provided sign token. This can be either a @c + or @c - sign, indicating the sign of the integer
     * literal. No sign means the integer is positive by default.
     */
    std::unique_ptr<TokenAst> tok_sign;

    /**
     * The token that represents the integer literal. This is the actual integer value in the source code.
     */
    std::unique_ptr<TokenAst> val;

    /**
     * The raw type of the integer literal. This is from the postfix tag to the literal, like "i32" or "u64".
     */
    std::string type;

    /**
     * Construct the IntegerLiteralAst with the arguments matching the members.
     * @param[in] tok_sign The optionally provided sign token.
     * @param[in] val The token that represents the integer literal.
     * @param[in] type The type of the integer literal.
     */
    IntegerLiteralAst(
        decltype(tok_sign) &&tok_sign,
        decltype(val) &&val,
        std::string &&type);

    ~IntegerLiteralAst() override;

    SPP_ATTR_NODISCARD auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    SPP_ATTR_NODISCARD auto equals_integer_literal(IntegerLiteralAst const &) const -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    template <typename T> requires std::integral<T>
    auto cpp_value() const -> T;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
