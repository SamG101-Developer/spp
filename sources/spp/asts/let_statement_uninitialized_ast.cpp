module;
#include <spp/macros.hpp>

module spp.asts.let_statement_uninitialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;


spp::asts::LetStatementUninitializedAst::LetStatementUninitializedAst(
    decltype(tok_let) &&tok_let,
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type) :
    LetStatementAst(),
    tok_let(std::move(tok_let)),
    var(std::move(var)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
}


spp::asts::LetStatementUninitializedAst::~LetStatementUninitializedAst() = default;


auto spp::asts::LetStatementUninitializedAst::pos_start() const
    -> std::size_t {
    return tok_let->pos_start();
}


auto spp::asts::LetStatementUninitializedAst::pos_end() const
    -> std::size_t {
    return type->pos_end();
}


auto spp::asts::LetStatementUninitializedAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LetStatementUninitializedAst>(
        ast_clone(tok_let),
        ast_clone(var),
        ast_clone(tok_colon),
        ast_clone(type));
}


spp::asts::LetStatementUninitializedAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_let).append(" ");
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(tok_colon).append(" ");
    SPP_STRING_APPEND(type);
    SPP_STRING_END;
}


auto spp::asts::LetStatementUninitializedAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_let).append(" ");
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(tok_colon).append(" ");
    SPP_PRINT_APPEND(type);
    SPP_PRINT_END;
}


auto spp::asts::LetStatementUninitializedAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the type.
    type->stage_7_analyse_semantics(sm, meta);

    // Create a mock value for analysis.
    const auto mock_init = std::make_unique<ObjectInitializerAst>(type, nullptr);

    // Update the meta arguments.
    meta->save();
    meta->let_stmt_value = mock_init.get();
    meta->let_stmt_explicit_type = type;
    meta->let_stmt_from_uninitialized = true;
    var->stage_7_analyse_semantics(sm, meta);
    meta->restore();
}


auto spp::asts::LetStatementUninitializedAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the variable for memory issues.
    meta->save();
    meta->let_stmt_value = nullptr;
    meta->let_stmt_explicit_type = type;
    meta->let_stmt_from_uninitialized = true;
    var->stage_8_check_memory(sm, meta);
    meta->restore();
}


auto spp::asts::LetStatementUninitializedAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Delegate the code generation to the variable, after setting up the meta.
    meta->save();
    meta->let_stmt_value = nullptr;
    meta->let_stmt_explicit_type = type;
    meta->let_stmt_from_uninitialized = true;
    const auto alloca = var->stage_10_code_gen_2(sm, meta, ctx);
    meta->restore();
    return alloca;
}
