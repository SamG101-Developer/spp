module;
#include <spp/macros.hpp>

module spp.asts.iter_pattern_variant_variable_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
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


spp::asts::IterPatternVariantVariableAst::IterPatternVariantVariableAst(
    decltype(var) &&var) :
    IterPatternVariantAst(),
    var(std::move(var)) {
}


spp::asts::IterPatternVariantVariableAst::~IterPatternVariantVariableAst() = default;


auto spp::asts::IterPatternVariantVariableAst::pos_start() const
    -> std::size_t {
    return var->pos_start();
}


auto spp::asts::IterPatternVariantVariableAst::pos_end() const
    -> std::size_t {
    return var->pos_end();
}


auto spp::asts::IterPatternVariantVariableAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IterPatternVariantVariableAst>(
        ast_clone(var));
}


spp::asts::IterPatternVariantVariableAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var).append(" ");
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantVariableAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var).append(" ");
    SPP_PRINT_END;
}


auto spp::asts::IterPatternVariantVariableAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a dummy type with the same type as the variable's type, to initialize it.
    auto dummy_type = meta->case_condition->infer_type(sm, meta)->type_parts().back()->generic_arg_group->type_at("Yield")->val;
    const auto conv = dummy_type->get_convention();
    auto dummy = std::make_unique<ObjectInitializerAst>(std::move(dummy_type), nullptr);

    // Create a new AST node that initializes the variable with the dummy value.
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, std::move(dummy));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);

    // Update borrow flags if the "Yield" type has a borrow convention attached to it.
    for (auto &&name : m_mapped_let->var->extract_names()) {
        // Apply the borrow to the symbol.
        const auto sym = sm->current_scope->get_var_symbol(name);
        sym->memory_info->ast_borrowed = {conv, sm->current_scope};
        sym->type = sym->type->with_convention(ast_clone(conv));
    }
}


auto spp::asts::IterPatternVariantVariableAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the variable.
    m_mapped_let->stage_8_check_memory(sm, meta);
}


auto spp::asts::IterPatternVariantVariableAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // Get the generator pointer of the targetted coroutine. This is just the value being inspected (cond).
    auto gen_env = meta->case_condition->stage_10_code_gen_2(sm, meta, ctx);

    // GEP to the "Yield" field (field 2).
    const auto gen_type = llvm::PointerType::get(*ctx->context, 0);
    const auto yield_ptr = ctx->builder.CreateStructGEP(gen_type, gen_env, 2, "gen.yield.ptr");
    const auto yield_val = ctx->builder.CreateLoad(llvm::PointerType::get(*ctx->context, 0), yield_ptr, "gen.yield.val");

    // Alloca for the variable.
    meta->save();
    meta->let_stmt_explicit_type = meta->case_condition->infer_type(sm, meta)->type_parts().back()->generic_arg_group->type_at("Yield")->val;
    meta->let_stmt_from_uninitialized = true; // skip normal value conversion (we have raw value already)
    const auto alloca = m_mapped_let->stage_10_code_gen_2(sm, meta, ctx); // Todo: will fail for non-single local vars
    meta->restore();

    // Store the yield value into the variable.
    ctx->builder.CreateStore(yield_val, alloca);
    return nullptr;
}
