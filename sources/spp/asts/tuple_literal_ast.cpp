module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.tuple_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.lex.tokens;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::TupleLiteralAst::TupleLiteralAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elements,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Elems(std::move(elements)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(", 0);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")", 0);
}

spp::asts::TupleLiteralAst::~TupleLiteralAst() = default;

auto spp::asts::TupleLiteralAst::EqualsTupleLiteral(
    TupleLiteralAst const &other) const
    -> Ordering {
    // If two tuple asts don't have the same size, they cannot be equal.
    if (Elems.Len() != other.Elems.Len()) { return Ordering::less; }

    // Ensure each element of the two array literals are equal.
    if (std::ranges::all_of(
        std::views::zip(Elems, other.Elems),
        [](auto const &pair) { return *std::get<0>(pair) == *std::get<1>(pair); })) {
        return Ordering::equal;
    }
    return Ordering::less;
}

auto spp::asts::TupleLiteralAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsTupleLiteral(*this);
}

auto spp::asts::TupleLiteralAst::PosStart() const
    -> std::size_t {
    // Use the "(" token.
    return TokL->PosStart();
}

auto spp::asts::TupleLiteralAst::PosEnd() const
    -> std::size_t {
    // Use the ")" token.
    return TokR->PosEnd();
}

auto spp::asts::TupleLiteralAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TupleLiteralAst>(
        AstClone(TokL),
        AstCloneVec(Elems),
        AstClone(TokR));
}

auto spp::asts::TupleLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Elems, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::TupleLiteralAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::IsTypeBorrowed;

    // Analyse the elements in the tuple.
    for (auto const &elem : Elems) {
        elem->Stage7_AnalyseSemantics(sm, meta);
        RaiseIf<SppInvalidPrimaryExpressionError>(
            not IsPrimaryExprTypeValid(*elem, *sm),
            {sm->CurrentScope}, ERR_ARGS(*elem));
    }

    // Check all the elements are owned by the tuple, not borrowed.
    for (auto const &elem : Elems) {
        auto elem_type = elem->InferType(sm, meta);
        RaiseIf<SppSecondClassBorrowViolationError>(
            IsTypeBorrowed(*elem_type, *sm),
            {sm->CurrentScope}, ERR_ARGS(*elem, *elem_type, "tuple element type"));
    }

    // Analyse the inferred tuple type to generate the generic implementation.
    InferType(sm, meta)->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::TupleLiteralAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Check the memory of each element in the tuple literal.
    for (auto const &elem : Elems) {
        elem->Stage8_CheckMemory(sm, meta);
        ValidateSymbolMemory(*elem, *elem, *sm, true, true, true, true, false, meta);
    }
}

auto spp::asts::TupleLiteralAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Convert the inner elements to compile-time values.
    auto cmp_elems = Vec<Unique<ExpressionAst>>();
    for (auto const &elem : Elems) {
        elem->Stage9_CompTimeResolve(sm, meta);
        cmp_elems.EmplaceBack(std::move(meta->CmpResult));
    }

    // Wrap the compile-time array value.
    meta->CmpResult = MakeUnique<TupleLiteralAst>(
        nullptr, std::move(cmp_elems), nullptr);
}

auto spp::asts::TupleLiteralAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create a struct, to hold the tuple elements (runtime numeric access maps to field indices).
    const auto uid = spp::utils::Uid(this);
    const auto tuple_type = InferType(sm, meta);
    const auto tuple_type_sym = sm->CurrentScope->GetTypeSymbol(tuple_type);
    const auto llvm_type = codegen::llvm_type(*tuple_type_sym, ctx);
    SPP_ASSERT(llvm_type != nullptr);

    // Create the alloca for the tuple.
    const auto alloca = ctx->Builder.CreateAlloca(llvm_type, nullptr, "tuple.alloca" + uid);
    SPP_ASSERT(alloca != nullptr);

    // Store each element into the tuple alloca.
    for (std::size_t i = 0; i < Elems.Len(); ++i) {
        const auto elem_value = Elems[i]->Stage11_CodeGen(sm, meta, ctx);
        SPP_ASSERT(elem_value != nullptr);

        const auto const_idx_0 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), 0);
        const auto const_idx_1 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), i);
        const auto elem_ptr = ctx->Builder.CreateGEP(llvm_type, alloca, {const_idx_0, const_idx_1}, "tuple.elem.ptr" + uid);
        ctx->Builder.CreateStore(elem_value, elem_ptr);
    }

    // Load the tuple value from the alloca and return it.
    const auto tuple_value = ctx->Builder.CreateLoad(llvm_type, alloca, "tuple.val" + uid);
    return tuple_value;
}

auto spp::asts::TupleLiteralAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    using generate::common_types::TupleType;
    USE_CACHED_TYPE_INFERENCE;

    // Create a "..Ts" type, for the tuple type.
    auto types_gen = Elems
        | std::views::transform([sm, meta](auto const &elem) { return AstClone(elem->InferType(sm, meta)); })
        | std::ranges::to<Vec>();

    // Create a tuple type with the inferred element types.
    auto tuple_type = TupleType(PosStart(), std::move(types_gen));
    tuple_type->Stage7_AnalyseSemantics(sm, meta);
    CACHE_TYPE_INFERENCE_AND_RETURN(tuple_type);
}

SPP_MOD_END
