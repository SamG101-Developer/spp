module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.ret_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_materialize;
import spp.codegen.llvm_type;
import spp.lex.tokens;
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


auto spp::asts::RetStatementAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ret).append(" ");
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::RetStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());

    // Check the enclosing function is a subroutine and not a subroutine, if a value is being returned.
    const auto function_flavour = meta->enclosing_function_flavour;
    if (function_flavour->token_type != lex::SppTokenType::KW_FUN and expr != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppCoroutineContainsRetExprExpressionError>()
            .with_args(*function_flavour, *tok_ret)
            .raises_from(sm->current_scope);
    }

    // Analyse the expression if it exists, and determine the type of the expression.
    auto expr_type = generate::common_types::void_type(pos_start());
    if (expr != nullptr) {
        meta->save();
        SPP_RETURN_TYPE_OVERLOAD_HELPER(expr.get()) {
            meta->return_type_overload_resolver_type = meta->enclosing_function_ret_type[0];
        }

        // For case conditions, we need an assignment target in case of variants.
        meta->assignment_target_type = meta->enclosing_function_ret_type.empty() ? nullptr : meta->enclosing_function_ret_type[0];
        meta->assignment_target = IdentifierAst::from_type(*meta->assignment_target_type);
        expr->stage_7_analyse_semantics(sm, meta);
        expr_type = expr->infer_type(sm, meta);
        meta->restore();

        // Check the expr_type isn't Void (don't allow "ret void_func()" => "void_func(); ret").
        if (analyse::utils::type_utils::is_type_void(*expr_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidVoidValueError>()
                .with_args(*expr, "return value")
                .raises_from(sm->current_scope);
        }
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
        if (not direct_match) {
            const auto expr_for_err = expr ? expr->to<Ast>() : tok_ret->to<Ast>();
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
                .with_args(*expr_type, *expr_type, *expr_for_err, *m_ret_type)
                .raises_from(sm->current_scope);
        }
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
        *expr, *tok_ret, *sm, true, true, true, true, true, true, meta);
}


auto spp::asts::RetStatementAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the return value, if there is one.
        if (expr != nullptr) {
        // Temp holder for non-symbolic condition.
        if (sm->current_scope->get_var_symbol_outermost(*expr).first == nullptr) {
            meta->save();
            meta->assignment_target_type = m_ret_type;
            const auto ret_val = codegen::llvm_materialize(*expr, sm, meta, ctx);
            const auto llvm_ret_val = ret_val->stage_10_code_gen_2(sm, meta, ctx);
            ctx->builder.CreateRet(llvm_ret_val);
            meta->restore();
        }

        // Otherwise, generate normally.
        else {
            const auto llvm_ret_val = expr->stage_10_code_gen_2(sm, meta, ctx);
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
    return generate::common_types::void_type(pos_start());
}
