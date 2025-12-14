module;
#include <spp/macros.hpp>

export module spp.asts.char_literal_ast;
import spp.asts.literal_ast;
import spp.asts.token_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CharLiteralAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::CharLiteralAst final : LiteralAst {
    /**
     * The char value of the char literal. This is the actual char that is represented by the literal.
     */
    std::unique_ptr<TokenAst> val;

    /**
     * Construct the CharLiteralAst with the arguments matching the members.
     * @param[in] val The char value of the char literal.
     */
    explicit CharLiteralAst(
        decltype(val) &&val);

    ~CharLiteralAst() override;

    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    auto equals_char_literal(CharLiteralAst const &) const -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


spp::asts::CharLiteralAst::~CharLiteralAst() = default;
