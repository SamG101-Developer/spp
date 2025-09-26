#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::LetStatementInitializedAst::LetStatementInitializedAst(
    decltype(tok_let) &&tok_let,
    decltype(var) &&var,
    decltype(type) &&type,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val) :
    tok_let(std::move(tok_let)),
    var(std::move(var)),
    type(std::move(type)),
    tok_assign(std::move(tok_assign)),
    val(std::move(val)) {
}


auto spp::asts::LetStatementInitializedAst::pos_start() const -> std::size_t {
    return tok_let->pos_start();
}


auto spp::asts::LetStatementInitializedAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


auto spp::asts::LetStatementInitializedAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<LetStatementInitializedAst>(
        ast_clone(tok_let),
        ast_clone(var),
        ast_clone(type),
        ast_clone(tok_assign),
        ast_clone(val));
}


spp::asts::LetStatementInitializedAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_let);
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::LetStatementInitializedAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_let);
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::LetStatementInitializedAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the value is a valid expression type.
    ENFORCE_EXPRESSION_SUBTYPE(val.get());

    // An explicit type can only be applied if the left-hand-side is a single identifier.
    if (type != nullptr and ast_cast<LocalVariableSingleIdentifierAst>(var.get()) == nullptr) {
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
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the variable's memory (which in turn checks the values memory - must be done this way for destructuring).
    meta->save();
    meta->assignment_target = var->extract_name();
    var->stage_8_check_memory(sm, meta);
    meta->restore();
}
