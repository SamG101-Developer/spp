module;
#include <spp/macros.hpp>

export module spp.asts.string_literal_ast;
import spp.asts.literal_ast;
import spp.asts.token_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct StringLiteralAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::StringLiteralAst final : LiteralAst {
    /**
     * The string value of the string literal. This is the actual string that is represented by the literal.
     */
    std::unique_ptr<TokenAst> val;

    /**
     * Construct the StringLiteralAst with the arguments matching the members.
     * @param[in] val The string value of the string literal.
     */
    explicit StringLiteralAst(
        decltype(val) &&val);

    ~StringLiteralAst() override;

    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    auto equals_string_literal(StringLiteralAst const &) const -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


spp::asts::StringLiteralAst::~StringLiteralAst() = default;
