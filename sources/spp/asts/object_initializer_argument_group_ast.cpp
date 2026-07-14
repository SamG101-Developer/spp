module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_argument_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.analyse.utils.visibility_utils;
import spp.asts.class_attribute_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.algorithms;

SPP_MOD_BEGIN
auto spp::asts::ObjectInitializerArgumentGroupAst::NewEmpty()
    -> Unique<ObjectInitializerArgumentGroupAst> {
    // Factory for an empty object initializer group.
    return MakeUnique<ObjectInitializerArgumentGroupAst>(
        nullptr, decltype(Args)(), nullptr);
}

spp::asts::ObjectInitializerArgumentGroupAst::ObjectInitializerArgumentGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Args) &&args,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Args(std::move(args)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}

spp::asts::ObjectInitializerArgumentGroupAst::~ObjectInitializerArgumentGroupAst() = default;

auto spp::asts::ObjectInitializerArgumentGroupAst::PosStart() const
    -> std::size_t {
    // Use the "{" token.
    return TokL->PosStart();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::PosEnd() const
    -> std::size_t {
    // Use the "}" token.
    return TokR->PosEnd();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ObjectInitializerArgumentGroupAst>(
        AstClone(TokL), AstCloneVec(Args), AstClone(TokR));
}

auto spp::asts::ObjectInitializerArgumentGroupAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Args, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::ObjectInitializerArgumentGroupAst::Stage6_PreAnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppArgumentNameInvalidError;
    using analyse::errors::SppIdentifierDuplicateError;
    using analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError;
    using analyse::utils::type_utils::GetAllAttrs;

    const auto all_attrs = GetAllAttrs(*meta->ObjectInitType, sm);
    const auto all_attr_names = all_attrs
        | std::views::transform([](auto const &x) { return x.First; })
        | std::ranges::to<Vec>();

    // Check there is at most 1 autofill argument.
    const auto af_args = GetShorthandArgs()
        | std::views::filter([](auto const &x) { return x->TokEllipsis != nullptr; })
        | std::ranges::to<Vec>();

    RaiseIf<SppObjectInitializerMultipleAutofillArgumentsError>(
        af_args.Len() > 1, {sm->CurrentScope},
        ERR_ARGS(*af_args[0], *af_args[1]));

    // Ensure there are no duplicate member names. This needs to be done before semantic analysis as other ASTs might
    // try reading a duplicate attribute before an error is raised.
    const auto duplicates = GetKeywordArgs()
        | std::views::transform([](auto const &x) { return x->Name.Get(); })
        | spp::views::duplicates({}, spp::meta::deref)
        | std::ranges::to<Vec>();

    RaiseIf<SppIdentifierDuplicateError>(
        not duplicates.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*duplicates[0], *duplicates[1], "keyword object initializer argument"));

    // Check there are no invalidly named arguments.
    auto arg_names = GetNonAutoFillArgs()
        | std::views::transform([](auto const &x) { return x->Name.Get(); })
        | std::views::filter([](auto const * x) { return x != nullptr; }) // Filter bad unnamed args checked after.
        | std::ranges::to<Vec>();

    const auto invalid_args = arg_names
        | spp::views::not_in(all_attr_names, spp::meta::deref, spp::meta::deref)
        | std::ranges::to<Vec>();

    RaiseIf<SppArgumentNameInvalidError>(
        not invalid_args.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*meta->ObjectInitType, "attribute", *invalid_args[0], "object initializer argument"));

    // Analyse the arguments in the group.
    for (auto const &arg : Args) {
        // Return type overload helper.
        meta->Save();
        auto attr_type = static_cast<TypeAst*>(nullptr);
        if (const auto kw_arg = arg->To<ObjectInitializerArgumentKeywordAst>(); kw_arg != nullptr) {
            SPP_RETURN_TYPE_OVERLOAD_HELPER(arg->Val.Get()) {
                // Multiple attributes with same name (via base classes) -> can't infer the one to use.
                auto attrs = all_attrs
                    | std::views::filter([kw_arg](auto const &x) { return *x.First == *kw_arg->Name; })
                    | std::ranges::to<Vec>();
                if (attrs.Len() > 1) { continue; }

                // Use the type off the single matching attribute.
                const auto attr_type_sym = attrs[0].Second;
                attr_type = attr_type_sym->IsGeneric ? nullptr : attr_type_sym->FqName();
                meta->ReturnTypeOverloadResolverType = attr_type;
            }
        }

        arg->Stage7_AnalyseSemantics(sm, meta);
        meta->Restore();
    }
}

auto spp::asts::ObjectInitializerArgumentGroupAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::type_utils::TypeEq;
    using analyse::utils::type_utils::GetAllAttrs;
    using analyse::utils::visibility_utils::CheckTypeMemberVisibility;
    using analyse::errors::SppAmbiguousMemberAccessError;
    using analyse::errors::SppTypeMismatchError;

    // Get the attributes on the type and supertypes.
    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(meta->ObjectInitType);
    const auto all_attrs = GetAllAttrs(*meta->ObjectInitType, sm);

    // Type check the non-autofill arguments against the class attributes.
    for (auto const &arg : GetNonAutoFillArgs()) {
        auto matching_attrs = all_attrs
            | std::views::filter([&arg](auto const &x) { return *x.First == *arg->Name; })
            | std::ranges::to<Vec>();

        RaiseIf<SppAmbiguousMemberAccessError>(
            matching_attrs.Len() > 1, {sm->CurrentScope},
            ERR_ARGS(*matching_attrs[0].First, *matching_attrs[1].First, *this));

        auto [attr, attr_type_sym] = matching_attrs[0];

        // Enforce visibility on the field being initialized.
        {
            const auto scope = cls_sym->LinkedScope->NonGenericScope;
            const auto sym = scope->GetVarSymbol(arg->Name.Get(), true);
            CheckTypeMemberVisibility(*sym, *arg->Name, *scope, *sm, *meta);
        }

        const auto attr_type = attr_type_sym->FqName();
        meta->Save();
        meta->AssignmentTargetType = attr_type;
        meta->AssignmentTarget = IdentifierAst::FromType(*meta->AssignmentTargetType);
        auto arg_type = arg->InferType(sm, meta);
        meta->Restore();

        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*attr_type, *arg_type, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*attr, *attr_type, *arg, *arg_type));
    }

    // Type check the default argument (if it exists).
    if (const auto af_arg = GetAutoFillArg(); af_arg != nullptr) {
        const auto af_arg_type = af_arg->Val->InferType(sm, meta);
        RaiseIf<SppTypeMismatchError>( // Todo: pass a "meta->SourceObjectInitType" or just pass a "meta->ObjectInit"
            not TypeEq(*af_arg_type, *meta->ObjectInitType, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*meta->ObjectInitType, *meta->ObjectInitType, *af_arg, *af_arg_type));
    }
}

auto spp::asts::ObjectInitializerArgumentGroupAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Check the memory of the arguments.
    for (auto const &arg : Args) { arg->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetAllArgs()
    -> Vec<ObjectInitializerArgumentAst*> {
    // Get all arguments in the group.
    return Args
        | spp::views::ptr
        | std::ranges::to<Vec>();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetAutoFillArg()
    -> ObjectInitializerArgumentShorthandAst* {
    // Get the first shorthand argument tagged with the ".." token.
    const auto filtered = Args
        | spp::views::ptr
        | spp::views::cast_dynamic<ObjectInitializerArgumentShorthandAst*>
        | std::views::filter([](auto const &x) { return x != nullptr and x->TokEllipsis != nullptr; })
        | std::ranges::to<Vec>();
    return filtered.IsEmpty() ? nullptr : filtered[0];
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetNonAutoFillArgs()
    -> Vec<ObjectInitializerArgumentAst*> {
    // Get the first shorthand argument tagged with the ".." token.
    return Args
        | spp::views::ptr
        | spp::views::remove(GetAutoFillArg())
        | std::ranges::to<Vec>();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetShorthandArgs()
    -> Vec<ObjectInitializerArgumentShorthandAst*> {
    // Get all shorthand arguments tagged with the ".." token.
    return Args
        | spp::views::ptr
        | spp::views::cast_dynamic<ObjectInitializerArgumentShorthandAst*>
        | std::ranges::to<Vec>();
}

auto spp::asts::ObjectInitializerArgumentGroupAst::GetKeywordArgs()
    -> Vec<ObjectInitializerArgumentKeywordAst*> {
    // Get all keyword arguments.
    return Args
        | spp::views::ptr
        | spp::views::cast_dynamic<ObjectInitializerArgumentKeywordAst*>
        | std::ranges::to<Vec>();
}

SPP_MOD_END
