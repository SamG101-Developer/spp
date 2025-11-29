module;
#include <spp/macros.hpp>

module spp.asts.tuple_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.lex.tokens;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::TupleLiteralAst::TupleLiteralAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elements,
    decltype(tok_r) &&tok_r) :
    LiteralAst(),
    tok_l(std::move(tok_l)),
    elems(std::move(elements)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(", 0);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")", 0);
}


spp::asts::TupleLiteralAst::~TupleLiteralAst() = default;


auto spp::asts::TupleLiteralAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_tuple_literal(*this);
}


auto spp::asts::TupleLiteralAst::equals_tuple_literal(
    TupleLiteralAst const &other) const
    -> std::strong_ordering {
    if (elems.size() == other.elems.size() and genex::all_of(
        genex::views::zip(elems | genex::views::ptr, other.elems | genex::views::ptr) | genex::to<std::vector>(),
        [](auto const &pair) { return *std::get<0>(pair) == *std::get<1>(pair); })) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::TupleLiteralAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::TupleLiteralAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::TupleLiteralAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<TupleLiteralAst>(
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::TupleLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::TupleLiteralAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::TupleLiteralAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the elements in the tuple.
    for (auto const &elem : elems) {
        SPP_ENFORCE_EXPRESSION_SUBTYPE(elem.get());
        elem->stage_7_analyse_semantics(sm, meta);
    }

    // Check all the elements are owned by the tuple, not borrowed.
    for (auto const &elem : elems | genex::views::indirect) {
        if (auto [elem_sym, _] = sm->current_scope->get_var_symbol_outermost(elem); elem_sym != nullptr) {
            if (const auto borrow_ast = std::get<0>(elem_sym->memory_info->ast_borrowed)) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
                    elem, *borrow_ast, "explicit array element type").with_scopes({sm->current_scope}).raise();
            }
        }
    }

    // Analyse the inferred array type to generate the generic implementation.
    infer_type(sm, meta)->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::TupleLiteralAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of each element in the array literal.
    for (auto &&elem : elems) {
        elem->stage_8_check_memory(sm, meta);
        analyse::utils::mem_utils::validate_symbol_memory(
            *elem, *elem, *sm, true, true, true, true, true, false, meta);
    }
}


auto spp::asts::TupleLiteralAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Create a "..Ts" type, for the tuple type.
    auto types_gen = elems
        | genex::views::transform([sm, meta](auto &&elem) { return elem->infer_type(sm, meta); })
        | genex::to<std::vector>();

    // Create a tuple type with the inferred element types.
    auto tuple_type = generate::common_types::tuple_type(pos_start(), std::move(types_gen));
    tuple_type->stage_7_analyse_semantics(sm, meta);
    return tuple_type;
}
