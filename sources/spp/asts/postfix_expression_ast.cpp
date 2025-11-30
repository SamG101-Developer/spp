module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


spp::asts::PostfixExpressionAst::PostfixExpressionAst(
    decltype(lhs) &&lhs,
    decltype(op) &&op) :
    lhs(std::move(lhs)),
    op(std::move(op)) {
}


spp::asts::PostfixExpressionAst::~PostfixExpressionAst() = default;


auto spp::asts::PostfixExpressionAst::pos_start() const
    -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::PostfixExpressionAst::pos_end() const
    -> std::size_t {
    return op->pos_end();
}


auto spp::asts::PostfixExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionAst>(
        ast_clone(lhs),
        ast_clone(op));
}


spp::asts::PostfixExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(op);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(op);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the lhs.
    SPP_ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TYPE(lhs.get());

    // The "ast_clone" is required because the "lhs" could be a uniquely owned TypeAst, which must have access to
    // "shared_from_this" (on a shared pointer, which "ast_clone" provides).
    meta->save();
    meta->return_type_overload_resolver_type = nullptr;
    meta->prevent_auto_generator_resume = false;
    if (lhs->to<TypeAst>() != nullptr) {
        const auto temp_lhs = std::shared_ptr<TypeAst>(lhs.release()->to<TypeAst>());
        temp_lhs->stage_7_analyse_semantics(sm, meta);
        lhs = ast_clone(temp_lhs);
    }
    else {
        lhs->stage_7_analyse_semantics(sm, meta);
    }
    meta->restore();

    // Re-attach the meta info, as it is targeting the lhs.
    meta->save();
    meta->postfix_expression_lhs = lhs.get();
    op->stage_7_analyse_semantics(sm, meta);
    meta->restore();
}


auto spp::asts::PostfixExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the lhs.
    lhs->stage_8_check_memory(sm, meta);
    op->stage_8_check_memory(sm, meta);
}


auto spp::asts::PostfixExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Forward into the operator AST.
    meta->save();
    meta->postfix_expression_lhs = lhs.get();
    const auto ret_val = op->stage_10_code_gen_2(sm, meta, ctx);
    meta->restore();
    return ret_val;
}


auto spp::asts::PostfixExpressionAst::infer_type(
    analyse::scopes::ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Forward into the operator AST.
    meta->save();
    meta->postfix_expression_lhs = lhs.get();
    auto ret_type = op->infer_type(sm, meta);
    meta->restore();
    return ret_type;
}
