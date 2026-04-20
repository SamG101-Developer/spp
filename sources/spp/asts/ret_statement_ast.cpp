module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import :common_types;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;
import spp.asts.utils;
import spp.codegen.llvm_materialize;
import spp.lex;
import spp.utils.uid;


spp::asts::RetStatementAst::RetStatementAst(
    decltype(tok_ret) &&tok_ret,
    decltype(expr) &&val) :
    tok_ret(std::move(tok_ret)),
    expr(std::move(val)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_ret, lex::SppTokenType::KW_RET, "ret", expr ? expr->pos_start() : 0);
}


spp::asts::RetStatementAst::~RetStatementAst() = default;


auto spp::asts::RetStatementAst::pos_start() const
    -> std::size_t {
    return tok_ret->pos_start();
}


auto spp::asts::RetStatementAst::pos_end() const
    -> std::size_t {
    return expr ? expr->pos_end() : tok_ret->pos_end();
}


auto spp::asts::RetStatementAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<RetStatementAst>(
        ast_clone(tok_ret),
        ast_clone(expr));
}


spp::asts::RetStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ret).append(" ");
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::RetStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());

    // Check the enclosing function is a subroutine and not a subroutine, if a value is being returned.
    const auto function_flavour = meta->enclosing_function_flavour;
    raise_if<analyse::errors::SppCoroutineContainsRetExprExpressionError>(
        function_flavour->token_type != lex::SppTokenType::KW_FUN and expr != nullptr,
        {sm->current_scope}, ERR_ARGS(*function_flavour, *tok_ret));

    // Analyse the expression if it exists, and determine the type of the expression.
    auto expr_type = common_types::void_type(pos_start());
    if (expr != nullptr) {
        meta->save();
        SPP_RETURN_TYPE_OVERLOAD_HELPER(expr.get()) {
            meta->return_type_overload_resolver_type = meta->enclosing_function_ret_type[0];
        }

        // For case conditions, we need an assignment target in case of variants.
        meta->assignment_target_type = meta->enclosing_function_ret_type.empty() ? nullptr : meta->enclosing_function_ret_type[0];
        meta->assignment_target = meta->assignment_target_type ? IdentifierAst::from_type(*meta->assignment_target_type) : nullptr;
        expr->stage_7_analyse_semantics(sm, meta);
        expr_type = expr->infer_type(sm, meta);
        meta->restore();

        // Check the expr_type isn't Void (don't allow "ret void_func()" => "void_func(); ret").
        raise_if<analyse::errors::SppInvalidVoidValueError>(
            analyse::utils::type_utils::is_type_void(*expr_type, *sm->current_scope),
            {sm->current_scope}, ERR_ARGS(*expr, "return value"));
    }

    // Functions provide the return type, closures require inference; handle the inference.
    if (meta->enclosing_function_ret_type.empty()) {
        m_ret_type = expr_type;
        meta->enclosing_function_ret_type.emplace_back(m_ret_type);
    }
    else {
        m_ret_type = meta->enclosing_function_ret_type.back();
    }

    // Type check the expression type against the return type of the enclosing subroutine.
    if (function_flavour->token_type == lex::SppTokenType::KW_FUN) {
        const auto direct_match = analyse::utils::type_utils::symbolic_eq(*m_ret_type, *expr_type, *meta->enclosing_function_scope, *sm->current_scope);
        const auto expr_for_err = expr ? expr->to<Ast>() : tok_ret->to<Ast>();
        raise_if<analyse::errors::SppTypeMismatchError>(
            not direct_match, {sm->current_scope},
            ERR_ARGS(*expr_type, *expr_type, *expr_for_err, *m_ret_type));
    }
}


auto spp::asts::RetStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If there is no expression, then now ork needs to be done.
    if (expr == nullptr) return;

    // Ensure the argument isn't moved or partially moved (for all conventions)
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *expr, *tok_ret, *sm, true, true, true, true, true, meta);
}


auto spp::asts::RetStatementAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If there is no expression, then return nullptr.
    if (expr == nullptr) { return; }

    // Resolve the expression.
    expr->stage_9_comptime_resolution(sm, meta);
}


auto spp::asts::RetStatementAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // Generate the return value, if there is one.
    if (expr != nullptr) {
        // Temp holder for non-symbolic condition.
        if (analyse::utils::scope_utils::get_var_symbol_outermost(*sm->current_scope, *expr).first == nullptr) {
            meta->save();
            meta->assignment_target_type = m_ret_type;
            const auto ret_val = codegen::llvm_materialize(*expr, sm, meta, ctx);
            const auto llvm_ret_val = ret_val->stage_11_code_gen_2(sm, meta, ctx);
            ctx->builder.CreateRet(llvm_ret_val);
            meta->restore();
        }

        // Otherwise, generate normally.
        else {
            const auto llvm_ret_val = expr->stage_11_code_gen_2(sm, meta, ctx);
            ctx->builder.CreateRet(llvm_ret_val);
        }

        return nullptr;
    }

    // Otherwise, use the return "void" instruction.
    ctx->builder.CreateRetVoid();
    return nullptr;
}


auto spp::asts::RetStatementAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    return common_types::void_type(pos_start());
}


auto spp::asts::RetStatementAst::terminates() const
    -> bool {
    // This is the only statement that always terminates.
    return true;
}
