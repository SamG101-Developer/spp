module;
#include <spp/macros.hpp>

module spp.asts.iter_pattern_variant_exception_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::IterPatternVariantExceptionAst::IterPatternVariantExceptionAst(
    decltype(tok_exc) &&tok_exc,
    decltype(var) &&var) :
    IterPatternVariantAst(),
    tok_exc(std::move(tok_exc)),
    var(std::move(var)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_exc, lex::SppTokenType::TK_EXCLAMATION_MARK, "!");
}


spp::asts::IterPatternVariantExceptionAst::~IterPatternVariantExceptionAst() = default;


auto spp::asts::IterPatternVariantExceptionAst::pos_start() const
    -> std::size_t {
    return tok_exc->pos_start();
}


auto spp::asts::IterPatternVariantExceptionAst::pos_end() const
    -> std::size_t {
    return var->pos_end();
}


auto spp::asts::IterPatternVariantExceptionAst::clone() const
    -> std::unique_ptr<Ast> {
    auto i = std::make_unique<IterPatternVariantExceptionAst>(
        ast_clone(tok_exc),
        ast_clone(var));
    i->m_mapped_let = ast_clone(m_mapped_let);
    return i;
}


spp::asts::IterPatternVariantExceptionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_exc).append(" ");
    SPP_STRING_APPEND(var);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantExceptionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_exc).append(" ");
    SPP_PRINT_APPEND(var);
    SPP_PRINT_END;
}


auto spp::asts::IterPatternVariantExceptionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a dummy type with the same type as the variable's type, to initialize it.
    auto dummy_type = meta->case_condition->infer_type(sm, meta)->type_parts().back()->generic_arg_group->type_at("Err")->val;
    auto dummy = std::make_unique<ObjectInitializerAst>(std::move(dummy_type), nullptr);

    // Create a new AST node that initializes the variable with the dummy value.
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, std::move(dummy));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::IterPatternVariantExceptionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the variable.
    m_mapped_let->stage_8_check_memory(sm, meta);
}


auto spp::asts::IterPatternVariantExceptionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // Get the generator pointer of the targetted coroutine. This is just the value being inspected (cond).
    auto gen_env = meta->case_condition->stage_10_code_gen_2(sm, meta, ctx);

    // GEP to the "Error" field (field 3).
    const auto gen_type = llvm::PointerType::get(*ctx->context, 0);
    const auto error_ptr = ctx->builder.CreateStructGEP(gen_type, gen_env, 3, "gen.error.ptr");
    const auto error_val = ctx->builder.CreateLoad(llvm::PointerType::get(*ctx->context, 0), error_ptr, "gen.error.val");

    // Alloca for the variable.
    meta->save();
    meta->let_stmt_explicit_type = meta->case_condition->infer_type(sm, meta)->type_parts().back()->generic_arg_group->type_at("Yield")->val;
    meta->let_stmt_from_uninitialized = true; // skip normal value conversion (we have raw value already)
    const auto alloca = m_mapped_let->stage_10_code_gen_2(sm, meta, ctx);
    meta->restore();

    // Store the yield value into the variable.
    ctx->builder.CreateStore(error_val, alloca);
    return nullptr;
}
