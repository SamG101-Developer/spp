module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.analyse.utils.visibility_utils;
import spp.asts.expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.strings;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PostfixExpressionOperatorStaticMemberAccessAst(
    decltype(TokDblColon) &&tok_dbl_colon,
    decltype(Name) &&name) :
    TokDblColon(std::move(tok_dbl_colon)),
    Name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokDblColon, lex::SppTokenType::TK_DOUBLE_COLON, "::", name ? name->PosStart() : 0);
}

spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::~PostfixExpressionOperatorStaticMemberAccessAst() = default;

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PosStart() const
    -> std::size_t {
    // Use the "::" token.
    return TokDblColon->PosStart();
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PosEnd() const
    -> std::size_t {
    // Use the name.
    return Name->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(
        AstClone(TokDblColon), AstClone(Name));
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokDblColon);
    SPP_STRING_APPEND(Name);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions;
    using analyse::utils::visibility_utils::CheckModuleMemberVisibility;
    using analyse::utils::visibility_utils::CheckTypeMemberVisibility;

    // Handle types on the left-hand-side of a static member access.
    if (const auto lhs_as_type = meta->PostfixExpressionLhs->To<TypeAst>(); lhs_as_type != nullptr) {
        const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(AstClone(lhs_as_type));

        // Check the target field exists on the type.
        if (not lhs_type_sym->LinkedScope->HasVarSymbol(Name, true)) {
            RaiseMissingIdentifierAndClosestOptions(*Name, lhs_type_sym->LinkedScope->AllVarSymbols(true), {}, *sm);
        }

        // Check there is only 1 target field on the type at the highest level.
        if (lhs_type_sym->LinkedScope->GetVarSymbol(Name)->Type->IsCompilerGeneratedType()) {
            return;
        }

        auto scopes_and_syms = Vec{lhs_type_sym->LinkedScope}
            | genex::views::concat(lhs_type_sym->LinkedScope->SupScopes())
            | genex::views::transform([name=Name.get()](auto &&x) { return MakePair(x, x->GetVarSymbol(AstCloneShared(name), true)); })
            | genex::views::filter([](auto &&x) { return x.Second != nullptr; })
            | genex::views::transform([&](auto &&x) { return std::make_tuple(lhs_type_sym->LinkedScope->DepthDiff(x.First), x.First, x.Second); })
            | genex::to<Vec>();

        auto min_depth = genex::min_element(scopes_and_syms
            | genex::views::transform([](auto &&x) { return std::get<0>(x); })
            | genex::to<Vec>());

        auto closest = scopes_and_syms
            | genex::views::filter([min_depth](auto &&x) { return std::get<0>(x) == min_depth; })
            | genex::views::transform([](auto &&x) { return MakePair(std::get<1>(x), std::get<2>(x)); })
            | genex::to<Vec>();

        // Enforce visibility on the accessed member.
        if (not closest.IsEmpty()) {
            const auto scope = closest[0].First->NonGenericScope;
            CheckTypeMemberVisibility(*scope->GetVarSymbol(Name), *Name, *scope, *sm, *meta);
        }

        if (closest.Len() <= 1) { return; }
        Raise<analyse::errors::SppAmbiguousMemberAccessError>(
            {closest[0].First, closest[1].First, sm->CurrentScope},
            ERR_ARGS(*closest[0].Second->Name, *closest[1].Second->Name, *Name));
    }

    else {
        const auto lhs_as_ident = meta->PostfixExpressionLhs->To<IdentifierAst>();
        const auto lhs_var_sym = sm->CurrentScope->GetVarSymbol(AstClone(lhs_as_ident));

        // Check the lhs is a namespace and not a variable.
        RaiseIf<analyse::errors::SppMemberAccessRuntimeOperatorExpectedError>(
            lhs_var_sym != nullptr, {sm->CurrentScope},
            ERR_ARGS(*meta->PostfixExpressionLhs, *TokDblColon));

        // Check the constant exists inside the namespace.
        const auto lhs_ns_sym = sm->CurrentScope->ConvertPostfixToNestedScope(meta->PostfixExpressionLhs)->NsSym;
        if (not lhs_ns_sym->LinkedScope->HasVarSymbol(Name, true) and not lhs_ns_sym->LinkedScope->HasNsSymbol(Name, true)) {
            RaiseMissingIdentifierAndClosestOptions(
                *Name, lhs_ns_sym->LinkedScope->AllVarSymbols(false, true), lhs_ns_sym->LinkedScope->AllNsSymbols(), *sm);
        }

        // Enforce visibility on the accessed namespace symbol.
        // Only for var symbols, not namespace symbols.
        if (const auto sym = lhs_ns_sym->LinkedScope->GetVarSymbol(Name)) {
            CheckModuleMemberVisibility(*sym, *Name, *lhs_ns_sym->LinkedScope, *sm);
        }
    }
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Handle accessing a symbol on a type.
    if (const auto lhs_as_type = meta->PostfixExpressionLhs->To<TypeAst>(); lhs_as_type != nullptr) {
        const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(AstClone(lhs_as_type));
        const auto sym = lhs_type_sym->LinkedScope->GetVarSymbol(Name, true);
        auto tm = ScopeManager(sm->GlobalScope, lhs_type_sym->LinkedScope);
        sym->CompTimeValue->Stage9_CompTimeResolve(&tm, meta);
        meta->CmpResult = AstClone(meta->CmpResult);
        return;
    }

    // Handle accessing a variable on a namespace.
    // Todo: Do we need to call stage_9 on the value?
    const auto lhs = meta->PostfixExpressionLhs;
    const auto lhs_ns_sym = sm->CurrentScope->ConvertPostfixToNestedScope(lhs)->NsSym;
    const auto sym = lhs_ns_sym->LinkedScope->GetVarSymbol(Name, true);
    meta->CmpResult = AstClone(sym->CompTimeValue->To<ExpressionAst>());
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    const auto uid = spp::utils::Uid(this);

    // Type case: LHS is a TypeAst — access a cmp constant on the type's scope.
    if (const auto lhs_as_type = meta->PostfixExpressionLhs->To<TypeAst>(); lhs_as_type != nullptr) {
        const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(AstClone(lhs_as_type));
        const auto var_sym = lhs_type_sym->LinkedScope->GetVarSymbol(Name, true);
        if (var_sym->Type->IsCompilerGeneratedType()) { return nullptr; }
        SPP_ASSERT(var_sym->LlvmInfo->Alloca != nullptr);
        const auto global_var = llvm::cast<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca);
        return ctx->Builder.CreateLoad(global_var->getValueType(), global_var, "load.static_type_member" + uid);
    }

    // Namespace case: LHS is a namespace identifier — access a cmp constant in the namespace's scope.
    const auto lhs_ns_scope = sm->CurrentScope->ConvertPostfixToNestedScope(meta->PostfixExpressionLhs);
    const auto var_sym = lhs_ns_scope->GetVarSymbol(Name, true);
    if (var_sym->Type->IsCompilerGeneratedType()) { return nullptr; }
    SPP_ASSERT(var_sym->LlvmInfo->Alloca != nullptr);
    const auto global_var = llvm::cast<llvm::GlobalVariable>(var_sym->LlvmInfo->Alloca);
    return ctx->Builder.CreateLoad(global_var->getValueType(), global_var, "load.static_ns_member" + uid);
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    //
    using analyse::utils::type_utils::GetFwdTypes;

    // Get the left-hand-side type's member's type.
    if (const auto lhs_as_type = meta->PostfixExpressionLhs->To<TypeAst>(); lhs_as_type != nullptr) {
        const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(AstClone(lhs_as_type));
        const auto sym = lhs_type_sym->LinkedScope->GetVarSymbol(Name, true);
        if (sym != nullptr) { return sym->Type; }

        // This is where we need to handle the FwdRef/FwdMut logic.
        auto lhs_as_type_clone = AstCloneShared(lhs_as_type);
        auto [fwd_ref_type, _] = GetFwdTypes(*lhs_as_type_clone, *sm);
        const auto inner_type = fwd_ref_type->TypeParts().Back()->GnArgGroup->TypeAt("T")->Val;
        const auto inner_type_sym = sm->CurrentScope->GetTypeSymbol(inner_type);
        const auto fwd_sym = inner_type_sym->LinkedScope->GetVarSymbol(Name, true);
        return fwd_sym->Type;
    }

    // Get the left-hand-side namespace's member's type.
    const auto lhs_ns_scope = sm->CurrentScope->ConvertPostfixToNestedScope(meta->PostfixExpressionLhs);
    const auto type = lhs_ns_scope->GetVarSymbol(Name, true)->Type;
    return lhs_ns_scope->GetTypeSymbol(type)->FqName();
}

auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::ExprParts() const
    -> Vec<Ast*> {
    // Static member access does not have any expression parts.
    return {Name.get()};
}

SPP_MOD_END
