module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.analyse.utils.type_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.utils.algorithms;
import spp.utils.ptr;
import spp.utils.strings;
import genex;

auto spp::analyse::utils::type_utils::ConventionEq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type)
    -> bool {
    // Extract the conventions.
    const auto lhs_conv = lhs_type.GetConvention();
    const auto rhs_conv = rhs_type.GetConvention();

    // Quick exits based on existence of conventions.
    if (lhs_conv == nullptr and rhs_conv == nullptr) { return true; }
    if (lhs_conv == nullptr and rhs_conv != nullptr) { return false; }
    if (lhs_conv != nullptr and rhs_conv == nullptr) { return false; }

    // If the conventions are not equal, return false - but allow "&mut" (rhs) to coerce to "&" (lhs)
    if (*lhs_conv != rhs_conv) {
        const auto is_lhs_mut = *lhs_conv == asts::ConventionTag::MUT;
        const auto is_rhs_ref = *rhs_conv == asts::ConventionTag::REF;
        return not(is_lhs_mut and is_rhs_ref);
    }

    return true;
}

auto spp::analyse::utils::type_utils::ConstraintEq(
    Vec<Shared<asts::TypeAst>> const &constraints,
    asts::TypeAst const &type,
    scopes::Scope const &constraint_scope,
    scopes::Scope const &type_scope)
    -> bool {
    //
    if (constraints.IsEmpty()) { return true; }

    // Check that all the constraints are satisfied. Use the non-throwing variant: this is called from TypeEq, so paying
    // for exception machinery on every mismatch is horrible for performance.
    return EnforceGenericConstraintsOneArg(constraints, type, constraint_scope, type_scope) == nullptr;
}

auto spp::analyse::utils::type_utils::TypeEq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type,
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope,
    const bool check_variant)
    -> bool {
    // Special case for the "!" never type.
    using asts::generate::common_types_precompiled::VAR;
    if (rhs_type.IsNeverType()) { return true; }
    if (lhs_type.IsNeverType()) { return rhs_type.IsNeverType(); }
    if (lhs_type.IsSelfType() and rhs_type.IsSelfType()) { return true; }

    // Strip the generics from the types.
    const auto stripped_lhs = lhs_type.WithoutGenerics();
    const auto stripped_rhs = rhs_type.WithoutGenerics();

    // Get the non-generic symbols.
    const auto stripped_lhs_sym = (lhs_type.IsSelfType() ? rhs_scope : lhs_scope).GetTypeSymbol(stripped_lhs, false);
    const auto stripped_rhs_sym = (rhs_type.IsSelfType() ? lhs_scope : rhs_scope).GetTypeSymbol(stripped_rhs, false);
    const auto lhs_sym = lhs_scope.GetTypeSymbol(lhs_type.shared_from_this());

    // If the left-hand-side is a "Variant" type, check the composite types first.
    if (check_variant and TypeEq(*stripped_lhs_sym->FqName(), *VAR, lhs_scope, lhs_scope, false)) {
        auto lhs_composite_types = DedupVariableInnerTypes(*lhs_sym->FqName(), lhs_scope);
        if (genex::any_of(lhs_composite_types, [&](auto &&lhs_composite_type) { return TypeEq(*lhs_composite_type, rhs_type, lhs_scope, rhs_scope); })) {
            return true;
        }
    }

    if (not ConventionEq(lhs_type, rhs_type)) { return false; }

    // If the stripped types are not equal, check function-mock and forwarding compatibility before returning
    // false. A "$" mock type is a function value: match it structurally against the target function type via
    // its superimposed overload types ($ types are only ever generated for this purpose).
    if (stripped_lhs_sym->Type != stripped_rhs_sym->Type) {
        if (lhs_type.IsCompilerGeneratedType()) { return TypeFuncEq(lhs_type, rhs_type, lhs_scope, rhs_scope); }
        if (rhs_type.IsCompilerGeneratedType()) { return TypeFuncEq(rhs_type, lhs_type, rhs_scope, lhs_scope); }
        return TypeFwdEq(rhs_type, lhs_type, rhs_scope, lhs_scope);
    }
    auto &lhs_generics = lhs_type.TypeParts().Back()->GnArgGroup->Args;
    auto &rhs_generics = rhs_type.TypeParts().Back()->GnArgGroup->Args;

    // Special case for variadic parameter types.
    const auto temp_type_proto = lhs_sym->Type;
    if (temp_type_proto and not temp_type_proto->GnParamGroup->Params.IsEmpty()) {
        if (temp_type_proto->GnParamGroup->Params.Back()->To<asts::FunctionParameterVariadicAst>() != nullptr) {
            if (lhs_generics.Len() != rhs_generics.Len()) {
                return false;
            }
        }
    }

    // Ensure each generic argument is symbolically equal to the other.
    // Todo: why genex broke here?
    // Todo: different lengths?
    for (auto const &[lhs_generic, rhs_generic] : std::views::zip(lhs_generics, rhs_generics)) {
        if (lhs_generic->To<asts::GenericArgumentTypeAst>()) {
            const auto lhs_generic_part = lhs_generic->To<asts::GenericArgumentTypeAst>();
            const auto rhs_generic_part = rhs_generic->To<asts::GenericArgumentTypeAst>();
            if (not TypeEq(*lhs_generic_part->Val, *rhs_generic_part->Val, lhs_scope, rhs_scope)) { return false; }
        }
        else {
            const auto lhs_generic_part = lhs_generic->To<asts::GenericArgumentCompAst>();
            const auto rhs_generic_part = rhs_generic->To<asts::GenericArgumentCompAst>();
            if (not TypeEq(*lhs_generic_part->Val, *rhs_generic_part->Val, lhs_scope, rhs_scope)) { return false; }
        }
    }

    // If all the generic arguments are symbolically equal, return true.
    return true;
}

auto spp::analyse::utils::type_utils::TypeEq(
    asts::ExpressionAst const &lhs_expr,
    asts::ExpressionAst const &rhs_expr,
    scopes::Scope const &,
    scopes::Scope const &)
    -> bool {
    // Simple equality between the expressions.
    return lhs_expr == rhs_expr;
}

auto spp::analyse::utils::type_utils::TypeFwdEq(
    asts::TypeAst const &arg_type,
    asts::TypeAst const &param_type,
    scopes::Scope const &arg_scope,
    scopes::Scope const &param_scope)
    -> bool {
    //
    using asts::generate::common_types_precompiled::FWD_REF;
    using asts::generate::common_types_precompiled::FWD_MUT;

    // Ensure that the conventions match between the two types.
    const auto arg_conv = arg_type.GetConvention();
    const auto param_conv = param_type.GetConvention();
    if (arg_conv == nullptr or param_conv == nullptr) { return false; }

    // Determine if we are targeting a forward ref or mut variation.
    const auto is_both_ref = (*arg_conv == asts::ConventionTag::REF) and (*param_conv == asts::ConventionTag::REF);
    const auto is_both_mut = (*arg_conv == asts::ConventionTag::MUT) and (*param_conv == asts::ConventionTag::MUT);
    if (not is_both_ref and not is_both_mut) { return false; }

    // Generic types won't forward, so ignore them here.
    // Todo: maybe allow via constraints at some point.
    const auto &fwd_target = is_both_ref ? FWD_REF : FWD_MUT;
    const auto arg_bare = arg_type.WithoutConvention();
    const auto arg_bare_sym = arg_scope.GetTypeSymbol(arg_bare);
    if (arg_bare_sym->IsGeneric) { return false; }

    // Get all the super types that we want to consider.
    auto sup_types = Vec{arg_bare};
    sup_types.AppendRange(arg_bare_sym->LinkedScope->SupTypes());

    // Check for a matching forwarding type, and compare to the inner type of it (the forwarding target).
    // Todo: ensure only 1 forwarding superimposition is present for a given type.
    // Todo: probably ensure that the ref & mut both forward to the same type?
    for (auto const &sup_type : sup_types) {
        if (not TypeEq(*sup_type->WithoutGenerics(), *fwd_target, arg_scope, arg_scope, false)) { continue; }
        const auto inner_type = sup_type->TypeParts().Back()->GnArgGroup->TypeAt("T")->Val->WithConvention(asts::AstClone(param_type.GetConvention()));
        if (TypeEq(*inner_type, param_type, param_scope, param_scope)) { return true; }
    }

    // Otherwise, there is no forwarding match.
    return false;
}

auto spp::analyse::utils::type_utils::TypeFuncEq(
    asts::TypeAst const &mock_type,
    asts::TypeAst const &func_type,
    scopes::Scope const &mock_scope,
    scopes::Scope const &func_scope)
    -> bool {
    // A "$" mock type is generated per function and superimposes a function type for each of its overloads
    // (and, because super types are transitive, the whole FunMov/FunMut/FunRef hierarchy above each). It
    // matches the target function type if any of those superimposed function types is equal to the target.
    const auto mock_sym = mock_scope.GetTypeSymbol(mock_type.WithoutConvention());
    if (mock_sym == nullptr) { return false; }

    for (auto const &sup_type : mock_sym->LinkedScope->SupTypes()) {
        if (IsTypeFunc(*sup_type, mock_scope) and TypeEq(*sup_type, func_type, mock_scope, func_scope)) {
            return true;
        }
    }
    return false;
}

auto spp::analyse::utils::type_utils::RelaxedTypeEq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type,
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope,
    GenericInferenceMap &generic_args,
    const bool check_variant,
    const bool check_constraints) -> bool {
    // Strip the generics from the right-hand-side type (possible generic).
    using asts::generate::common_types_precompiled::VAR;
    const auto stripped_lhs = mut_shared_cast(lhs_type.WithoutGenerics()->WithoutConvention());
    const auto stripped_rhs = mut_shared_cast(rhs_type.WithoutGenerics()->WithoutConvention());

    // If the right-hand-side is generic, then return a match: "sup[T] T { ... }" matches all types.
    const auto stripped_rhs_sym = rhs_scope.GetTypeSymbol(stripped_rhs);
    if (stripped_rhs_sym->IsGeneric) {
        const auto t = dynamic_shared_cast<asts::TypeIdentifierAst>(stripped_rhs);
        generic_args.insert({t, const_cast<asts::TypeAst*>(&lhs_type)});
        if (check_constraints and not ConstraintEq(stripped_rhs_sym->GenericConstraints, lhs_type, rhs_scope, lhs_scope)) { return false; }
        return true;
    }

    // TODO: Deliberately inverted for the param/arg checker. This will be removed once that type check is actually done
    //  properly.
    if (not ConventionEq(rhs_type, lhs_type)) { return false; }

    const auto stripped_lhs_sym = lhs_scope.GetTypeSymbol(stripped_lhs);
    if (stripped_lhs_sym->IsGeneric) {
        const auto t = dynamic_shared_cast<asts::TypeIdentifierAst>(stripped_lhs);
        generic_args.insert({t, const_cast<asts::TypeAst*>(&rhs_type)});
        if (check_constraints and not ConstraintEq(stripped_lhs_sym->GenericConstraints, rhs_type, lhs_scope, rhs_scope)) { return false; }
        return true;
    }

    // If the right-hand-side is a "Variant" type, check the composite types first.
    // Todo: on the failure of a variant match in "any_of", does the generic map need rolling back?
    if (check_variant and TypeEq(*VAR, *stripped_rhs_sym->FqName()->WithoutGenerics(), rhs_scope, rhs_scope)) {
        auto rhs_composite_types = DedupVariableInnerTypes(*rhs_scope.GetTypeSymbol(rhs_type.shared_from_this())->FqName(), rhs_scope);
        if (genex::any_of(rhs_composite_types, [&](auto &&rhs_composite_type) { return RelaxedTypeEq(lhs_type, *rhs_composite_type, lhs_scope, rhs_scope, generic_args); })) {
            return true;
        }
    }

    // If the stripped types aren't equal, then return false.
    if (stripped_lhs_sym->Type != stripped_rhs_sym->Type) { return false; }
    auto &lhs_generics = lhs_type.TypeParts().Back()->GnArgGroup->Args;
    auto &rhs_generics = rhs_type.TypeParts().Back()->GnArgGroup->Args;

    // Special case for variadic parameter types.
    const auto temp_type_proto = lhs_scope.GetTypeSymbol(lhs_type.shared_from_this())->Type;
    if (temp_type_proto and not temp_type_proto->GnParamGroup->Params.IsEmpty()) {
        if (temp_type_proto->GnParamGroup->Params.Back()->To<asts::FunctionParameterVariadicAst>() != nullptr) {
            if (lhs_generics.Len() != rhs_generics.Len()) { return false; }
        }
    }

    // Ensure each generic argument is symbolically equal to the other. Todo: why genex broke here?
    for (auto [lhs_generic, rhs_generic] : std::views::zip(lhs_generics, rhs_generics)) {
        if (rhs_generic->To<asts::GenericArgumentTypeAst>()) {
            const auto lhs_generic_part = lhs_generic->To<asts::GenericArgumentTypeAst>();
            const auto rhs_generic_part = rhs_generic->To<asts::GenericArgumentTypeAst>();
            if (not RelaxedTypeEq(*lhs_generic_part->Val, *rhs_generic_part->Val, lhs_scope, rhs_scope, generic_args, check_variant, check_constraints)) { return false; }
        }
        else {
            const auto lhs_generic_part = lhs_generic->To<asts::GenericArgumentCompAst>();
            const auto rhs_generic_part = rhs_generic->To<asts::GenericArgumentCompAst>();
            if (not RelaxedTypeEq(*lhs_generic_part->Val, *rhs_generic_part->Val, lhs_scope, rhs_scope, generic_args)) { return false; }
        }
    }

    // If all the generic arguments are symbolically equal, return true.
    return true;
}

auto spp::analyse::utils::type_utils::RelaxedTypeEq(
    asts::ExpressionAst const &lhs_expr,
    asts::ExpressionAst const &rhs_expr,
    scopes::Scope const &,
    scopes::Scope const &,
    GenericInferenceMap &generic_args)
    -> bool {
    // Simple equality between the expressions, with generic matching.
    if (const auto rhs_expr_as_identifier = rhs_expr.To<asts::IdentifierAst>()) {
        generic_args[asts::TypeIdentifierAst::FromIdentifier(*rhs_expr_as_identifier)] = const_cast<asts::ExpressionAst*>(&lhs_expr);
        return true;
    }
    return lhs_expr == rhs_expr;
}

auto spp::analyse::utils::type_utils::IsTypeCompTimeIndexable(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Test for the tuple or array type.
    return
        IsTypeTup(*type.WithoutGenerics(), scope) or IsTypeArr(*type.WithoutGenerics(), scope);
}

auto spp::analyse::utils::type_utils::IsTypeArr(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::array::Arr[T, n]".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::ARR, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeTup(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::tuple::Tup[Ts...]".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::TUP, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeVariant(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::variant::Variant[Ts...]".
    return
        TypeEq(*type.WithoutConvention()->WithoutGenerics(), *asts::generate::common_types_precompiled::VAR, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeBool(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::bool::Bool".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::BOOL, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeVoid(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::void::Void".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::VOID, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeNever(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::void::Void".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::NEVER, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeGen(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::generator::Gen[T]/GenOpt[T]/GenRes[T, E]".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::GEN, scope, scope) or
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::GEN_ONCE, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeSelf(
    asts::TypeAst const &type)
    -> bool {
    // Check for a string match to "Self".
    const auto type_identifier = type.To<asts::TypeIdentifierAst>();
    return type_identifier != nullptr and type_identifier->Name == "Self";
}

auto spp::analyse::utils::type_utils::IsTypeRuntimeIndexable(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Test for the type against "std::iter::IndexRef[T]/IndexMut[T]".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::INDEX_REF, scope, scope) or
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::INDEX_MUT, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeTry(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::try::Try[Ok, Err]".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::TRY, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeFunc(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::function::FunMov[Ret, Args]/FunMut[Ret, Args]/FunRef[Ret, Args]".
    return
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::FUN_MOV, scope, scope) or
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::FUN_MUT, scope, scope) or
        TypeEq(*type.WithoutGenerics(), *asts::generate::common_types_precompiled::FUN_REF, scope, scope);
}

auto spp::analyse::utils::type_utils::IsTypeRecursive(
    asts::ClassPrototypeAst const &type,
    scopes::ScopeManager const &sm)
    -> Shared<asts::TypeAst> {
    // Get the attribute types recursively from the class prototype, and check for a match with the class prototype.
    // Use the source type as this is used for error reporting.
    auto attr_info = Vec<Pair<Shared<scopes::TypeSymbol>, asts::ClassAttributeAst*>>{};
    GetAttrTypes(&type, sm.CurrentScope, attr_info);
    for (auto const &[attr_type_sym, attr_ast] : attr_info) {
        if (attr_type_sym == type.GetClsSym()) {
            return attr_ast->Source.OriginalType;
        }
    }

    // No recursive type was found, so return nullptr.
    return nullptr;
}

auto spp::analyse::utils::type_utils::IsTypeBorrowed(
    asts::TypeAst const &type,
    scopes::ScopeManager const &sm,
    const bool deep)
    -> bool {
    // Check that either this type, or any inner types for variants, are "&" or "&mut".
    using asts::generate::common_types_precompiled::VAR;
    if (type.GetConvention() != nullptr) { return true; }
    if (type.IsSelfType()) { return false; }
    // if (type.IsCompilerGeneratedType()) { return false; }

    // Check the inner types for variant types.
    if (deep and TypeEq(*type.WithoutGenerics(), *VAR, *sm.CurrentScope, *sm.CurrentScope, false)) {
        for (auto const &inner_type_arg : type.TypeParts().Back()->GnArgGroup->GetTypeArgs()) {
            if (IsTypeBorrowed(*inner_type_arg->Val, sm)) { return true; }
        }
    }
    return false;
}

auto spp::analyse::utils::type_utils::IsTypeCopyable(
    asts::TypeAst const &type,
    scopes::ScopeManager const &sm)
    -> bool {
    // Generic types are not Copy types, so return nullptr.
    const auto type_sym = sm.CurrentScope->GetTypeSymbol(type.shared_from_this());
    if (type_sym->IsGeneric) { return false; }

    // Discover the supertypes and add the current type to it.
    auto sup_types = Vec{type.shared_from_this()};
    sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

    auto inner_copy_check = [&](auto &&t) {
        return TypeEq(*t.WithoutGenerics(), *asts::generate::common_types_precompiled::COPY, *sm.CurrentScope, *sm.CurrentScope, false);
    };

    // Search through the supertypes for a direct Copy type.
    const auto copy_type_candidates = sup_types
        | genex::views::filter([&](auto &&sup_type) { return inner_copy_check(*sup_type); })
        | genex::to<Vec>();

    // If there is an explicit Copy type, return true, otherwise return false.
    return not copy_type_candidates.IsEmpty();
}

auto spp::analyse::utils::type_utils::GetAttrTypes(
    const asts::ClassPrototypeAst *cls_proto,
    const scopes::Scope *cls_scope,
    Vec<Pair<Shared<scopes::TypeSymbol>, asts::ClassAttributeAst*>> &attr_symbols)
    -> void {
    // Get all attribute types, without recursion errors (this will be handled elsewhere).
    for (auto const &member : cls_proto->Impl->Members | genex::views::ptr | genex::views::cast_dynamic<asts::ClassAttributeAst*>) {
        auto type_sym = cls_scope->GetTypeSymbol(member->Type);
        if (genex::contains(attr_symbols, type_sym, [](auto &&x) { return x.First; })) { continue; }
        if (type_sym->IsGeneric) { continue; }

        attr_symbols.EmplaceBack(type_sym, member);
        GetAttrTypes(type_sym->Type, type_sym->LinkedScope, attr_symbols);
    }
}

auto spp::analyse::utils::type_utils::IsIndexWithinBound(
    const std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Pair<bool, std::size_t> {
    // For tuples, count the number of generic arguments.
    using errors::SppInternalCompilerError;
    if (IsTypeTup(type, scope)) {
        auto elems = type.TypeParts().Back()->GnArgGroup->Args.Len();
        return MakePair(index < elems, elems);
    }

    // For arrays, check the size argument.
    if (IsTypeArr(type, scope)) {
        const auto size_arg = type.TypeParts().Back()->GnArgGroup->CompAt("n");
        const auto size_arg_cast = size_arg->Val->To<asts::IntegerLiteralAst>();
        const auto elems = std::stoul(size_arg_cast->Val->TokenData);
        return MakePair(index < elems, elems);
    }

    Raise<SppInternalCompilerError>(
        {&scope},
        ERR_ARGS(type, "Non indexable type used in index check"));
}

auto spp::analyse::utils::type_utils::GetNthTypeOfIndexableType(
    const std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Shared<asts::TypeAst> {
    // For tuples, return the nth generic argument.
    using errors::SppInternalCompilerError;
    if (IsTypeTup(type, scope)) {
        return type.TypeParts().Back()->GnArgGroup->GetTypeArgs()[index]->Val;
    }

    // For arrays, return the element type.
    if (IsTypeArr(type, scope)) {
        return type.TypeParts().Back()->GnArgGroup->GetTypeArgs()[0]->Val;
    }

    Raise<SppInternalCompilerError>(
        {&scope},
        ERR_ARGS(type, "Non indexable type used in index access"));
}

auto spp::analyse::utils::type_utils::GetFunctionalType(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Shared<const asts::TypeAst> {
    //
    const auto type_sym = scope.GetTypeSymbol(type.shared_from_this());

    // Check the type itself and all its supertypes (a type
    // superimposing a function type is also callable).
    auto sup_types = Vec{type.shared_from_this()};
    sup_types.AppendRange(type_sym->LinkedScope->SupTypes());
    for (auto const &sup_type : sup_types) {
        if (IsTypeFunc(*sup_type, scope)) { return sup_type; }
    }

    return nullptr;
}

auto spp::analyse::utils::type_utils::GetGenAndYieldTypes(
    asts::TypeAst const &type,
    scopes::Scope const &scope,
    asts::ExpressionAst const &expr,
    StrView what)
    -> std::tuple<Shared<const asts::TypeAst>, Shared<asts::TypeAst>, bool> {
    //
    using asts::generate::common_types_precompiled::GEN_ONCE;
    using errors::SppExpressionNotGeneratorError;
    using errors::SppExpressionAmbiguousGeneratorError;

    // Generic types are not generators, so raise an error.
    const auto type_sym = scope.GetTypeSymbol(type.shared_from_this());

    // Discover the supertypes and add the current type to it.=.
    auto sup_types = Vec{type.shared_from_this()};
    sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

    // Search through the supertypes for a direct generator type.
    const auto generator_type_candidates = sup_types
        | genex::views::filter([&](auto const &sup_type) { return IsTypeGen(*sup_type, scope); })
        | genex::to<Vec>();

    RaiseIf<SppExpressionNotGeneratorError>(
        generator_type_candidates.IsEmpty(), {&scope}, ERR_ARGS(expr, type, what));

    RaiseIf<SppExpressionAmbiguousGeneratorError>(
        generator_type_candidates.Len() > 1, {&scope}, ERR_ARGS(expr, type, what));

    // Extract the generator and yield type.
    auto generator_type = generator_type_candidates[0];
    auto yield_type = generator_type->TypeParts().Back()->GnArgGroup->TypeAt("Yield")->Val;

    // Extract the multiplicity, optionality and fallibility from the generator type.
    auto is_once = TypeEq(
        *GEN_ONCE, *generator_type->WithoutGenerics(), scope, scope);

    // Return all the information about the generator type.
    return std::make_tuple(generator_type, yield_type, is_once);
}

auto spp::analyse::utils::type_utils::GetTryType(
    asts::TypeAst const &type,
    asts::ExpressionAst const &expr,
    scopes::ScopeManager const &sm)
    -> Shared<const asts::TypeAst> {
    // Generic types are not Try types, so return nullptr.
    const auto type_sym = sm.CurrentScope->GetTypeSymbol(type.shared_from_this());
    if (type_sym->IsGeneric) { return nullptr; }

    // Discover the supertypes and add the current type to it.
    auto sup_types = Vec{type.shared_from_this()};
    sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

    // Search through the supertypes for a direct Try type.
    const auto try_type_candidates = sup_types
        | genex::views::filter([&sm](auto &&sup_type) { return IsTypeTry(*sup_type, *sm.CurrentScope); })
        | genex::to<Vec>();

    RaiseIf<errors::SppEarlyReturnRequiresTryTypeError>(
        try_type_candidates.IsEmpty(), {sm.CurrentScope}, ERR_ARGS(expr, type));

    // Extract the Try type and return it.
    return try_type_candidates[0];
}

auto spp::analyse::utils::type_utils::GetFwdTypes(
    asts::TypeAst const &type,
    scopes::ScopeManager const &sm)
    -> Pair<Shared<const asts::TypeAst>, Shared<const asts::TypeAst>> {
    //
    using asts::generate::common_types_precompiled::FWD_MUT;
    using asts::generate::common_types_precompiled::FWD_REF;

    // Generic types do not have forward types, so return nullptr.
    const auto type_sym = sm.CurrentScope->GetTypeSymbol(type.shared_from_this());
    if (type_sym->IsGeneric) { return {nullptr, nullptr}; }

    // Discover the supertypes and add the current type to it.
    auto sup_types = Vec{type.shared_from_this()};
    sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

    // Search through the supertypes for a direct FwdRef type.
    const auto fwd_ref_type_candidates = sup_types
        | genex::views::filter([&sm](auto const &sup_type) { return TypeEq(*sup_type->WithoutGenerics(), *FWD_REF, *sm.CurrentScope, *sm.CurrentScope); })
        | genex::to<Vec>();

    // Search through the supertypes for a direct FwdMut type.
    const auto fwd_mut_type_candidates = sup_types
        | genex::views::filter([&sm](auto const &sup_type) { return TypeEq(*sup_type->WithoutGenerics(), *FWD_MUT, *sm.CurrentScope, *sm.CurrentScope); })
        | genex::to<Vec>();

    // No error raised here; just return the pair of types (nullptr if not found).
    auto fwd_ref_type = fwd_ref_type_candidates.IsEmpty() ? nullptr : fwd_ref_type_candidates[0];
    auto fwd_mut_type = fwd_mut_type_candidates.IsEmpty() ? nullptr : fwd_mut_type_candidates[0];
    return MakePair(fwd_ref_type, fwd_mut_type);
}

auto spp::analyse::utils::type_utils::ValidateInconsistentTypes(
    Vec<asts::CaseExpressionBranchAst*> const &branches,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<Pair<asts::Ast*, Shared<asts::TypeAst>>, Vec<Pair<asts::Ast*, Shared<asts::TypeAst>>>> {
    //
    using errors::SppTypeMismatchError;

    // Collect type information for each branch, pairing the branch with its inferred type.
    auto branches_type_info = branches
        | genex::views::transform([sm, meta](auto *x) { return MakePair(x, x->InferType(sm, meta)); })
        | genex::to<Vec>();

    // Filter the branch types down to variant types for custom analysis.
    auto variant_branches_type_info = branches_type_info
        | genex::views::filter([sm](auto &&x) { return type_utils::IsTypeVariant(*x.Second, *sm->CurrentScope); })
        | genex::to<Vec>();

    // Set the master branch type to the first branch's type, if it exists. This is the default and may be subsequently changed.
    auto master_branch_type_info = not branches.IsEmpty()
        ? MakePair(branches_type_info[0].First, branches_type_info[0].Second)
        : MakePair<asts::CaseExpressionBranchAst*, Shared<asts::TypeAst>>(nullptr, nullptr);

    // Override the master type if a pre-provided type (for assignment) has been given.
    if (meta->AssignmentTargetType != nullptr) {
        master_branch_type_info = MakePair(nullptr, meta->AssignmentTargetType);
    }

    // Otherwise, if there are variant branches, use the most variant type as the master branch type.
    else if (not variant_branches_type_info.IsEmpty()) {
        auto most_inner_types = 0uz;
        for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
            const auto variant_size = variant_type->TypeParts().Back()->GnArgGroup->TypeAt("Variant")->Val->TypeParts().Back()->GnArgGroup->Args.Len();
            if (variant_size > most_inner_types) {
                master_branch_type_info = MakePair(variant_branch, variant_type);
                most_inner_types = variant_size;
            }
        }
    }

    // Remove the master branch pointer from the list of remaining branch types and check all types match.
    // Todo: Shouldn't need to auto-remove "!" type, because TypeEq handles it?
    auto mismatch_branches_type_info = branches_type_info
        | genex::views::remove_if([&](auto const &x) { return TypeEq(*asts::generate::common_types_precompiled::NEVER, *x.Second, *sm->CurrentScope, *sm->CurrentScope); })
        | genex::views::remove_if([&](auto const &x) { return x.First == master_branch_type_info.First; })
        | genex::views::remove_if([&](auto const &x) { return TypeEq(*master_branch_type_info.Second, *x.Second, *sm->CurrentScope, *sm->CurrentScope); })
        | genex::to<Vec>();

    if (not mismatch_branches_type_info.IsEmpty()) {
        const auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
        const auto [master_branch, master_branch_type] = master_branch_type_info;
        const auto final_member = master_branch ? master_branch->Body->FinalMember() : meta->AssignmentTarget.get();
        Raise<SppTypeMismatchError>(
            {sm->CurrentScope},
            ERR_ARGS(*final_member, *master_branch_type, *mismatch_branch->Body->FinalMember(), *mismatch_branch_type));
    }

    // Cast to common AST nodes and return with the types.
    auto cast_master_branch_type_info = MakePair(master_branch_type_info.First->template To<asts::Ast>(), master_branch_type_info.Second);
    auto cast_branches_type_info = branches_type_info
        | genex::views::transform([](auto &&x) { return MakePair(x.First->template To<asts::Ast>(), x.Second); })
        | genex::to<Vec>();
    return std::make_tuple(cast_master_branch_type_info, cast_branches_type_info);
}

auto spp::analyse::utils::type_utils::GetAllAttrs(
    asts::TypeAst const &type,
    scopes::ScopeManager const *sm)
    -> Vec<std::tuple<Shared<asts::IdentifierAst>, Shared<scopes::TypeSymbol>, scopes::Scope*>> {
    // Get the symbol of the class type.
    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(type.shared_from_this());

    // Get the attribute information from the class type and all super types.
    auto all_scopes = Vec{cls_sym->LinkedScope};
    all_scopes.AppendRange(cls_sym->LinkedScope->SupScopes());

    const auto all_syms = all_scopes
        | genex::views::filter([](auto &&sup_scope) { return sup_scope->AstNode->template To<asts::ClassPrototypeAst>() != nullptr; })
        | genex::views::transform([](auto &&sup_scope) { return MakePair(sup_scope, sup_scope->AllVarSymbols(true)); })
        | genex::to<Vec>();

    auto extended_syms = Vec<std::tuple<Shared<asts::IdentifierAst>, Shared<scopes::TypeSymbol>, scopes::Scope*>>{};
    for (auto const &[sup_scope, syms] : all_syms) {
        for (auto const &sym : syms) {
            if (sym->IsGeneric) { continue; }
            const auto sym_type = sup_scope->GetTypeSymbol(sym->Type);
            extended_syms.EmplaceBack(sym->Name, sym_type, sup_scope);
        }
    }

    return extended_syms;
}

static auto SupMemberAsMethod(
    spp::asts::Ast const *member)
    -> spp::asts::FunctionPrototypeAst const* {
    if (const auto fn = member->To<spp::asts::FunctionPrototypeAst>(); fn != nullptr) { return fn; }
    if (const auto ext = member->To<spp::asts::SupPrototypeExtensionAst>(); ext != nullptr and ext->Impl != nullptr) {
        const auto final_member = ext->Impl->FinalMember();
        return final_member != nullptr ? final_member->To<spp::asts::FunctionPrototypeAst>() : nullptr;
    }
    return nullptr;
}

auto spp::analyse::utils::type_utils::GetUnimplementedAbstractMethods(
    scopes::Scope const &type_scope)
    -> Vec<asts::FunctionPrototypeAst const*> {
    // Skip on functional types because of the awkward difference with the base method having "args: Args" tuple of
    // args, and implementations having their own individual args.
    // Todo: Autopack and check?
    if (type_scope.TySym != nullptr) {
        if (type_scope.TySym->Name->IsCompilerGeneratedType()) { return {}; }
        if (const auto fq_name = type_scope.TySym->FqName(); fq_name != nullptr and IsTypeFunc(*fq_name, type_scope)) { return {}; }
    }

    // Gather every method visible on the type, from the type's own scope and from all of its super scopes, each tagged
    // with the scope that resolves the types in its signature.
    auto all_scopes = Vec<scopes::Scope const*>{&type_scope};
    all_scopes.AppendRange(type_scope.SupScopes());

    auto methods = Vec<Pair<scopes::Scope const*, asts::FunctionPrototypeAst const*>>();
    for (auto const *scope : all_scopes) {
        if (scope->AstNode == nullptr) { continue; }

        const auto impl = scope->AstNode->To<asts::ClassPrototypeAst>() == nullptr
            ? asts::AstBody(scope->AstNode)
            : Vec<asts::Ast*>{};
        if (impl.IsEmpty()) { continue; }

        for (const auto member : impl) {
            const auto fn = SupMemberAsMethod(member);
            if (fn == nullptr) { continue; }

            const auto method_scope = genex::find_if(
                scope->Children, [member](auto const &child) { return child->AstNode == member; });
            if (method_scope == scope->Children.end()) { continue; }

            methods.EmplaceBack(method_scope->get(), fn);
        }
    }

    // A method stays abstract only while nothing anywhere on the type implements its signature.
    auto unimplemented = Vec<asts::FunctionPrototypeAst const*>();
    for (auto const &[abs_scope, abs_fn] : methods) {
        if (abs_fn->AbstractAnnotation == nullptr) { continue; }
        const auto is_implemented = genex::any_of(methods, [&](auto const &other) {
            return other.Second->AbstractAnnotation == nullptr
                and func_utils::SameSignature(*other.Second, *other.First, *abs_fn, *abs_scope);
        });

        if (not is_implemented) { unimplemented.EmplaceBack(abs_fn); }
    }

    return unimplemented;
}

auto spp::analyse::utils::type_utils::GetAllAttrAsts(
    asts::TypeAst const &type,
    scopes::ScopeManager const *sm)
    -> Vec<asts::ClassAttributeAst*> {
    // Get the symbol of the class type.
    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(type.shared_from_this());

    // Walk the class and its super types in the same order as "GetAllAttrs", so the results line up index for index.
    auto all_scopes = Vec{cls_sym->LinkedScope};
    all_scopes.AppendRange(cls_sym->LinkedScope->SupScopes());

    auto attr_asts = Vec<asts::ClassAttributeAst*>{};
    for (auto const &sup_scope : all_scopes) {
        const auto cls_proto = sup_scope->AstNode->To<asts::ClassPrototypeAst>();
        if (cls_proto == nullptr) { continue; }
        for (auto const &member : cls_proto->Impl->Members) {
            if (const auto attr = member->To<asts::ClassAttributeAst>(); attr != nullptr) {
                attr_asts.EmplaceBack(attr);
            }
        }
    }

    return attr_asts;
}

auto spp::analyse::utils::type_utils::CreateGenericClsScope(
    asts::TypeIdentifierAst &type_part,
    scopes::TypeSymbol const &old_cls_sym,
    Vec<Shared<scopes::Symbol>> const &external_generic_syms,
    const bool is_tuple,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> scopes::Scope* {
    // Create a new scope and symbol for the generic substituted type.
    const auto old_cls_scope = old_cls_sym.LinkedScope ? : old_cls_sym.ScopeDefinedIn;
    auto new_cls_scope = MakeUnique<scopes::Scope>(
        scopes::ScopeTypeIdentifierName(type_part.shared_from_this()),
        old_cls_scope->Parent, old_cls_scope->AstNode);

    // Note there is no LLVM type propagation: handled separately before stage 10.
    const auto new_cls_sym = MakeShared<scopes::TypeSymbol>(
        dynamic_shared_cast<asts::TypeIdentifierAst>(type_part.shared_from_this()),
        new_cls_scope->AstNode->To<asts::ClassPrototypeAst>(), new_cls_scope.get(), sm->CurrentScope,
        old_cls_scope->Parent, old_cls_sym.IsGeneric, old_cls_sym.IsDirectlyCopyable, old_cls_sym.Visibility);
    const auto new_cls_scope_ptr = new_cls_scope.get();

    new_cls_sym->IsCopyable = [old_cls_sym] { return old_cls_sym.IsCopyable(); };
    const auto raw_new_cls_sym = new_cls_sym.get();
    new_cls_sym->IsZeroType = [old_cls_sym, raw_new_cls_sym] { return raw_new_cls_sym->IsDirectlyZeroType or old_cls_sym.IsZeroType(); };
    auto new_alias_stmt = asts::AstClone(old_cls_sym.AliasStmt);
    if (new_alias_stmt) {
        new_alias_stmt->MappedOldType = new_alias_stmt->MappedOldType->SubstituteGenerics(type_part.GnArgGroup->GetAllArgs());
        new_alias_stmt->OldType = new_alias_stmt->MappedOldType;
        new_alias_stmt->OldType->Stage7_AnalyseSemantics(sm, meta);
        // TODO: Remove generic parameters that have been given arguments (not always all generic args).
        //  Move the argument filter out of the recursive alias searcher and reuse it here.

        const auto target_scope = new_alias_stmt->GetAstScope()->Parent;
        target_scope->AddTypeSymbol(new_cls_sym);
        new_alias_stmt->_TrackingScope->AddTypeSymbol(new_cls_sym);
        new_alias_stmt->_TrackingScope->Children.EmplaceBack(std::move(new_cls_scope));
        new_cls_sym->AliasStmt = std::move(new_alias_stmt);
    }

    // Configure the new scope based on the base (old) scope.
    else {
        new_cls_scope->Parent->AddTypeSymbol(new_cls_sym);
        new_cls_scope->Parent->Children.EmplaceBack(std::move(new_cls_scope));
    }
    new_cls_scope_ptr->TySym = new_cls_sym;
    new_cls_scope_ptr->InternalTable = old_cls_scope->InternalTable;
    new_cls_scope_ptr->NonGenericScope = old_cls_scope;

    if (meta->CurrentStage > 7) {
        sm->AttachSpecificSuperScopes(*new_cls_scope_ptr, meta);
    }

    // No more checks for tuples.
    auto new_ast = asts::AstClone(old_cls_scope->AstNode->To<asts::ClassPrototypeAst>());
    new_ast->SetAstScope(new_cls_scope_ptr);
    new_ast->GnParamGroup->Params.Clear();
    const auto new_ast_ptr = new_ast.get();

    old_cls_sym.Type->RegisterGenericSubstitution(new_cls_scope_ptr, std::move(new_ast));
    if (is_tuple) {
        return new_cls_scope_ptr;
    }

    // Register the generic symbols.
    RegisterGenericSyms(external_generic_syms, type_part.GnArgGroup->Args, new_cls_scope_ptr, sm, meta);

    // Run generic substitution on the symbols in the scope.
    const auto fq_type = new_cls_sym->FqName();
    auto substitution_generics = fq_type->TypeParts().Back()->GnArgGroup->GetAllArgs();
    substitution_generics.AppendRange(type_part.GnArgGroup->GetAllArgs());

    // Substitute the "Self" sym.
    const auto self_type = asts::AstName(old_cls_scope->AstNode)->SubstituteGenerics(type_part.GnArgGroup->GetAllArgs());
    const auto new_self_sym = MakeShared<scopes::TypeSymbol>(
        MakeUnique<asts::TypeIdentifierAst>(0uz, "Self", nullptr), sm->SelfProto(), new_cls_scope_ptr,
        new_cls_scope_ptr);
    new_cls_scope_ptr->AddTypeSymbol(new_self_sym);

    auto tm = scopes::ScopeManager(sm->GlobalScope, new_alias_stmt ? sm->CurrentScope->GetTypeSymbol(new_alias_stmt->OldType)->LinkedScope : new_cls_scope_ptr);
    for (auto const &scoped_sym : new_cls_scope_ptr->AllVarSymbols(true)) {
        scoped_sym->Type = scoped_sym->Type->SubstituteGenerics(substitution_generics);
        if (meta->CurrentStage > 5) {
            meta->Save();
            meta->AllowAbstractType = true;
            scoped_sym->Type->Stage7_AnalyseSemantics(&tm, meta);
            meta->Restore();
        }
    }

    for (auto *attr : new_ast_ptr->Impl->Members
         | genex::views::ptr
         | genex::views::cast_dynamic<asts::ClassAttributeAst*>()) {
        //
        attr->Type = attr->Type->SubstituteGenerics(substitution_generics);
        if (meta->CurrentStage > 5) {
            attr->Stage7_AnalyseSemantics(&tm, meta);
        }

        // Remove void attributes from the class.
        if (IsTypeVoid(*attr->Type, *new_cls_scope_ptr)) {
            // new_ast_ptr->impl |= genex::actions::remove_if([&](auto &&x) { return x == attr; }, [](auto &&x) { return x.get(); });
            new_cls_scope_ptr->RemVarSymbol(attr->Name);
            new_ast_ptr->Impl->Members |= genex::actions::remove_if([&](auto &&x) { return x.get() == attr; });
        }
    }

    // Return the new class scope.
    return new_cls_scope_ptr;
}

auto spp::analyse::utils::type_utils::CreateGenericFunScope(
    scopes::Scope const &old_fun_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    Vec<Shared<scopes::Symbol>> const &external_generic_syms,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> scopes::Scope* {
    // Create a new scope and symbol for the generic substituted function.
    auto new_fun_scope = MakeUnique<scopes::Scope>(old_fun_scope);
    const auto new_fun_scope_ptr = new_fun_scope.get();
    const auto old_fn_proto = asts::AstBody(old_fun_scope.AstNode)[0]->To<asts::FunctionPrototypeAst>();
    old_fn_proto->RegisterGenericSubstitution(std::move(new_fun_scope), nullptr);

    auto tm = scopes::ScopeManager(sm->GlobalScope, new_fun_scope_ptr);
    RegisterGenericSyms(external_generic_syms, generic_args.Args, new_fun_scope_ptr, &tm, meta);

    // Return the new function scope.
    return new_fun_scope_ptr;
}

auto spp::analyse::utils::type_utils::CreateGenericSupScope(
    scopes::Scope &old_sup_scope,
    scopes::Scope &new_cls_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    Vec<Shared<scopes::Symbol>> const &external_generic_syms,
    scopes::ScopeManager const *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<scopes::Scope*, scopes::Scope*> {
    // Create a new scope for the generic substituted super scope.
    const auto self_type = asts::AstName(old_sup_scope.AstNode)->SubstituteGenerics(generic_args.GetAllArgs());
    auto new_sup_scope = MakeUnique<scopes::Scope>(old_sup_scope);
    auto new_sup_scope_ptr = new_sup_scope.get();
    new_sup_scope_ptr->InternalTable = old_sup_scope.InternalTable;
    old_sup_scope.Parent->Children.EmplaceBack(std::move(new_sup_scope));

    std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->Name).Name =
        SubstituteSupScopeName(std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->Name).Name, generic_args);

    // Register the generic symbols.
    auto tm = scopes::ScopeManager(sm->GlobalScope, new_sup_scope_ptr);
    RegisterGenericSyms(external_generic_syms, generic_args.Args, new_sup_scope_ptr, &tm, meta);

    // Add the "Self" symbol into the new scope.
    self_type->Stage7_AnalyseSemantics(&tm, meta);
    const auto new_self_sym = MakeShared<scopes::TypeSymbol>(
        MakeUnique<asts::TypeIdentifierAst>(0uz, "Self", nullptr), sm->SelfProto(), &new_cls_scope,
        new_sup_scope_ptr);
    new_sup_scope_ptr->AddTypeSymbol(new_self_sym);

    // Run generic substitution on the aliases in the new scope.
    for (auto const &scoped_sym : new_sup_scope_ptr->AllTypeSymbols(true)) {
        if (scoped_sym->AliasStmt != nullptr) {
            auto old_type_sub = scoped_sym->AliasStmt->OldType->SubstituteGenerics(generic_args.GetAllArgs());
            // old_type_sub->Stage7_AnalyseSemantics(&tm, meta);  // Todo: Why is this commented?
            const auto old_type_sub_sym = new_sup_scope_ptr->GetTypeSymbol(old_type_sub);

            scoped_sym->AliasStmt->OldType = std::move(old_type_sub);
            scoped_sym->AliasStmt->MappedOldType = scoped_sym->AliasStmt->OldType;
            if (scoped_sym->AliasStmt->GetAstScope()) {
                // Self doesn't have a scope on it
                scoped_sym->AliasStmt->GetAstScope()->Parent = new_sup_scope_ptr;
            }

            if (old_type_sub_sym != nullptr) {
                old_type_sub_sym->AliasedBySyms.PushBack(scoped_sym);
                scoped_sym->Type = old_type_sub_sym->Type;
                scoped_sym->LinkedScope = old_type_sub_sym->LinkedScope;
            }
        }
    }

    // Run generic substitution on the constants in the new scope.
    for (auto const &scoped_sym : new_sup_scope_ptr->AllVarSymbols(true)) {
        auto old_type_sub = scoped_sym->Type->SubstituteGenerics(generic_args.GetAllArgs());
        scoped_sym->Type = std::move(old_type_sub);
    }

    // Create the scope for the new super class type. This will handle recursive sup-scope creation.
    auto super_cls_scope = static_cast<scopes::Scope*>(nullptr);
    if (const auto ext_ast = old_sup_scope.AstNode->To<asts::SupPrototypeExtensionAst>(); ext_ast != nullptr) {
        const auto new_fq_super_type = ext_ast->SuperClass->SubstituteGenerics(generic_args.GetAllArgs());
        meta->Save();
        meta->AllowAbstractType = true;
        new_fq_super_type->Stage7_AnalyseSemantics(&tm, meta);
        meta->Restore();
        super_cls_scope = new_cls_scope.GetTypeSymbol(new_fq_super_type)->LinkedScope;
    }

    return std::make_tuple(new_sup_scope_ptr, super_cls_scope);
}

auto spp::analyse::utils::type_utils::CreateGenericSym(
    asts::GenericArgumentAst const &generic,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta,
    scopes::ScopeManager *tm)
    -> Shared<scopes::Symbol> {
    //
    using errors::SppInternalCompilerError;

    // Handle the generic type argument => creates a type symbol.
    if (const auto type_arg = generic.To<asts::GenericArgumentTypeKeywordAst>(); type_arg != nullptr) {
        // "Self" should not be looked up and changed.
        if (type_arg->Val->IsSelfType()) {
            return MakeShared<scopes::TypeSymbol>(type_arg->Name->TypeParts().Back(), nullptr, nullptr, sm.CurrentScope, sm.CurrentScope->ParentModule(), true);
        }

        const auto true_val_sym = sm.CurrentScope->GetTypeSymbol(type_arg->Val);

        // If the target is generic, then create a generic scope for it.
        // auto generic_scope = generic_scope = ( {
        //     auto generic_scope_name = scopes::ScopeBlockName("<generic#" + type_arg->Name->TypeParts().Back()->name + ">");
        //     MakeUnique<scopes::Scope>(generic_scope_name, nullptr);
        // });

        // Build the type symbol for the generic type argument.
        auto sym = MakeShared<scopes::TypeSymbol>(
            type_arg->Name->TypeParts().Back(), true_val_sym ? true_val_sym->Type : nullptr,
            true_val_sym ? true_val_sym->LinkedScope : nullptr, sm.CurrentScope, sm.CurrentScope->ParentModule(), true,
            true_val_sym ? true_val_sym->IsDirectlyCopyable : false, asts::utils::Visibility::kPublic,
            asts::AstClone(type_arg->Val->GetConvention()));
        sym->GenericConstraints = true_val_sym->GenericConstraints;
        sym->IsDirectlyZeroType = true_val_sym->IsDirectlyZeroType;

        // Link the generic scope to the type symbol if it was created.
        // if (true_val_sym) {
        //     generic_scope->TySym = sym;
        //     scopes::ScopeManager::temp_scopes.EmplaceBack(std::move(generic_scope));
        // }
        return sym;
    }

    // Handle the generic comp argument => creates a variable symbol.
    if (const auto comp_arg = generic.To<asts::GenericArgumentCompKeywordAst>(); comp_arg != nullptr) {
        auto sym = MakeShared<scopes::VariableSymbol>(
            asts::IdentifierAst::FromType(*comp_arg->Name),
            comp_arg->Val->InferType(tm ? tm : &sm, meta),
            sm.CurrentScope, // or tm?
            false, true, asts::utils::Visibility::kPublic);
        sym->MemInfo->AstCompTime = asts::AstClone(comp_arg);
        return sym;
    }

    Raise<SppInternalCompilerError>(
        {sm.CurrentScope},
        ERR_ARGS(generic, "Unknown generic argument ast type"));
}

auto spp::analyse::utils::type_utils::RegisterGenericSyms(
    Vec<Shared<scopes::Symbol>> const &external_generic_syms,
    Vec<Unique<asts::GenericArgumentAst>> const &generic_args,
    scopes::Scope *scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // Register the type symbols to the scope.
    for (auto const &e : external_generic_syms | spp::views::cast_shared<scopes::TypeSymbol>()) {
        scope->AddTypeSymbol(e);
    }

    // Register the variable symbols to the scope.
    for (auto const &e : external_generic_syms | spp::views::cast_shared<scopes::VariableSymbol>()) {
        scope->AddVarSymbol(e);
    }

    // Convert the generic arguments into symbols.
    auto generic_syms = generic_args
        | genex::views::transform([&](auto const &g) { return CreateGenericSym(*g, *sm, meta); })
        | genex::to<Vec>();

    // Register the created generic symbols to the scope.
    for (auto const &e : generic_syms | spp::views::cast_shared<scopes::TypeSymbol>()) {
        const auto old = scope->RemTypeSymbol(e->Name);
        if (old) { e->GenericConstraints = asts::AstCloneVecShared(old->GenericConstraints); }
        scope->AddTypeSymbol(e);
    }

    // Register the created generic symbols to the scope.
    for (auto const &e : generic_syms | spp::views::cast_shared<scopes::VariableSymbol>()) {
        scope->RemVarSymbol(e->Name);
        scope->AddVarSymbol(e);
    }
}

auto spp::analyse::utils::type_utils::GetTypeSymOrError(
    scopes::Scope const &scope,
    asts::TypeIdentifierAst const &type_part,
    scopes::ScopeManager const &sm,
    asts::meta::CompilerMetaData *)
    -> scopes::TypeSymbol* {
    //
    using expr_utils::RaiseMissingTypeIdentifierAndClosestOptions;

    // Get the type part's symbol, and raise an error if it doesn't exist.
    const auto type_sym = scope.GetTypeSymbol(type_part.shared_from_this(), false);
    if (type_sym == nullptr) {
        RaiseMissingTypeIdentifierAndClosestOptions(type_part, scope.AllTypeSymbols(), sm);
    }

    // Return the found type symbol.
    return type_sym.get();
}

auto spp::analyse::utils::type_utils::GetNsScopeOrError(
    scopes::Scope const &scope,
    asts::IdentifierAst const &ns,
    scopes::ScopeManager const &sm)
    -> scopes::Scope* {
    //
    using expr_utils::RaiseMissingIdentifierAndClosestOptions;

    // If the namespace does not exist, raise an error.
    const auto ns_sym = scope.GetNsSymbol(ns.shared_from_this());
    if (ns_sym == nullptr) {
        RaiseMissingIdentifierAndClosestOptions(ns, {}, scope.AllNsSymbols(), sm);
    }

    // Return the found namespace scope.
    return ns_sym->LinkedScope;
}

auto spp::analyse::utils::type_utils::EnforceGenericConstraintsOneArg(
    Vec<Shared<asts::TypeAst>> const &constraints,
    asts::TypeAst const &concrete_type,
    scopes::Scope const &constraints_owner_scope,
    scopes::Scope const &concrete_scope)
    -> asts::TypeAst const* {
    // Note: concrete scope is where the type is being used; concrete_sym->LinkedScope is the scope of the type
    // definition.

    // Determine the concrete symbol, and if non-generic, add its scope.
    const auto concrete_sym = concrete_scope.GetTypeSymbol(concrete_type.shared_from_this());
    auto sup_info = Vec<Pair<Shared<asts::TypeAst>, scopes::Scope const*>>{};
    if (concrete_type.IsSelfType() and not concrete_sym->IsGeneric) {
        // Todo: might need to keep the self sym, mapped to fq
        sup_info.EmplaceBack(concrete_sym->FqName(), concrete_sym->LinkedScope);
    }

    // Get all the sup scopes of the concrete type (none for generic).
    // Using "SupScopes" not "SupTypes" because we need both the scopes and types.
    const auto sup_scopes = concrete_sym->LinkedScope
        ? concrete_sym->LinkedScope->SupScopes()
        : concrete_sym->GenericConstraints | genex::views::transform([&](auto const &constraint) { return constraints_owner_scope.GetTypeSymbol(constraint)->LinkedScope; }) | genex::to<Vec>();
    sup_info.EmplaceBack(concrete_sym->FqName(), &concrete_scope);
    for (auto const *sup_scope : sup_scopes) {
        if (sup_scope->AstNode->To<asts::ClassPrototypeAst>() == nullptr) { continue; }
        const auto &sup_sym = sup_scope->TySym;
        sup_info.EmplaceBack(sup_sym->FqName(), sup_scope);
    }

    // Compare each constraint against the concrete type and its supertypes.
    for (auto const &constraint : constraints) {
        auto matched = false;
        for (auto const &[sup_type, sup_scope] : sup_info) {
            matched = TypeEq(*constraint, *sup_type, constraints_owner_scope, *sup_scope);
            if (matched) { break; }
        }

        // If any constraint is not met, return it so the caller can decide whether to raise an error.
        if (not matched) { return constraint.get(); }
    }

    // All constraints are satisfied.
    return nullptr;
}

auto spp::analyse::utils::type_utils::DedupVariableInnerTypes(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Vec<Shared<asts::TypeAst>> {
    // Create the list of types.
    auto out = Vec<Shared<asts::TypeAst>>();
    if (type.TypeParts().Back()->GnArgGroup->Args.IsEmpty()) {
        return out;
    }

    for (auto &&generic_arg : type.TypeParts().Back()->GnArgGroup->GetTypeArgs()[0]->Val->TypeParts().Back()->GnArgGroup->GetTypeArgs()) {
        // Inspect inner variant types by extending the composite type list.
        if (IsTypeVariant(*generic_arg->Val->WithoutGenerics(), scope)) {
            out.AppendRange(DedupVariableInnerTypes(*generic_arg->Val, scope));
        }

        // Inspect a non-variant type, and if it hasn't beem added to the list, add it.
        else if (not genex::any_of(out, [&](auto x) { return TypeEq(*generic_arg->Val, *x, scope, scope); })) {
            out.EmplaceBack(generic_arg->Val);
        }
    }

    // Return the deduplicated list of types.
    return out;
}

auto spp::analyse::utils::type_utils::SubstituteSupScopeName(
    Str const &old_sup_scope_name,
    asts::GenericArgumentGroupAst const &generic_args)
    -> Str {
    const auto parts = old_sup_scope_name
        | genex::views::split('#')
        | genex::to<Vec>()
        | genex::views::transform([](auto &&x) { return Str(x.begin(), x.end()); })
        | genex::to<Vec>();

    if (not parts[1].contains(" ext ")) {
        const auto t = INJECT_CODE(parts[1], parse_type_expression)->SubstituteGenerics(generic_args.GetAllArgs());
        const auto o = parts[0] + "#" + t->ToString() + "#" + parts[2];
        return o;
    }
    const auto t = INJECT_CODE(parts[1].substr(0, parts[1].find(" ext ")), parse_type_expression)->SubstituteGenerics(generic_args.GetAllArgs());
    const auto u = INJECT_CODE(parts[1].substr(parts[1].find(" ext ") + 5), parse_type_expression)->SubstituteGenerics(generic_args.GetAllArgs());
    const auto o = parts[0] + "#" + t->ToString() + " ext " + u->ToString() + "#" + parts[2];

    return o;
}

auto spp::analyse::utils::type_utils::RecursiveAliasSearch(
    asts::TypeStatementAst const &alias_stmt,
    const bool from_use_stmt,
    scopes::Scope *tracking_scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<Shared<asts::TypeAst>, Shared<asts::GenericParameterGroupAst>, scopes::Scope*> {
    // How to extract generic parameters from a type symbol: alias, then type, otherwise none (generic).
    const auto NO_PARAMS = asts::GenericParameterGroupAst::NewEmpty();
    const auto extract_params = [&NO_PARAMS](scopes::TypeSymbol const &ts) {
        return ts.AliasStmt
            ? ts.AliasStmt->GnParamGroup.get()
            : ts.Type
            ? ts.Type->GnParamGroup.get()
            : NO_PARAMS.get();
    };

    const auto filter_params = [](asts::GenericParameterGroupAst const &pg, asts::GenericArgumentGroupAst const &ag) {
        auto out = asts::GenericParameterGroupAst::NewEmptyShared();
        for (auto const &param : pg.GetTypeParams()) {
            if (not genex::any_of(ag.GetTypeKeywordArgs(), [&](auto const *arg) { return *arg->Name == *param->Name; })) {
                out->Params.EmplaceBack(asts::AstClone(param));
            }
        }
        for (auto const &param : pg.GetCompParams()) {
            if (not genex::any_of(ag.GetCompKeywordArgs(), [&](auto const *arg) { return *arg->Name == *param->Name; })) {
                out->Params.EmplaceBack(asts::AstClone(param));
            }
        }
        return out;
    };

    // Get the next type in the search, and its symbol.
    auto old_type = alias_stmt.OldType;
    auto old_sym = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics());
    auto use_stmt_propagating_generics = static_cast<asts::GenericArgumentGroupAst*>(nullptr);

    // If this is a use statement to a class, then grab its generics and return immediately.
    // For example, use Vec::Vec => type Vec[T, A: ... = ...] = Vec::Vec[T=T, A=A]
    if (from_use_stmt and old_sym->AliasStmt == nullptr) {
        auto generic_params = old_sym->Type->GnParamGroup;
        old_type = old_type->WithGenerics(asts::GenericArgumentGroupAst::FromParams(*generic_params));
        return {old_type, generic_params, old_sym->LinkedScope};
    }

    auto generic_args = asts::AstClone(old_type->TypeParts().Back()->GnArgGroup.get());
    auto final_generic_params = asts::GenericParameterGroupAst::NewEmptyShared();
    tracking_scope = old_sym->ScopeDefinedIn;

    while (true) {
        // If there are generics to propagate from a use statement, apply them now, then reset the propagation.
        if (use_stmt_propagating_generics) {
            old_type = old_type->WithGenerics(asts::AstClone(use_stmt_propagating_generics));
            use_stmt_propagating_generics = nullptr;
        }

        // If this alias is from a use statement, we need to propagate its generics for the next alias search.
        if (old_sym->AliasStmt and old_sym->AliasStmt->IsFromUseStatement()) {
            use_stmt_propagating_generics = old_type->TypeParts().Back()->GnArgGroup.get();
            if (use_stmt_propagating_generics->Args.IsEmpty()) { use_stmt_propagating_generics = nullptr; }
            tracking_scope = old_sym->ScopeDefinedIn;
        }

        // Name the generics for this alias, and shift into the next scope.
        else {
            func_utils::NameGnArgs(*old_type->TypeParts().Back()->GnArgGroup, *extract_params(*old_sym), *old_type, *sm, *meta, false);
            if (old_sym->AliasStmt) {
                final_generic_params = filter_params(*old_sym->AliasStmt->GnParamGroup, *old_type->TypeParts().Back()->GnArgGroup);
            }
            old_type = old_type->SubstituteGenerics(generic_args->GetAllArgs());
            *generic_args += *old_type->TypeParts().Back()->GnArgGroup;
            tracking_scope = old_sym->ScopeDefinedIn;
        }

        if (old_sym->AliasStmt == nullptr) { break; }

        old_type = old_sym->AliasStmt->OldType;
        old_sym = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics());
        if (old_sym->AliasStmt == nullptr and (use_stmt_propagating_generics == nullptr or use_stmt_propagating_generics->Args.IsEmpty())) { break; }
    }

    old_type = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics())->FqName()->WithGenerics(AstClone(old_type->TypeParts().Back()->GnArgGroup));

    auto &temp = *old_type->TypeParts().Back()->GnArgGroup;
    func_utils::NameGnArgs(
        temp, *extract_params(*old_sym), *old_type, *sm, *meta, false);
    old_type = old_type->SubstituteGenerics(generic_args->GetAllArgs());
    return std::make_tuple(old_type, final_generic_params, tracking_scope);
}

auto spp::analyse::utils::type_utils::GetFieldIndexInType(
    asts::TypeAst const &type_sym,
    asts::IdentifierAst const &field_name,
    scopes::ScopeManager *sm)
    -> std::size_t {
    // Get all the attributes on the type.
    const auto all_attrs = GetAllAttrs(type_sym, sm);

    // Find the field index.
    for (auto index = 0uz; index < all_attrs.Len(); ++index) {
        if (*std::get<0>(all_attrs[index]) == field_name) {
            return index;
        }
    }

    return all_attrs.Len();

    // return genex::position(all_attrs, genex::operations::eq_fixed(field_name), [](auto &&attr) -> decltype(auto) { return *attr.first->Name; });
}

auto spp::analyse::utils::type_utils::ResolveAndSubstituteSelfType(
    asts::TypeAst const &type,
    scopes::Scope const &scope,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> Shared<asts::TypeAst> {
    // Todo: always clone here? performance hit i think.
    using asts::generate::common_types::SelfType;
    const auto true_self_type = scope.GetEnclosingSelfType(meta);
    if (true_self_type == nullptr) { return AstClone(&type); }

    //
    auto g = MakeUnique<asts::GenericArgumentTypeKeywordAst>(
        SelfType(0), nullptr, true_self_type);
    const auto gg = asts::GenericArgumentGroupAst::NewEmpty();
    gg->Args.PushBack(std::move(g));

    //
    auto t = type.SubstituteGenerics(gg->GetAllArgs());
    meta.Save();
    meta.AllowAbstractType = true;
    t->Stage7_AnalyseSemantics(&sm, &meta);
    meta.Restore();
    return t;
}
