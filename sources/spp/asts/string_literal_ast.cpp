module;
#include <spp/macros.hpp>

module spp.asts.string_literal_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import llvm;


spp::asts::StringLiteralAst::StringLiteralAst(
    decltype(val) &&val) :
    LiteralAst(),
    val(std::move(val)) {
}


auto spp::asts::StringLiteralAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_string_literal(*this);
}


auto spp::asts::StringLiteralAst::equals_string_literal(
    StringLiteralAst const &other) const
    -> std::strong_ordering {
    if (val->token_data == other.val->token_data) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::StringLiteralAst::pos_start() const
    -> std::size_t {
    return val->pos_start();
}


auto spp::asts::StringLiteralAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::StringLiteralAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<StringLiteralAst>(
        ast_clone(val));
}


spp::asts::StringLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::StringLiteralAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::StringLiteralAst::stage_10_code_gen_2(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create a global string for the string literal.
    const auto bytes = val->token_data;
    const auto str_alloc = ctx->builder.CreateGlobalString(bytes, "string_literal", 0, ctx->module.get(), false);
    return str_alloc;
}


auto spp::asts::StringLiteralAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // The type of a string literal is always a string type.
    return generate::common_types::string_view_type(val->pos_start());
}
