module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.array_literal_explicit_elements_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
spp::asts::ArrayLiteralExplicitElementsAst::ArrayLiteralExplicitElementsAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elements,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Elems(std::move(elements)),
    TokR(std::move(tok_r)) {
    // Default the two tokens.
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

spp::asts::ArrayLiteralExplicitElementsAst::~ArrayLiteralExplicitElementsAst() = default;

auto spp::asts::ArrayLiteralExplicitElementsAst::EqualsArrayLiteralExplicitElements(
    ArrayLiteralExplicitElementsAst const &other) const
    -> Ordering {
    // If two explicit array asts don't have the same size, they cannot be equal.
    if (Elems.Len() != other.Elems.Len()) { return Ordering::less; }

    // Ensure each element of the two array literals are equal.
    if (genex::all_of(
        genex::views::zip(Elems | genex::views::ptr, other.Elems | genex::views::ptr),
        [](auto const &pair) { return *genex::get<0>(pair) == *genex::get<1>(pair); })) {
        return Ordering::equal;
    }
    return Ordering::less;
}

auto spp::asts::ArrayLiteralExplicitElementsAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsArrayLiteralExplicitElements(*this);
}

auto spp::asts::ArrayLiteralExplicitElementsAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::ArrayLiteralExplicitElementsAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::ArrayLiteralExplicitElementsAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ArrayLiteralExplicitElementsAst>(
        AstClone(TokL), AstCloneVec(Elems), AstClone(TokR));
}

auto spp::asts::ArrayLiteralExplicitElementsAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Elems, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::ArrayLiteralExplicitElementsAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Alias the common utils functions and types.
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::IsTypeBorrowed;
    using analyse::utils::type_utils::TypeEq;
    using analyse::errors::SppExpressionTypeInvalidError;
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::errors::SppTypeMismatchError;

    // Analyse the element inside the array. Also enforce beforehand that the element
    // is an acceptable primary expression, ie not a TypeAst or a TokenAst.
    for (auto const &elem : Elems) {
        RaiseIf<SppExpressionTypeInvalidError>(
            not IsPrimaryExprTypeValid(*elem),
            {sm->CurrentScope}, ERR_ARGS(*elem));
        elem->Stage7_AnalyseSemantics(sm, meta);
    }

    // Get the 0th element information for comparisons (always exists due to parser
    // rules). This will be the "correct type" that all elems are compared against.
    // Array elements cannot be borrowed, so check this too.
    const auto z_elem = Elems[0].get();
    const auto z_type = z_elem->InferType(sm, meta);

    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*z_type, *sm),
        {sm->CurrentScope}, ERR_ARGS(*z_elem, *z_type, "array element type"));

    // Check all of the remaining elements have the same type as the 0th element.
    // This also covers the borrow checking for all other elements as they will
    // mismatch with the already validated 0th element.
    for (auto const &c_elem : Elems | genex::views::ptr | genex::views::drop(1)) {
        auto c_type = c_elem->InferType(sm, meta);

        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*z_type, *c_type, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*z_elem, *z_type, *c_elem, *c_type));
    }

    // Analyse the inferred array type to generate the generic implementation for
    // future analysis; the first occurrence of Vec[S32] will not exist in the symbol
    // table, so add it now.
    InferType(sm, meta)->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::ArrayLiteralExplicitElementsAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Alias the common utils functions and types.
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Check the memory of each element in the array literal.
    for (auto const &elem : Elems) {
        elem->Stage8_CheckMemory(sm, meta);
        ValidateSymbolMemory(*elem, *elem, *sm, true, true, true, true, false, meta);
    }
}

auto spp::asts::ArrayLiteralExplicitElementsAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Convert the inner elements to compile-time values.
    auto cmp_elems = UniqueVec<ExpressionAst>();
    for (auto const &elem : Elems) {
        elem->Stage9_CompTimeResolve(sm, meta);
        cmp_elems.EmplaceBack(std::move(meta->CmpResult));
    }

    // Wrap the compile-time array value.
    meta->CmpResult = MakeUnique<ArrayLiteralExplicitElementsAst>(
        nullptr, std::move(cmp_elems), nullptr);
}

auto spp::asts::ArrayLiteralExplicitElementsAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Alias the common utils functions and types.
    using spp::utils::Uid;

    // Runtime allocation. Todo: Can this be removed for comp only?
    if (not ctx->InConstantContext) {
        // Collect the generated versions of the elements.
        auto vals = Vec<llvm::Value*>{};
        vals.Reserve(Elems.Len());
        for (auto const &elem : Elems) {
            vals.EmplaceBack(elem->Stage11_CodeGen(sm, meta, ctx));
        }

        // Create the array type and allocation.
        const auto uid = Uid(this);
        const auto elem_ty = vals[0]->getType();
        const auto arr_ty = llvm::ArrayType::get(elem_ty, vals.Len());
        SPP_ASSERT(arr_ty != nullptr);
        const auto arr_alloc = ctx->Builder.CreateAlloca(arr_ty, nullptr, "array.explicit.alloca" + uid);

        // Store the elements in the array allocation.
        for (auto i = 0uz; i < vals.Len(); ++i) {
            const auto idx0 = llvm::ConstantInt::get(*ctx->Context, llvm::APInt(64, 0));
            const auto idx1 = llvm::ConstantInt::get(*ctx->Context, llvm::APInt(64, i));
            const auto elem_ptr = ctx->Builder.CreateGEP(arr_ty, arr_alloc, {idx0, idx1});

            SPP_ASSERT(vals[i] != nullptr and elem_ptr != nullptr);
            ctx->Builder.CreateStore(vals[i], elem_ptr);
        }

        // Return the array allocation.
        return arr_alloc;
    }

    // Constant array creation.
    auto comp_vals = Vec<llvm::Constant*>{};
    comp_vals.Reserve(Elems.Len());
    for (auto const &elem : Elems) {
        const auto comp_val = llvm::cast<llvm::Constant>(elem->Stage11_CodeGen(sm, meta, ctx));
        comp_vals.EmplaceBack(comp_val);
    }
    const auto elem_ty = comp_vals[0]->getType();
    const auto arr_ty = llvm::ArrayType::get(elem_ty, comp_vals.Len());
    const auto arr_alloc = llvm::ConstantArray::get(arr_ty, comp_vals.ToStdVector());
    return arr_alloc;
}

auto spp::asts::ArrayLiteralExplicitElementsAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Create a "T" type and "n" size, for the array type.
    auto size_tok = MakeUnique<TokenAst>(TokL->PosStart(), lex::SppTokenType::LX_NUMBER, std::to_string(Elems.Len()));
    auto size_gen = MakeUnique<IntegerLiteralAst>(nullptr, std::move(size_tok), "uz");
    auto elem_gen = Elems[0]->InferType(sm, meta);

    // Create an array type with the inferred element type and size.
    auto array_type = generate::common_types::ArrayType(PosStart(), std::move(elem_gen), std::move(size_gen));
    array_type->Stage7_AnalyseSemantics(sm, meta);
    return array_type;
}

SPP_MOD_END
