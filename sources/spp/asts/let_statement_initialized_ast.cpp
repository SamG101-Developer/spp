module;
#include <spp/macros.hpp>

module spp.asts.let_statement_initialized_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.asts.type_ast;
import spp.lex.tokens;


spp::asts::LetStatementInitializedAst::LetStatementInitializedAst(
    decltype(tok_let) &&tok_let,
    decltype(var) &&var,
    decltype(type) type,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val) :
    tok_let(std::move(tok_let)),
    var(std::move(var)),
    type(std::move(type)),
    tok_assign(std::move(tok_assign)),
    val(std::move(val)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_let, lex::SppTokenType::KW_LET, "let");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_assign, lex::SppTokenType::TK_ASSIGN, "=");
}


spp::asts::LetStatementInitializedAst::~LetStatementInitializedAst() = default;


auto spp::asts::LetStatementInitializedAst::pos_start() const
    -> std::size_t {
    return tok_let->pos_start();
}


auto spp::asts::LetStatementInitializedAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::LetStatementInitializedAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LetStatementInitializedAst>(
        ast_clone(tok_let),
        ast_clone(var),
        ast_clone(type),
        ast_clone(tok_assign),
        ast_clone(val));
}


spp::asts::LetStatementInitializedAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_let).append(" ");
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(type).append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::LetStatementInitializedAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_let).append(" ");
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(type).append(" ");
    SPP_PRINT_APPEND(tok_assign).append(" ");
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::LetStatementInitializedAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the value is a valid expression type.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(val.get());

    // An explicit type can only be applied if the left-hand-side is a single identifier.
    if (type != nullptr and var->to<LocalVariableSingleIdentifierAst>() == nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidTypeAnnotationError>().with_args(
            *type, *var).with_scopes({sm->current_scope}).raise();
    }

    // Analyse the type if it has been given.
    if (type != nullptr) {
        type->stage_7_analyse_semantics(sm, meta);
    }

    // Add the type into the return type overload resolver.
    meta->save();
    meta->return_type_overload_resolver_type = type;
    if (operator std::string().starts_with("let t = ")) {
        auto _ = 123;
    }
    val->stage_7_analyse_semantics(sm, meta);

    meta->assignment_target = var->extract_name();

    // Ensure the value's type matches the type (if given), including variant matching.
    if (type != nullptr) {
        meta->assignment_target_type = type;
        const auto val_type = val->infer_type(sm, meta);
        if (not analyse::utils::type_utils::symbolic_eq(*type, *val_type, *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *type, *type, *val, *val_type).with_scopes({sm->current_scope}).raise();
        }
    }

    meta->let_stmt_explicit_type = type;
    meta->let_stmt_value = val.get();
    var->stage_7_analyse_semantics(sm, meta);
    meta->restore();
}


auto spp::asts::LetStatementInitializedAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the variable's memory (which in turn checks the values memory - must be done this way for destructuring).
    meta->save();
    meta->assignment_target = var->extract_name();
    meta->let_stmt_value = val.get();
    var->stage_8_check_memory(sm, meta);
    meta->restore();
}


auto spp::asts::LetStatementInitializedAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Delegate the code generation to the variable, after setting up the meta.
    meta->save();
    meta->assignment_target = var->extract_name();
    meta->let_stmt_value = val.get();
    const auto alloca = var->stage_10_code_gen_2(sm, meta, ctx);
    meta->restore();
    return alloca;
}
