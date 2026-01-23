module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_argument_comp_positional_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;


spp::asts::GenericArgumentCompPositionalAst::GenericArgumentCompPositionalAst(
    decltype(val) &&val) :
    GenericArgumentCompAst(std::move(val), utils::OrderableTag::POSITIONAL_ARG) {
}


auto spp::asts::GenericArgumentCompPositionalAst::equals(
    GenericArgumentAst const &other) const
    -> std::strong_ordering {
    return other.equals_generic_argument_comp_positional(*this);
}


auto spp::asts::GenericArgumentCompPositionalAst::equals_generic_argument_comp_positional(
    GenericArgumentCompPositionalAst const &other) const
    -> std::strong_ordering {
    if (*val == *other.val) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentCompPositionalAst::pos_start() const
    -> std::size_t {
    return val->pos_start();
}


auto spp::asts::GenericArgumentCompPositionalAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::GenericArgumentCompPositionalAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericArgumentCompPositionalAst>(
        ast_clone(val));
}


spp::asts::GenericArgumentCompPositionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentCompPositionalAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the value.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(val.get());
    val->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::GenericArgumentCompPositionalAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Ensure the argument isn't moved or partially moved (for all conventions)
    val->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *val, *val, *sm, true, true, true, true, true, true, meta);
}
