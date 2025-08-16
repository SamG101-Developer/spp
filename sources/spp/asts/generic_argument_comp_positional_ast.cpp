#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/generic_argument_comp_positional_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericArgumentCompPositionalAst::GenericArgumentCompPositionalAst(
    decltype(val) &&val):
    GenericArgumentCompAst(std::move(val)) {
}


spp::asts::GenericArgumentCompPositionalAst::~GenericArgumentCompPositionalAst() = default;


auto spp::asts::GenericArgumentCompPositionalAst::pos_start() const -> std::size_t {
    return val->pos_start();
}


auto spp::asts::GenericArgumentCompPositionalAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


auto spp::asts::GenericArgumentCompPositionalAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<GenericArgumentCompPositionalAst>(
        ast_clone(val));
}


spp::asts::GenericArgumentCompPositionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentCompPositionalAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::GenericArgumentCompPositionalAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the value.
    ENFORCE_EXPRESSION_SUBTYPE(val.get())
    val->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::GenericArgumentCompPositionalAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Ensure the argument isn't moved or partially moved (for all conventions)
    val->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*val, *val, sm, true, true, true, true, true, true, meta);
}
