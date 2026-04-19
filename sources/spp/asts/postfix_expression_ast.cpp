module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.mem_utils;
import spp.asts.utils;


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
    // Memory analysis used the transformed AST to not repeat lhs as self.
    if (const auto func = op->to<PostfixExpressionOperatorFunctionCallAst>(); func != nullptr and func->transformed_ast != nullptr) {
        func->transformed_ast->stage_8_check_memory(sm, meta);
        return;
    }

    // Check the memory of the lhs.
    lhs->stage_8_check_memory(sm, meta);
    meta->save();
    meta->postfix_expression_lhs = lhs.get();
    if (lhs->to<IdentifierAst>() != nullptr) {
        analyse::utils::mem_utils::validate_symbol_memory(
            *meta->postfix_expression_lhs, *op, *sm, true, false, false, false, false, meta);
    }
    op->stage_8_check_memory(sm, meta);
    meta->restore();
}


auto spp::asts::PostfixExpressionAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward into the operator AST.
    meta->save();
    meta->postfix_expression_lhs = lhs.get();
    op->stage_9_comptime_resolution(sm, meta);
    meta->restore();
}


auto spp::asts::PostfixExpressionAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // Forward into the operator AST.
    meta->save();
    meta->postfix_expression_lhs = lhs.get();
    const auto ret_val = op->stage_11_code_gen_2(sm, meta, ctx);
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


auto spp::asts::PostfixExpressionAst::expr_parts() const
    -> std::vector<Ast*> {
    // Recursively search the lhs, and add the rhs if it exists.
    auto lhs_parts = lhs->expr_parts();
    auto rhs_parts = op->expr_parts();
    if (not rhs_parts.empty()) {
        lhs_parts.append_range(std::move(rhs_parts));
    }
    return lhs_parts;
}
