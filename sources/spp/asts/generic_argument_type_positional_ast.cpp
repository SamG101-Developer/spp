module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_type_positional_ast;
import spp.asts.ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;


spp::asts::GenericArgumentTypePositionalAst::GenericArgumentTypePositionalAst(
    decltype(val) val) :
    GenericArgumentTypeAst(std::move(val), mixins::OrderableTag::POSITIONAL_ARG) {
}


spp::asts::GenericArgumentTypePositionalAst::~GenericArgumentTypePositionalAst() = default;


auto spp::asts::GenericArgumentTypePositionalAst::equals(
    GenericArgumentAst const &other) const
    -> std::strong_ordering {
    return other.equals_generic_argument_type_positional(*this);
}


auto spp::asts::GenericArgumentTypePositionalAst::equals_generic_argument_type_positional(
    GenericArgumentTypePositionalAst const &other) const
    -> std::strong_ordering {
    if (*val == *other.val) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentTypePositionalAst::pos_start() const
    -> std::size_t {
    return val->pos_start();
}


auto spp::asts::GenericArgumentTypePositionalAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::GenericArgumentTypePositionalAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericArgumentTypePositionalAst>(
        ast_clone(val));
}


spp::asts::GenericArgumentTypePositionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentTypePositionalAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}


auto spp::asts::GenericArgumentTypePositionalAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Analyse the name and value of the generic type argument.
    val->stage_7_analyse_semantics(sm, meta);
    const auto tmp1 = sm->current_scope->get_type_symbol(val);
    const auto tmp2 = tmp1->fq_name();
    auto tmp3 = ast_clone(val->get_convention());
    const auto tmp4 = tmp2->with_convention(std::move(tmp3));
    val = tmp4;
}
