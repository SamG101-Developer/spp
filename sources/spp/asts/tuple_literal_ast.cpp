module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.tuple_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.lex.tokens;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.utils.uid;
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
    SPP_STRING_EXTEND(elems, ", ");
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::TupleLiteralAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems, ", ");
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
    for (auto const &elem : elems | genex::views::ptr) {
        auto elem_type = elem->infer_type(sm, meta);
        SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(elem, elem_type, *sm, "tuple literal element");
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


auto spp::asts::TupleLiteralAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create a struct, to hold the tuple elements (runtime numeric access maps to field indices).
    const auto uid = spp::utils::generate_uid(this);
    const auto tuple_type = infer_type(sm, meta);
    const auto tuple_type_sym = sm->current_scope->get_type_symbol(tuple_type);
    const auto llvm_type = codegen::llvm_type(*tuple_type_sym, ctx);
    SPP_ASSERT(llvm_type != nullptr);

    // Create the alloca for the tuple.
    const auto alloca = ctx->builder.CreateAlloca(llvm_type, nullptr, "tuple.alloca" + uid);
    SPP_ASSERT(alloca != nullptr);

    // Store each element into the tuple alloca.
    for (std::size_t i = 0; i < elems.size(); ++i) {
        const auto elem_value = elems[i]->stage_10_code_gen_2(sm, meta, ctx);
        SPP_ASSERT(elem_value != nullptr);

        const auto const_idx_0 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), 0);
        const auto const_idx_1 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->context), i);
        const auto elem_ptr = ctx->builder.CreateGEP(llvm_type, alloca, {const_idx_0, const_idx_1}, "tuple.elem.ptr" + uid);
        ctx->builder.CreateStore(elem_value, elem_ptr);
    }

    // Load the tuple value from the alloca and return it.
    const auto tuple_value = ctx->builder.CreateLoad(llvm_type, alloca, "tuple.val" + uid);
    return tuple_value;
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
