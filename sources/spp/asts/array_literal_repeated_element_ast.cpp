#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/array_literal_repeated_element_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>


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


spp::asts::ArrayLiteralRepeatedElementAst::~ArrayLiteralRepeatedElementAst() = default;


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
    SPP_STRING_APPEND(tok_semicolon);
    SPP_STRING_APPEND(size);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ArrayLiteralRepeatedElementAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_APPEND(elem);
    SPP_PRINT_APPEND(tok_semicolon);
    SPP_PRINT_APPEND(size);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
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


auto spp::asts::ArrayLiteralRepeatedElementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the repeated element.
    ENFORCE_EXPRESSION_SUBTYPE(elem.get());
    elem->stage_7_analyse_semantics(sm, meta);
    const auto elem_type = elem->infer_type(sm, meta);
    const auto elem_type_sym = sm->current_scope->get_type_symbol(elem_type);

    // Ensure the element type is copyable, so that is can be repeated in the array.
    if (not elem_type_sym->is_copyable()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidExpressionNonCopyableTypeError>().with_args(
            *elem, *elem_type).with_scopes({sm->current_scope}).raise();
    }

    // Ensure the element's type is not a borrow type, as array elements cannot be borrows.
    if (const auto conv = elem_type->get_convention()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *elem, *conv, "repeated array element type").with_scopes({sm->current_scope}).raise();
    }

    // Ensure the size is a constant expression (if symbolic).
    ENFORCE_EXPRESSION_SUBTYPE(size.get());
    auto symbolic_size = asts::ast_cast<IdentifierAst>(ast_clone(size));
    const auto size_sym = sm->current_scope->get_var_symbol(std::move(symbolic_size));
    if (size_sym != nullptr) {
        if (size_sym->memory_info->ast_comptime == nullptr) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppCompileTimeConstantError>().with_args(
                *size).with_scopes({sm->current_scope}).raise();
        }
    }
}


auto spp::asts::ArrayLiteralRepeatedElementAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory of the repeated element (is it initialized etc).
    elem->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *elem, *tok_semicolon, *sm, true, true, true, true, true, false, meta);
}


auto spp::asts::ArrayLiteralRepeatedElementAst::stage_10_code_gen_2(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Collect the generated versions of the elements.
    auto vals = std::vector<llvm::Value*>{};
    vals.reserve(std::stoull(asts::ast_cast<IntegerLiteralAst>(size.get())->val->token_data));
    for (auto i = 0uz; i < vals.capacity(); ++i) {
        vals.emplace_back(elem->stage_9_code_gen_1(sm, meta, ctx));
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


auto spp::asts::ArrayLiteralRepeatedElementAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Create the standard "std::array::Arr[T, n]" type, with generic arguments.
    auto elem_type = elem->infer_type(sm, meta);
    auto array_type = generate::common_types::array_type(tok_l->pos_start(), std::move(elem_type), ast_clone(size));
    array_type->stage_7_analyse_semantics(sm, meta);
    return array_type;
}
