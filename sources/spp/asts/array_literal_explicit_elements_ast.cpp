#include <spp/pch.hpp>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/array_literal_explicit_elements_ast.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>

#include <genex/algorithms/all_of.hpp>
#include <genex/algorithms/any_of.hpp>
#include <genex/views/address.hpp>
#include <genex/views/to.hpp>
#include <genex/views/zip.hpp>


spp::asts::ArrayLiteralExplicitElementsAst::ArrayLiteralExplicitElementsAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elements,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    elems(std::move(elements)),
    tok_r(std::move(tok_r)) {
}


spp::asts::ArrayLiteralExplicitElementsAst::~ArrayLiteralExplicitElementsAst() = default;


auto spp::asts::ArrayLiteralExplicitElementsAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ArrayLiteralExplicitElementsAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::ArrayLiteralExplicitElementsAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<ArrayLiteralExplicitElementsAst>(
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::ArrayLiteralExplicitElementsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ArrayLiteralExplicitElementsAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::ArrayLiteralExplicitElementsAst::equals_array_literal_explicit_elements(
    ArrayLiteralExplicitElementsAst const &other) const
    -> std::strong_ordering {
    if (genex::algorithms::all_of(
        genex::views::zip(elems | genex::views::ptr, other.elems | genex::views::ptr) | genex::views::to<std::vector>(),
        [](auto &&pair) { return *std::get<0>(pair) == *std::get<1>(pair); })) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::ArrayLiteralExplicitElementsAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_array_literal_explicit_elements(*this);
}


auto spp::asts::ArrayLiteralExplicitElementsAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    const auto zeroth_elem = elems[0].get();
    const auto zeroth_type = zeroth_elem->infer_type(sm, meta);

    // Analyse the element inside the array.
    for (auto &&elem : elems) {
        ENFORCE_EXPRESSION_SUBTYPE(elem.get());
        elem->stage_7_analyse_semantics(sm, meta);
    }

    // Check all elements have the same type as the 0th element.
    const auto elem_types = elems
        | genex::views::transform([sm, meta](auto &&elem) { return elem->infer_type(sm, meta); })
        | genex::views::to<std::vector>();

    for (auto &&[elem, elem_type] : genex::views::zip(elems | genex::views::ptr, elem_types) | genex::views::to<std::vector>()) {
        if (not analyse::utils::type_utils::symbolic_eq(*zeroth_type, *elem_type, *sm->current_scope, *sm->current_scope))
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *zeroth_elem, *zeroth_type, *elem, *elem_type).with_scopes({sm->current_scope}).raise();
    }

    // Check all the elements are owned by the array, not borrowed.
    for (auto &&elem : elems | genex::views::ptr | genex::views::to<std::vector>()) {
        if (auto [elem_sym, _] = sm->current_scope->get_var_symbol_outermost(*elem); elem_sym != nullptr) {
            if (const auto borrow_ast = elem_sym->memory_info->ast_borrowed) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
                    *elem, *borrow_ast, "explicit array element type").with_scopes({sm->current_scope}).raise();
            }
        }
    }

    // Analyse the inferred array type to generate the generic implementation.
    infer_type(sm, meta)->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ArrayLiteralExplicitElementsAst::stage_8_check_memory(
    ScopeManager *,
    mixins::CompilerMetaData *)
    -> void {
    // Check the memory of each element in the array literal.
    // for (auto &&elem : elems) {
    //     elem->stage_8_check_memory(sm, meta);
    //     analyse::utils::mem_utils::validate_symbol_memory(*elem, *elem, *sm, true, true, true, true, true, true, meta);
    // }
}


auto spp::asts::ArrayLiteralExplicitElementsAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> {
    // Create a "T" type and "n" size, for the array type.
    auto size_tok = std::make_unique<TokenAst>(tok_l->pos_start(), lex::SppTokenType::LX_NUMBER, std::to_string(elems.size()));
    auto size_gen = std::make_unique<IntegerLiteralAst>(nullptr, std::move(size_tok), "uz");
    auto elem_gen = elems[0]->infer_type(sm, meta);

    // Create an array type with the inferred element type and size.
    auto array_type = generate::common_types::array_type(pos_start(), std::move(elem_gen), std::move(size_gen));
    array_type->stage_7_analyse_semantics(sm, meta);
    return array_type;
}
