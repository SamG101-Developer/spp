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
import spp.asts.utils.ast_utils;


spp::asts::IterPatternVariantExceptionAst::IterPatternVariantExceptionAst(
    decltype(tok_exc) &&tok_exc,
    decltype(var) &&var) :
    IterPatternVariantAst(),
    tok_exc(std::move(tok_exc)),
    var(std::move(var)) {
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
    return std::make_unique<IterPatternVariantExceptionAst>(
        ast_clone(tok_exc),
        ast_clone(var));
}


spp::asts::IterPatternVariantExceptionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_exc);
    SPP_STRING_APPEND(var);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantExceptionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_exc);
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
