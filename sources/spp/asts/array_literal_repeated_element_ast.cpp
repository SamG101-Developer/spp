module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.array_literal_repeated_element_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;
import llvm;


spp::asts::ArrayLiteralRepeatedElementAst::ArrayLiteralRepeatedElementAst(
    decltype(tok_l) &&tok_l,
    decltype(elem) &&elem,
    decltype(tok_semicolon) &&tok_semicolon,
    decltype(size) &&size,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    elem(std::move(elem)),
    tok_semicolon(std::move(tok_semicolon)),
    size(std::move(size)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_semicolon, lex::SppTokenType::TK_SEMICOLON, ";");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}


auto spp::asts::ArrayLiteralRepeatedElementAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_array_literal_repeated_elements(*this);
}


auto spp::asts::ArrayLiteralRepeatedElementAst::equals_array_literal_repeated_elements(
    ArrayLiteralRepeatedElementAst const &other) const
    -> std::strong_ordering {
    if (*elem == *other.elem and *size == *other.size) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::ArrayLiteralRepeatedElementAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ArrayLiteralRepeatedElementAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::ArrayLiteralRepeatedElementAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ArrayLiteralRepeatedElementAst>(
        ast_clone(tok_l),
        ast_clone(elem),
        ast_clone(tok_semicolon),
        ast_clone(size),
        ast_clone(tok_r));
}


spp::asts::ArrayLiteralRepeatedElementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_APPEND(elem);
    SPP_STRING_APPEND(tok_semicolon).append(" ");
    SPP_STRING_APPEND(size);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ArrayLiteralRepeatedElementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the repeated element.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(elem.get());
    elem->stage_7_analyse_semantics(sm, meta);
    const auto elem_type = elem->infer_type(sm, meta);
    const auto elem_type_sym = sm->current_scope->get_type_symbol(elem_type);

    // Ensure the element type is copyable, so that is can be repeated in the array.
    raise_if<analyse::errors::SppInvalidExpressionNonCopyableTypeError>(
        not elem_type_sym->is_copyable(),
        {sm->current_scope}, ERR_ARGS(*elem, *elem_type));

    // Ensure the element's type is not a borrow type, as array elements cannot be borrows.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(size.get());
    SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(
        elem, elem_type, *sm, "repeated array element type");

    // Ensure the size is a constant expression (if symbolic).
    auto symbolic_size = ast_clone(size->to<IdentifierAst>());
    const auto size_sym = sm->current_scope->get_var_symbol(std::move(symbolic_size));
    raise_if<analyse::errors::SppCompileTimeConstantError>(
        size_sym != nullptr and size_sym->comptime_value == nullptr,
        {sm->current_scope}, ERR_ARGS(*size));
}


auto spp::asts::ArrayLiteralRepeatedElementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the repeated element (is it initialized etc).
    elem->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *elem, *tok_semicolon, *sm, true, true, true, true, false, meta);
}


auto spp::asts::ArrayLiteralRepeatedElementAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Convert the inner element to a compile-time value.
    elem->stage_9_comptime_resolution(sm, meta);

    // Wrap the compile-time array value.
    meta->cmp_result = std::make_unique<ArrayLiteralRepeatedElementAst>(
        nullptr, std::move(meta->cmp_result), nullptr, ast_clone(size), nullptr);
}


auto spp::asts::ArrayLiteralRepeatedElementAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the size from the generic comp arg.
    const auto num_vals = std::stoull(size->to<IntegerLiteralAst>()->val->token_data);

    // Runtime allocation. Todo: Can this be removed for comp only?
    if (not ctx->in_constant_context) {
        // Collect the generated versions of the elements.
        auto vals = std::vector<llvm::Value*>{};
        vals.reserve(num_vals);
        for (auto i = 0uz; i < vals.capacity(); ++i) {
            vals.emplace_back(elem->stage_11_code_gen_2(sm, meta, ctx));
        }

        // Create the array type.
        const auto uid = spp::utils::generate_uid(this);
        const auto elem_ty = vals[0]->getType();
        const auto arr_ty = llvm::ArrayType::get(elem_ty, vals.size());
        SPP_ASSERT(arr_ty != nullptr);
        const auto arr_alloc = ctx->builder.CreateAlloca(arr_ty, nullptr, "array.repeated.alloca" + uid);

        // Store the elements in the array allocation.
        for (auto i = 0uz; i < vals.size(); ++i) {
            const auto idx0 = llvm::ConstantInt::get(*ctx->context, llvm::APInt(64, 0));
            const auto idx1 = llvm::ConstantInt::get(*ctx->context, llvm::APInt(64, i));
            const auto elem_ptr = ctx->builder.CreateGEP(arr_ty, arr_alloc, {idx0, idx1});

            SPP_ASSERT(vals[i] != nullptr and elem_ptr != nullptr);
            ctx->builder.CreateStore(vals[i], elem_ptr);
        }

        // Return the array allocation.
        return arr_alloc;
    }

    // Constant array creation.
    const auto comp_val = llvm::cast<llvm::Constant>(elem->stage_11_code_gen_2(sm, meta, ctx));
    const auto comp_vals = std::vector(num_vals, comp_val);

    const auto elem_ty = comp_val->getType();
    const auto arr_ty = llvm::ArrayType::get(elem_ty, comp_vals.size());
    const auto arr_alloc = llvm::ConstantArray::get(arr_ty, comp_vals);
    return arr_alloc;
}


auto spp::asts::ArrayLiteralRepeatedElementAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Create the standard "std::array::Arr[T, n]" type, with generic arguments.
    auto elem_type = elem->infer_type(sm, meta);
    auto array_type = generate::common_types::array_type(tok_l->pos_start(), std::move(elem_type), ast_clone(size));
    array_type->stage_7_analyse_semantics(sm, meta);
    return array_type;
}
