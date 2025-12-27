module;
#include <spp/macros.hpp>

module spp.asts.local_variable_single_identifier_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.type_ast;
import spp.codegen.llvm_type;
import spp.utils.uid;


spp::asts::LocalVariableSingleIdentifierAst::LocalVariableSingleIdentifierAst(
    decltype(tok_mut) &&tok_mut,
    decltype(name) name,
    decltype(alias) &&alias) :
    tok_mut(std::move(tok_mut)),
    name(std::move(name)),
    alias(std::move(alias)) {
}


spp::asts::LocalVariableSingleIdentifierAst::~LocalVariableSingleIdentifierAst() = default;


auto spp::asts::LocalVariableSingleIdentifierAst::pos_start() const
    -> std::size_t {
    return tok_mut ? tok_mut->pos_start() : name->pos_start();
}


auto spp::asts::LocalVariableSingleIdentifierAst::pos_end() const
    -> std::size_t {
    return alias ? alias->pos_end() : name->pos_end();
}


auto spp::asts::LocalVariableSingleIdentifierAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableSingleIdentifierAst>(
        ast_clone(tok_mut),
        ast_clone(name),
        ast_clone(alias));
}


spp::asts::LocalVariableSingleIdentifierAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_mut);
    raw_string.append(tok_mut ? " " : "");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(alias);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableSingleIdentifierAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_mut);
    formatted_string.append(tok_mut ? " " : "");
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(alias);
    SPP_PRINT_END;
}


auto spp::asts::LocalVariableSingleIdentifierAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return name;
}


auto spp::asts::LocalVariableSingleIdentifierAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return {name};
}


auto spp::asts::LocalVariableSingleIdentifierAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Get the value and its type from the "meta" information.
    const auto val = meta->let_stmt_from_uninitialized ? nullptr : meta->let_stmt_value;
    const auto val_type = meta->let_stmt_value != nullptr ? meta->let_stmt_value->infer_type(sm, meta) : nullptr;

    // Create a variable symbol for this identifier and value.
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(
        alias != nullptr ? alias->name : name,
        meta->let_stmt_explicit_type != nullptr ? meta->let_stmt_explicit_type : val_type,
        tok_mut != nullptr);

    // Set the initialization AST (for errors).
    sym->memory_info->ast_initialization = {name.get(), sm->current_scope};
    sym->memory_info->ast_initialization_origin = {name.get(), sm->current_scope};

    // Increment the initialization counter for initialized statements.
    if (val != nullptr) {
        sym->memory_info->initialization_counter = 1;

        // Set borrow asts based on the value's type's convention.
        if (const auto conv = val_type->get_convention(); conv != nullptr) {
            sym->memory_info->ast_borrowed = {val, sm->current_scope};
        }
    }

    else {
        // Mark an uninitialized variable as "moved"
        sym->memory_info->ast_moved = {this, sm->current_scope};
    }

    // Add the symbol to the current scope.
    sm->current_scope->add_var_symbol(std::move(sym));
}


auto spp::asts::LocalVariableSingleIdentifierAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // No value => nothing to check.
    if (meta->let_stmt_from_uninitialized) { return; }

    // Check the value's memory.
    meta->let_stmt_value->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *meta->let_stmt_value, *this, *sm, true, true, true, true, true, true, meta);

    // Get the name or alias symbol to mark it as initialized.
    const auto sym = sm->current_scope->get_var_symbol(alias != nullptr ? alias->name : name);
    sym->memory_info->initialized_by(*name, sm->current_scope);
}


auto spp::asts::LocalVariableSingleIdentifierAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create the alloca for the variable.
    const auto uid = spp::utils::generate_uid(this);
    const auto type_sym = sm->current_scope->get_type_symbol(meta->let_stmt_explicit_type);
    const auto llvm_type = codegen::llvm_type(*type_sym, ctx);
    SPP_ASSERT(llvm_type != nullptr);

    // Always alloca at the top of the function for stack variables.
    const auto func = ctx->builder.GetInsertBlock()->getParent();
    const auto entry = &func->getEntryBlock();
    auto temp_builder = llvm::IRBuilder(entry, entry->begin());

    const auto alloca = temp_builder.CreateAlloca(llvm_type, nullptr, "local.alloca" + uid);
    sm->current_scope->get_var_symbol(alias != nullptr ? alias->name : name)->llvm_info->alloca = alloca;

    // Generate the initializer expression.
    if (not meta->let_stmt_from_uninitialized) {
        meta->save();
        meta->assignment_target = alias != nullptr ? alias->name : name;
        const auto llvm_val = meta->let_stmt_value->stage_10_code_gen_2(sm, meta, ctx);
        ctx->builder.CreateStore(llvm_val, alloca);
        meta->restore();
    }

    // Alloca already added; return nullptr.
    return nullptr;
}
