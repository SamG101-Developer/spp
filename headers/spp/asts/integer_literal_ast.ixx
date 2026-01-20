module;
#include <spp/macros.hpp>

export module spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import spp.asts.token_ast;
import spp.codegen.llvm_ctx;
import spp.lex.tokens;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IntegerLiteralAst;
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

    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    auto equals_integer_literal(IntegerLiteralAst const &) const -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    template <typename T> requires std::integral<T>
    auto cpp_value() const -> T {
        const auto raw_str = static_cast<std::string>(*val);
        const auto signed_str = tok_sign != nullptr ? "-" + raw_str : raw_str;

        if constexpr (std::is_unsigned_v<T>) { return static_cast<T>(std::stoull(signed_str)); }
        else { return static_cast<T>(std::stoll(signed_str)); }
    }

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


spp::asts::IntegerLiteralAst::~IntegerLiteralAst() = default;
