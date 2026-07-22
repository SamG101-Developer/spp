module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.visibility_utils;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.identifier_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.strings;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PostfixExpressionOperatorRuntimeMemberAccessAst(
    decltype(TokDot) &&tok_dot,
    decltype(Name) name) :
    TokDot(std::move(tok_dot)),
    Name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokDot, lex::SppTokenType::TK_DOT, ".");
}

spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::~PostfixExpressionOperatorRuntimeMemberAccessAst() = default;

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PosStart() const
    -> std::size_t {
    // Use the "." token.
    return TokDot->PosStart();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PosEnd() const
    -> std::size_t {
    // Use the name.
    return Name->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
        AstClone(TokDot), AstClone(Name));
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokDot);
    SPP_STRING_APPEND(Name);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppMemberAccessNonIndexableError;
    using analyse::errors::SppMemberAccessOutOfBoundsError;
    using analyse::errors::SppMemberAccessStaticOperatorExpectedError;
    using analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions;
    using analyse::utils::type_utils::IsTypeCompTimeIndexable;
    using analyse::utils::type_utils::IsIndexWithinBound;
    using analyse::utils::visibility_utils::CheckTypeMemberVisibility;

    // Prevent types on the left-hand-side of a runtime member access.
    RaiseIf<SppMemberAccessStaticOperatorExpectedError>(
        meta->PostfixExpressionLhs->To<TypeAst>() != nullptr,
        {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *TokDot, "type"));

    // Numeric index access (for tuples).
    if (std::isdigit(Name->Val[0])) {
        const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
        const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type);

        // Check the lhs is a tuple/array (the only indexable types).
        RaiseIf<SppMemberAccessNonIndexableError>(
            not IsTypeCompTimeIndexable(*lhs_type, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, *TokDot));

        // Check the index is within the bounds of the tuple/array.
        auto [in_bounds, n] = IsIndexWithinBound(std::stoul(Name->Val), *lhs_type, *sm->CurrentScope);
        RaiseIf<SppMemberAccessOutOfBoundsError>(
            not in_bounds,
            {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, n, *TokDot));
    }

    // Accessing a regular attribute/method on an instance.
    else {
        const auto lhs_as_ident_raw = meta->PostfixExpressionLhs->To<IdentifierAst>();
        const auto lhs_as_ident = lhs_as_ident_raw ? MakeShared<IdentifierAst>(lhs_as_ident_raw->PosStart(), lhs_as_ident_raw->Val) : nullptr;
        const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);

        const auto lhs_ns_sym = sm->CurrentScope->GetNsSymbol(lhs_as_ident);
        const auto lhs_var_sym = sm->CurrentScope->GetVarSymbol(lhs_as_ident);
        const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type);

        // Check the lhs is a variable and not a namespace.
        RaiseIf<SppMemberAccessStaticOperatorExpectedError>( // Todo: this error message uses "Type" -> accept param for ctx (ns)
            lhs_var_sym == nullptr and lhs_ns_sym != nullptr, {sm->CurrentScope},
            ERR_ARGS(*meta->PostfixExpressionLhs, *TokDot, "namespace"));

        // Check the target field exists on the type.
        if (not lhs_type_sym->LinkedScope->HasVarSymbol(Name, true)) {
            // At this point, we need to check for the presence of "FwdMut" or "FwdRef" superimpositions, allowing
            // access to their members.
            auto [fwd_ref_type, _] = analyse::utils::type_utils::GetFwdTypes(*lhs_type, *sm);
            if (fwd_ref_type != nullptr) {
                const auto inner_type = fwd_ref_type->TypeParts().Back()->GnArgGroup->TypeAt("T")->Val->WithoutConvention();
                auto mock_init = MakeUnique<ObjectInitializerAst>(AstClone(inner_type), nullptr);
                auto mock_access = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, Name);
                const auto pf = MakeUnique<PostfixExpressionAst>(std::move(mock_init), std::move(mock_access));
                pf->Stage7_AnalyseSemantics(sm, meta);
                return;
            }

            // Type field was not found on this type, or the forwarding type (includes nested forwarding checks).
            RaiseMissingIdentifierAndClosestOptions(
                *Name, lhs_type_sym->LinkedScope->AllVarSymbols(true, true), {}, *sm);
        }

        auto all_scopes_and_syms = (genex::views::concat(Vec{lhs_type_sym->LinkedScope}, lhs_type_sym->LinkedScope->SupScopes()) | genex::to<Vec>())
            | genex::views::transform([name=Name.get()](auto const &x) { return MakePair(x, x->GetVarSymbol(AstCloneShared(name), true, false)); })
            | genex::to<Vec>()
            | genex::views::filter([](auto const &x) { return x.Second != nullptr; })
            | genex::views::transform([&](auto const &x) { return std::make_tuple(lhs_type_sym->LinkedScope->DepthDiff(x.First), x.First, x.Second); })
            | genex::to<Vec>();

        // Enforce visibility on functional (method) members. Their mock ("$"-typed) symbols are excluded from the
        // attribute handling below, so without this the visibility check never runs for method accesses.
        auto fn_scopes_and_syms = all_scopes_and_syms
            | genex::views::filter([](auto const &x) { return std::get<2>(x)->Type->IsCompilerGeneratedType(); })
            | genex::to<Vec>();
        if (not fn_scopes_and_syms.IsEmpty()) {
            const auto fn_closest = fn_scopes_and_syms.Back();
            const auto cls_scope = lhs_type_sym->LinkedScope->NonGenericScope;
            CheckTypeMemberVisibility(*std::get<2>(fn_closest), *Name, *cls_scope, *sm, *meta);
        }

        auto scopes_and_syms = all_scopes_and_syms
            | genex::views::filter([](auto const &x) { return not std::get<2>(x)->Type->IsCompilerGeneratedType(); })
            | genex::to<Vec>();

        // If we only have functional types, just return.
        if (scopes_and_syms.Len() < 1) { return; }

        auto min_depth = genex::min_element(scopes_and_syms
            | genex::views::tuple_nth<0>
            | genex::to<Vec>());

        auto closest = scopes_and_syms
            | genex::views::filter([min_depth](auto const &x) { return std::get<0>(x) == min_depth; })
            | genex::views::transform([](auto const &x) { return MakePair(std::get<1>(x), std::get<2>(x)); })
            | genex::to<Vec>();

        // Enforce visibility on the accessed member.
        if (not closest.IsEmpty()) {
            const auto scope = closest[0].First->NonGenericScope;
            CheckTypeMemberVisibility(*scope->GetVarSymbol(Name, true), *Name, *scope, *sm, *meta);
        }

        if (closest.Len() <= 1) { return; }
        Raise<analyse::errors::SppAmbiguousMemberAccessError>(
            {closest[0].First, closest[1].First, sm->CurrentScope},
            ERR_ARGS(*closest[0].Second->Name, *closest[1].Second->Name, *Name));
    }
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::cmp_utils::GetCompTimeAttrValue;

    // Resolve the left-hand-side expression.
    meta->PostfixExpressionLhs->Stage9_CompTimeResolve(sm, meta);

    // Handle numeric index access (for tuples).
    if (std::isdigit(Name->Val[0]) and meta->CmpResult->To<TupleLiteralAst>()) {
        const auto cmp_tup = meta->CmpResult->To<TupleLiteralAst>();
        const auto index = std::stoul(Name->Val);
        auto cmp_field = AstClone(cmp_tup->Elems[index]);
        meta->CmpResult = std::move(cmp_field);
        return;
    }

    // Handle numeric index access (for arrays).
    if (std::isdigit(Name->Val[0]) and meta->CmpResult->To<ArrayLiteralExplicitElementsAst>()) {
        const auto cmp_tup = meta->CmpResult->To<ArrayLiteralExplicitElementsAst>();
        const auto index = std::stoul(Name->Val);
        auto cmp_field = AstClone(cmp_tup->Elems[index]);
        meta->CmpResult = std::move(cmp_field);
        return;
    }

    // Handle normal attribute access (for objects).
    const auto cmp_obj = meta->CmpResult->To<ObjectInitializerAst>();
    meta->CmpResult = GetCompTimeAttrValue(cmp_obj, Name.get());
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::utils::type_utils::GetFieldIndexInType;

    // Get the type of the left-hand-side expression.
    const auto uid = "." + spp::utils::Uid(this);
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
    const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type);

    // Index through the object's own type, never the borrow's pointer type, so a borrowed lhs (such as a "&Self"
    // parameter) indexes the same struct that an owned one does.
    const auto is_borrow = lhs_type_sym->Convention != nullptr;
    const auto llvm_type = lhs_type_sym->LlvmInfo->LlvmType;
    SPP_ASSERT(llvm_type != nullptr);

    // If the lhs is symbolic, get the address of the outermost part.
    const auto [sym, _] = sm->CurrentScope->GetVarSymbolOutermost(*meta->PostfixExpressionLhs);
    auto base_ptr = static_cast<llvm::Value*>(nullptr);
    if (sym != nullptr) {
        // The symbol's alloca is already the address of the object (the base pointer). Load borrows to get value.
        SPP_ASSERT(sym->LlvmInfo->Alloca != nullptr);
        base_ptr = is_borrow
            ? ctx->Builder.CreateLoad(llvm::PointerType::get(*ctx->Context, 0), sym->LlvmInfo->Alloca, "load.member_access.base_ptr" + uid)
            : sym->LlvmInfo->Alloca;
    }
    else if (is_borrow) {
        // A borrowed expression already evaluates to the address of the object.
        base_ptr = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);
    }
    else {
        // Materialize the lhs expression into a temporary, to have an address to index through.
        const auto lhs_val = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);
        const auto temp = codegen::llvm_entry_alloca(llvm_type, "temp.member_access.lhs" + uid, ctx);
        ctx->Builder.CreateStore(lhs_val, temp);
        base_ptr = temp;
    }

    // The physical field order isn't the declaration order, because the S++ layout re-orders the fields to minimize
    // padding, so the declaration index has to be resolved through the type's field index map.
    const auto decl_index = GetFieldIndexInType(
        *lhs_type, *Name, sm);
    const auto field_index = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, decl_index);
    return ctx->Builder.CreateStructGEP(llvm_type, base_ptr, field_index, "member_access.field_ptr" + uid);
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    //
    using analyse::utils::type_utils::GetFwdTypes;
    using analyse::utils::type_utils::GetNthTypeOfIndexableType;

    // Get the type of the left-hand-side expression.
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);

    // Numeric index access (for tuples).
    if (std::isdigit(Name->Val[0])) {
        const auto elem_type = GetNthTypeOfIndexableType(
            std::stoul(Name->Val), *lhs_type, *sm->CurrentScope);
        return elem_type;
    }

    // Get the field symbol and return its type, falling back to the forwarding type if needed.
    auto lhs_sym = sm->CurrentScope->GetTypeSymbol(lhs_type);
    auto var_sym = lhs_sym->LinkedScope->GetVarSymbol(Name);
    if (var_sym == nullptr) {
        auto [fwd_ref_type, _] = GetFwdTypes(*lhs_type, *sm);
        const auto inner_type = fwd_ref_type->TypeParts().Back()->GnArgGroup->TypeAt("T")->Val;
        lhs_sym = sm->CurrentScope->GetTypeSymbol(inner_type);
        var_sym = lhs_sym->LinkedScope->GetVarSymbol(Name);
    }
    const auto field_type = var_sym->Type;
    return lhs_sym->LinkedScope->GetTypeSymbol(field_type)->FqName();
}

auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::ExprParts() const
    -> Vec<Ast*> {
    return {Name.get()};
}

SPP_MOD_END
