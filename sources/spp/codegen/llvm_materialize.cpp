module spp.codegen.llvm_materialize;
import spp.analyse.scopes.scope_manager;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import std;


auto spp::codegen::llvm_materialize(
    asts::ExpressionAst &ast,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LLvmCtx *ctx)
    -> asts::IdentifierAst* {
    // Materialize an expression by assigning it to a temporary variable.
    auto uid = spp::utils::generate_uid(&ast);
    auto var_name = std::make_unique<asts::IdentifierAst>(ast.pos_start(), "$temp" + std::move(uid));
    const auto var = std::make_unique<asts::LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);

    // Analyse semantics and generate code for the let statement.
    meta->save();
    meta->let_stmt_from_uninitialized = true; // Prevent double analysis of the expression.
    meta->let_stmt_explicit_type = ast.infer_type(sm, meta);
    var->stage_7_analyse_semantics(sm, meta);

    // Set the lhs to the variable name.
    meta->let_stmt_from_uninitialized = false; // Need to generate the expression now.
    meta->let_stmt_value = &ast;
    var->stage_10_code_gen_2(sm, meta, ctx);
    meta->restore();
    const auto materialized_val = var->to<asts::LocalVariableSingleIdentifierAst>()->name.get();
    return materialized_val;
}
