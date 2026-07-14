module spp.codegen.llvm_materialize;
import spp.analyse.scopes.scope_manager;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import spp.utils.types;
import std;


auto spp::codegen::llvm_materialize(
    asts::ExpressionAst &ast,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LLvmCtx *ctx)
    -> asts::IdentifierAst* {
    // Materialise an expression by assigning it to a temporary variable.
    auto uid = spp::utils::Uid(&ast);
    auto var_name = MakeUnique<asts::IdentifierAst>(ast.PosStart(), "$temp" + std::move(uid));
    const auto var = MakeUnique<asts::LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);
    const auto ty = ast.InferType(sm, meta);

    // Analyse semantics and generate code for the let statement.
    meta->Save();
    meta->LetStatementFromUninitialized = true; // Prevent double analysis of the expression.
    meta->LetStatementExplicitType = ty; // Safe as we restore in this scope.
    var->Stage7_AnalyseSemantics(sm, meta);

    // Set the lhs to the variable name.
    meta->LetStatementFromUninitialized = false; // Need to generate the expression now.
    meta->LetStatementValue = &ast;
    var->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    const auto materialized_val = var->To<asts::LocalVariableSingleIdentifierAst>()->Name.Get();
    return materialized_val;
}
