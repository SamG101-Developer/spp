module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.local_variable_destructure_array_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.destructure_utils;
import spp.analyse.utils.type_utils;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.asts.meta.compiler_meta_data;
import spp.asts.type_identifier_ast;
import spp.lex.tokens;
import spp.utils.algorithms;
import genex;

SPP_MOD_BEGIN
spp::asts::LocalVariableDestructureArrayAst::LocalVariableDestructureArrayAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Elems(std::move(elems)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

spp::asts::LocalVariableDestructureArrayAst::~LocalVariableDestructureArrayAst() = default;

auto spp::asts::LocalVariableDestructureArrayAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::LocalVariableDestructureArrayAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::LocalVariableDestructureArrayAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LocalVariableDestructureArrayAst>(
        AstClone(TokL), AstCloneVec(Elems), AstClone(TokR));
}

auto spp::asts::LocalVariableDestructureArrayAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Elems, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::LocalVariableDestructureArrayAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppMultipleRestPatternsError;
    using analyse::errors::SppVariableArrayDestructureArraySizeMismatchError;
    using analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError;
    using analyse::utils::type_utils::IsTypeArr;

    // Only 1 "multi-skip" allowed in a destructure.
    const auto multi_arg_skips = Elems
        | genex::views::ptr
        | genex::views::cast_dynamic<LocalVariableDestructureSkipMultipleArgumentsAst*>()
        | genex::to<Vec>();

    RaiseIf<SppMultipleRestPatternsError>(
        multi_arg_skips.Len() > 1, {sm->CurrentScope},
        ERR_ARGS(*this, *multi_arg_skips[0], *multi_arg_skips[1]));

    // Ensure the right-hand-side is an array type.
    const auto val = meta->LetStatementValue;
    const auto val_type = val->InferType(sm, meta);
    RaiseIf<SppVariableArrayDestructureArrayTypeMismatchError>(
        not IsTypeArr(*val_type, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*this, *val, *val_type));

    // Determine number of elements in the left-hand-side and right-hand-side arrays.
    // Todo: Test destructuring generic array - how would that work? like Arr[Str, n] => don't allow.
    const auto num_lhs_arr_elems = Elems.Len();
    const auto num_rhs_arr_elems = std::stoul(
        val_type->TypeParts().Back()->GnArgGroup->Args[1]->To<GenericArgumentCompAst>()->Val->To<IntegerLiteralAst>()->Val->TokenData);
    RaiseIf<SppVariableArrayDestructureArraySizeMismatchError>(
        (num_lhs_arr_elems < num_rhs_arr_elems and multi_arg_skips.IsEmpty()) or (num_lhs_arr_elems > num_rhs_arr_elems),
        {sm->CurrentScope}, ERR_ARGS(*this, num_lhs_arr_elems, *val, num_rhs_arr_elems));

    // For a bound ".." destructure, ie "let [a, ..b, c] = t", create an intermediary type.
    auto bound_multi_skip = Unique<ArrayLiteralExplicitElementsAst>(nullptr);
    if (not multi_arg_skips.IsEmpty() and multi_arg_skips[0]->Binding != nullptr) {
        const auto m = static_cast<std::size_t>(genex::position(Elems | genex::views::ptr, [&multi_arg_skips](auto const &x) { return x == multi_arg_skips[0]; }));
        auto new_elems = genex::views::iota(m, m + num_rhs_arr_elems - num_lhs_arr_elems + 1)
            | genex::to<Vec>()
            | genex::views::transform([val](const auto i) -> Unique<ExpressionAst> {
                auto id = MakeUnique<IdentifierAst>(val->PosEnd(), std::to_string(i));
                auto rm = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(id));
                auto pf = MakeUnique<PostfixExpressionAst>(AstClone(val), std::move(rm));
                return pf;
            })
            | genex::to<Vec>();

        bound_multi_skip = MakeUnique<ArrayLiteralExplicitElementsAst>(nullptr, std::move(new_elems), nullptr);
    }

    // Create new indexes.
    const auto skip_index = not multi_arg_skips.IsEmpty()
        ? static_cast<std::size_t>(genex::position(Elems | genex::views::ptr, [&](auto const &x) { return x == multi_arg_skips[0]; }))
        : Elems.Len() - 1;
    auto indexes = genex::views::iota(0uz, skip_index + 1uz) | genex::to<Vec>();
    indexes.AppendRange(genex::views::iota(num_lhs_arr_elems, num_rhs_arr_elems) | genex::to<Vec>());

    // Create expanded "let" statements for each part of the destructure.
    for (auto const &[i, elem] : genex::views::zip(indexes, Elems | genex::views::ptr)) {
        const auto cast_elem = elem->To<LocalVariableDestructureSkipMultipleArgumentsAst>();

        // Handle bound multi argument skipping, by assigning the skipped elements into a variable.
        if (cast_elem != nullptr and multi_arg_skips[0]->Binding != nullptr) {
            auto new_ast = MakeUnique<LetStatementInitializedAst>(nullptr, AstClone(cast_elem->Binding), nullptr, nullptr, std::move(bound_multi_skip));
            if (_FromCasePattern) { new_ast->Var->MarkFromCasePattern(); }
            new_ast->Stage7_AnalyseSemantics(sm, meta);
            _NewAsts.EmplaceBack(std::move(new_ast));
        }

        // Skip any conversion for single argument skipping.
        else if (elem->To<LocalVariableDestructureSkipSingleArgumentAst>() != nullptr) {
        }

        // Skip any conversion for unbound multi argument skipping.
        else if (elem->To<LocalVariableDestructureSkipMultipleArgumentsAst>() != nullptr) {
        }

        // Handle and other nested destructure or single identifier.
        else {
            auto index = MakeUnique<IdentifierAst>(val->PosEnd(), std::to_string(i));
            auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(index));
            auto pstfx = MakeUnique<PostfixExpressionAst>(AstClone(val), std::move(field));
            auto new_ast = MakeUnique<LetStatementInitializedAst>(nullptr, AstClone(elem), nullptr, nullptr, std::move(pstfx));
            if (_FromCasePattern) { new_ast->Var->MarkFromCasePattern(); }
            new_ast->Stage7_AnalyseSemantics(sm, meta);
            _NewAsts.EmplaceBack(std::move(new_ast));
        }
    }
}

auto spp::asts::LocalVariableDestructureArrayAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the elements.
    for (auto const &x : _NewAsts) { x->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::LocalVariableDestructureArrayAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve each element.
    for (auto const &x : _NewAsts) { x->Stage9_CompTimeResolve(sm, meta); }
}

auto spp::asts::LocalVariableDestructureArrayAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the "let" statements for each element.
    for (auto const &ast : _NewAsts) { ast->Stage11_CodeGen(sm, meta, ctx); }
    return nullptr;
}

auto spp::asts::LocalVariableDestructureArrayAst::ExtractNames() const
    -> Vec<Shared<IdentifierAst>> {
    // Walk the nested bindings for variable names.
    return analyse::utils::destructure_utils::GetNestedBindingIdentifiers(Elems);
}

auto spp::asts::LocalVariableDestructureArrayAst::ExtractName() const
    -> Shared<IdentifierAst> {
    // No single identifier for destructured bindings.
    return analyse::utils::destructure_utils::UnmatchableSingleIdentifier(PosStart());
}

SPP_MOD_END
