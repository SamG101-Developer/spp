module;
#include <spp/macros.hpp>

module spp.asts.char_literal_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import llvm;


spp::asts::CharLiteralAst::CharLiteralAst(
    decltype(val) &&val) :
    LiteralAst(),
    val(std::move(val)) {
}


auto spp::asts::CharLiteralAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_char_literal(*this);
}


auto spp::asts::CharLiteralAst::equals_char_literal(
    CharLiteralAst const &other) const
    -> std::strong_ordering {
    if (val->token_data == other.val->token_data) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::CharLiteralAst::pos_start() const
    -> std::size_t {
    return val->pos_start();
}


auto spp::asts::CharLiteralAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::CharLiteralAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CharLiteralAst>(
        ast_clone(val));
}


spp::asts::CharLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::CharLiteralAst::stage_10_code_gen_2(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create the LLVM constant integer value.
    const auto bit_width = llvm::Type::getInt8Ty(*ctx->context)->getBitWidth();
    const auto ap_int = llvm::APInt(bit_width, val->token_data.c_str(), 10);
    return llvm::ConstantInt::get(*ctx->context, ap_int);
}


auto spp::asts::CharLiteralAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // The type of a string literal is always a string type.
    return generate::common_types::u8(val->pos_start());
}
