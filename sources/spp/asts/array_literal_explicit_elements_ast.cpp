module;
#include <spp/macros.hpp>

module spp.asts.array_literal_explicit_elements_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

import genex;
import llvm;


spp::asts::ArrayLiteralExplicitElementsAst::ArrayLiteralExplicitElementsAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elements,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    elems(std::move(elements)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}


auto spp::asts::ArrayLiteralExplicitElementsAst::equals_array_literal_explicit_elements(
    ArrayLiteralExplicitElementsAst const &other) const
    -> std::strong_ordering {
    if (genex::all_of(
        genex::views::zip(elems | genex::views::ptr, other.elems | genex::views::ptr) | genex::to<std::vector>(),
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


auto spp::asts::ArrayLiteralExplicitElementsAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ArrayLiteralExplicitElementsAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::ArrayLiteralExplicitElementsAst::clone() const
    -> std::unique_ptr<Ast> {
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


auto spp::asts::ArrayLiteralExplicitElementsAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::ArrayLiteralExplicitElementsAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the element inside the array.
    for (auto &&elem : elems) {
        SPP_ENFORCE_EXPRESSION_SUBTYPE(elem.get());
        elem->stage_7_analyse_semantics(sm, meta);
    }

    // Get the 0th element information for comparisons (always exists due to parser rules).
    const auto zeroth_elem = elems[0].get();
    const auto zeroth_type = zeroth_elem->infer_type(sm, meta);

    // Check all elements have the same type as the 0th element.
    const auto elem_types = elems
        | genex::views::transform([sm, meta](auto &&elem) { return elem->infer_type(sm, meta); })
        | genex::to<std::vector>();

    for (auto &&[elem, elem_type] : genex::views::zip(elems | genex::views::ptr, elem_types)) {
        if (not analyse::utils::type_utils::symbolic_eq(*zeroth_type, *elem_type, *sm->current_scope, *sm->current_scope))
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *zeroth_elem, *zeroth_type, *elem, *elem_type).with_scopes({sm->current_scope}).raise();
    }

    // Check all the elements are owned by the array, not borrowed.
    for (auto &&elem : elems | genex::views::ptr) {
        if (auto [elem_sym, _] = sm->current_scope->get_var_symbol_outermost(*elem); elem_sym != nullptr) {
            if (const auto borrow_ast = std::get<0>(elem_sym->memory_info->ast_borrowed)) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
                    *elem, *borrow_ast, "explicit array element type").with_scopes({sm->current_scope}).raise();
            }
        }
    }

    // Analyse the inferred array type to generate the generic implementation.
    infer_type(sm, meta)->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ArrayLiteralExplicitElementsAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of each element in the array literal.
    for (auto const &elem : elems) {
        elem->stage_8_check_memory(sm, meta);
        analyse::utils::mem_utils::validate_symbol_memory(
            *elem, *elem, *sm, true, true, true, true, true, false, meta);
    }
}


auto spp::asts::ArrayLiteralExplicitElementsAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Collect the generated versions of the elements.
    auto vals = std::vector<llvm::Value*>{};
    vals.reserve(elems.size());
    for (auto const &elem : elems) {
        vals.emplace_back(elem->stage_10_code_gen_2(sm, meta, ctx));
    }

    // Create the array type and allocation.
    const auto elem_ty = vals[0]->getType();
    const auto arr_ty = llvm::ArrayType::get(elem_ty, vals.size());
    const auto arr_alloc = ctx->builder.CreateAlloca(arr_ty);

    // Store the elements in the array allocation.
    for (auto i = 0uz; i < vals.size(); ++i) {
        const auto idx0 = llvm::ConstantInt::get(ctx->context, llvm::APInt(64, 0));
        const auto idx1 = llvm::ConstantInt::get(ctx->context, llvm::APInt(64, i));
        const auto elem_ptr = ctx->builder.CreateGEP(arr_ty, arr_alloc, {idx0, idx1});
        ctx->builder.CreateStore(vals[i], elem_ptr);
    }

    // Return the array allocation.
    return arr_alloc;
}


auto spp::asts::ArrayLiteralExplicitElementsAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {

    // Create a "T" type and "n" size, for the array type.
    auto size_tok = std::make_unique<TokenAst>(tok_l->pos_start(), lex::SppTokenType::LX_NUMBER, std::to_string(elems.size()));
    auto size_gen = std::make_unique<IntegerLiteralAst>(nullptr, std::move(size_tok), "uz");
    auto elem_gen = elems[0]->infer_type(sm, meta);

    // Create an array type with the inferred element type and size.
    auto array_type = generate::common_types::array_type(pos_start(), std::move(elem_gen), std::move(size_gen));
    array_type->stage_7_analyse_semantics(sm, meta);
    return array_type;
}
