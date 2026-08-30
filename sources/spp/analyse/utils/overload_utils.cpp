module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.overload_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.monomorphization_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.subroutine_prototype_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import std;
import sys;

namespace spp::analyse::utils::overload_utils {
  namespace {
    struct FailedOverload {
      asts::FunctionPrototypeAst *Proto;
      Str Error;
      Str Reason;
    };

    struct OverloadCandidates {
      bool IsClosure;
      Unique<asts::FunctionPrototypeAst> ClosureProto;
      Vec<func_utils::FunctionOverload> Overloads;
    };

    struct PropagatedMethodCall {
      PassedOverload Overload;
      bool IsClosure;
      Unique<asts::PostfixExpressionAst> TransformedAst;
    };

    /**
     * Re-record, against the instantiation's own symbols, the callability its parameters inherited from their generic
     * constraints. A parameter declared "F: FunMov" keeps being callable through what the constraint promised even
     * though its type has just been rewritten to the argument: "FunMut" satisfies "FunMov" while also being callable
     * through a borrow, so reading the substituted type would make this body consume the value here and borrow it in
     * the next instantiation, which linear ownership cannot account for. It goes on the symbol because the constraint
     * lives on the template's generic parameter, which nothing in the instantiation refers to any more.
     * @param new_fn_proto The instantiated prototype whose parameter symbols are being retyped.
     * @param fn_proto The template the instantiation was cloned from, which still carries the constraints.
     * @param new_fn_scope The instantiation's scope.
     * @param combined_generics The arguments this instantiation pins.
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param meta Associated metadata.
     */
    auto ReattachCallableConstraints(
      asts::FunctionPrototypeAst const &new_fn_proto,
      asts::FunctionPrototypeAst const &fn_proto,
      scopes::Scope *new_fn_scope,
      asts::GenericArgumentGroupAst const &combined_generics,
      scopes::ScopeManager &tm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      for (auto *p : new_fn_proto.FnParamGroup->GetNonSelfParams()) {
        const auto declared = p->Source.OriginalType;
        if (declared == nullptr) { continue; }

        const auto gn_param = genex::find_if(
          fn_proto.GnParamGroup->Params, [&](auto const &g) { return *g->Name == *declared; });
        if (gn_param == fn_proto.GnParamGroup->Params.end()) { continue; }

        const auto constraints = (*gn_param)->To<asts::GenericParameterTypeAst>();
        if (constraints == nullptr or constraints->Constraints == nullptr) { continue; }

        for (auto const &c : constraints->Constraints->Constraints) {
          if (not type_predicates::IsTypeFunc(*c, *new_fn_scope)) { continue; }
          const auto sym = new_fn_scope->Children[0]->GetVarSymbol(p->ExtractName().get(), true);
          if (sym == nullptr) { break; }

          // Substituted the same way the parameter's own type is: the constraint is written in the template's terms
          // ("FunMov[(T,), U]"), and what the call needs is this instantiation's argument and return types.
          auto callable = c->SubstituteGenerics(combined_generics.GetAllArgs());
          callable->Stage7_AnalyseSemantics(&tm, meta);
          sym->CallableAsType = std::move(callable);
          break;
        }
      }
    }

    /**
     * Retype the two symbols an instantiation inherits from its template rather than getting for itself: "self" and
     * the variadic pack. Both are typed by stage 6, which only ever runs on the template, so the clone would otherwise
     * carry the template's symbol - "self" still typed as the template's "Self", and a pack still typed as the single
     * element it declares ("..b: T") rather than the tuple the call collapsed its trailing arguments into.
     * @param new_fn_proto The instantiated prototype, whose "self" parameter and pack type are rewritten in place.
     * @param new_fn_scope The instantiation's scope, holding the symbols to retype.
     * @param combined_generics The arguments this instantiation pins.
     * @param variadic_pack_type The tuple the call collapsed its trailing arguments into, or @c nullptr .
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param meta Associated metadata.
     */
    auto RetypeInheritedSymbols(
      asts::FunctionPrototypeAst &new_fn_proto,
      scopes::Scope *new_fn_scope,
      asts::GenericArgumentGroupAst const &combined_generics,
      Shared<asts::TypeAst> const &variadic_pack_type,
      scopes::ScopeManager &tm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      // "self" is typed as "Self", so only a substitution that pins "Self" to the receiver has anything to rewrite.
      if (const auto self_param = new_fn_proto.FnParamGroup->GetSelfParam(); self_param != nullptr) {
        auto substituted_self = self_param->Type->SubstituteGenerics(combined_generics.GetAllArgs());
        if (not substituted_self->IsSelfType()) {
          substituted_self->Stage7_AnalyseSemantics(&tm, meta);
          self_param->Type = substituted_self;
          const auto self_name = self_param->ExtractName();
          if (const auto self_sym = new_fn_scope->Children[0]->GetVarSymbol(self_name.get(), true);
            self_sym != nullptr) {
            self_sym->Type = substituted_self->WithConvention(asts::AstClone(self_param->Conv));
          }
        }
      }
      new_fn_proto.VariadicPackType = asts::AstClone(variadic_pack_type);

      // A variadic parameter declares one element but binds the whole tuple.
      if (variadic_pack_type != nullptr) {
        auto pack_type = asts::AstClone(variadic_pack_type);
        pack_type->Stage7_AnalyseSemantics(&tm, meta);
        const auto pack_name = new_fn_proto.FnParamGroup->GetVariadicParams()->ExtractName();
        if (const auto pack_sym = new_fn_scope->Children[0]->GetVarSymbol(pack_name.get(), true);
          pack_sym != nullptr) {
          pack_sym->Type = std::move(pack_type);
        }
      }
    }

    /**
     * Whether an instantiation is concrete: every argument it pins names a real type or value, and every type in its
     * own signature resolves to one. A non-concrete instantiation is still a template as far as anything downstream is
     * concerned, so codegen has nothing to emit for it.
     * @param combined_generics The arguments this instantiation pins.
     * @param new_fn_proto The instantiated prototype whose signature is checked.
     * @param new_fn_scope The instantiation's scope, which the signature's types are resolved in.
     * @param tm A scope manager positioned at @p new_fn_scope .
     * @param sm The scope manager at the call site, which the arguments are resolved in.
     * @param meta Associated metadata.
     * @return If the instantiation is fully concrete.
     */
    auto ComputeIsConcrete(
      asts::GenericArgumentGroupAst const &combined_generics,
      asts::FunctionPrototypeAst const &new_fn_proto,
      scopes::Scope const *new_fn_scope,
      scopes::ScopeManager &tm,
      scopes::ScopeManager const *sm,
      asts::meta::CompilerMetaData *meta)
      -> bool {
      const auto type_is_concrete = [&](asts::TypeAst const &type) {
        const auto resolved = type_utils::ResolveAndSubstituteSelfType(type, *new_fn_scope, tm, *meta);
        return type_predicates::IsTypeFullyConcrete(*resolved, *new_fn_scope);
      };

      return genex::all_of(combined_generics.Args | genex::views::ptr, [&](auto const *arg) {
          if (const auto type_arg = arg->template To<asts::GenericArgumentTypeAst>(); type_arg != nullptr) {
            return type_predicates::IsTypeFullyConcrete(*type_arg->Val, *sm->CurrentScope);
          }
          if (const auto comp_arg = arg->template To<asts::GenericArgumentCompAst>(); comp_arg != nullptr) {
            return comp_arg->Val->template To<asts::IdentifierAst>() == nullptr;
          }
          return true;
        })
        and type_is_concrete(*new_fn_proto.ReturnType)
        and genex::all_of(new_fn_proto.FnParamGroup->GetAllParams(), [&](auto *p) {
          return type_is_concrete(*p->Type);
        });
    }

    auto GetFuncOwnerTypeAndFuncName(
      asts::ExpressionAst const &lhs,
      scopes::ScopeManager &sm,
      asts::meta::CompilerMetaData *meta)
      -> Tup<Shared<asts::TypeAst>, scopes::Scope const*, Shared<asts::IdentifierAst>> {
      //
      using expr_utils::RaiseMissingIdentifierAndClosestOptions;

      // Define some expression casts that are used commonly.
      const auto postfix_lhs = lhs.To<asts::PostfixExpressionAst>();
      const auto runtime_field = postfix_lhs
        ? postfix_lhs->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>()
        : nullptr;
      const auto static_field = postfix_lhs
        ? postfix_lhs->Op->To<asts::PostfixExpressionOperatorStaticMemberAccessAst>()
        : nullptr;

      // Specific casts.
      const auto postfix_lhs_as_type = postfix_lhs ? postfix_lhs->Lhs->To<asts::TypeAst>() : nullptr;
      const auto lhs_as_ident = lhs.To<asts::IdentifierAst>();

      // If the lhs is an identifier, it must be a variable
      // symbol, not a namespace symbol.
      if (lhs_as_ident and sm.CurrentScope->GetVarSymbol(lhs_as_ident) == nullptr) {
        RaiseMissingIdentifierAndClosestOptions(*lhs_as_ident, sm.CurrentScope->AllVarSymbols(), {}, sm);
      }

      // Variables that will be set in each branch, and
      // returned. These are used to determine what variation
      // of function call is being performed.
      auto fn_owner_type = Shared<asts::TypeAst>(nullptr);
      auto fn_owner_scope = static_cast<scopes::Scope const*>(nullptr);
      auto fn_name = Shared<asts::IdentifierAst>(nullptr);

      // Runtime access into an object: "object.method()".
      // No namespacing involved.
      if (postfix_lhs != nullptr and runtime_field != nullptr) {
        fn_owner_type = postfix_lhs->Lhs->InferType(&sm, meta);
        fn_name = runtime_field->Name;
        fn_owner_scope = sm.CurrentScope->GetTypeSymbol(fn_owner_type.get())->LinkedScope;
      }

      // Static access into a type: "Type::method()" or
      // "ns::Type::method()".
      else if (static_field != nullptr and postfix_lhs_as_type != nullptr) {
        fn_owner_type = asts::AstCloneShared(postfix_lhs_as_type);
        fn_name = static_field->Name;
        fn_owner_scope = sm.CurrentScope->GetTypeSymbol(fn_owner_type.get())->LinkedScope;
      }

      // Direct access into a namespaced free function:
      // "std::io::print(variable)".
      else if (postfix_lhs != nullptr and static_field != nullptr) {
        fn_owner_scope = sm.CurrentScope->ConvertPostfixToNestedScope(postfix_lhs->Lhs.get());
        fn_name = static_field->Name;

        // Add a name check here because we need to get
        // the type off of it before it is even analysed.
        const auto fn_owner_sym = fn_owner_scope->GetVarSymbol(fn_name.get());
        if (fn_owner_sym == nullptr) {
          RaiseMissingIdentifierAndClosestOptions(*fn_name, fn_owner_scope->AllVarSymbols(), {}, sm);
        }
        fn_owner_type = fn_owner_sym->Type;
      }

      // Direct access into a non-namespaced function:
      // "function()":
      else if (lhs_as_ident != nullptr) {
        fn_owner_type = nullptr;
        fn_name = asts::AstCloneShared(lhs_as_ident);
        fn_owner_scope = sm.CurrentScope->ParentModule();
      }

      // Non-callable AST.
      else {
        fn_owner_type = nullptr;
        fn_name = nullptr;
        fn_owner_scope = nullptr;
      }

      return {fn_owner_type, fn_owner_scope, fn_name};
    }

    auto ConvertMethodToFuncForm(
      asts::TypeAst const &function_owner_type,
      asts::IdentifierAst const &function_name,
      asts::PostfixExpressionAst const &lhs,
      asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
      scopes::ScopeManager &sm,
      asts::meta::CompilerMetaData *meta)
      -> Pair<Unique<asts::PostfixExpressionAst>, Unique<asts::PostfixExpressionOperatorFunctionCallAst>> {
      // A method reached through a forwarding type is invoked
      // on the forwarded-to value, not on the object that forwards
      // to it: "w.greet()" calls "greet" on "w.fwd_ref()". The
      // member access has already built that call, so use it as
      // the receiver; a method found on the object's own type uses
      // the object itself.
      // Todo: Check this for when we use a method on a type who has a forwarding type, but the forward isn't used.
      const auto member_access = lhs.Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>();
      const auto fwd_receiver = member_access != nullptr ? member_access->GetFwdReceiver() : nullptr;
      const auto self_expr = fwd_receiver != nullptr ? fwd_receiver : lhs.Lhs.get();
      auto self_arg_val = asts::AstClone(self_expr);

      // Create the static method access (without the function
      // call and args).
      auto field = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(
        nullptr, AstClone(&function_name));
      auto field_access = MakeUnique<asts::PostfixExpressionAst>(
        AstClone(&function_owner_type), std::move(field));

      // Create an argument for "self" and inject it into the
      // current arguments.
      auto self_arg = MakeUnique<asts::FunctionCallArgumentPositionalAst>(
        nullptr, nullptr, std::move(self_arg_val));
      auto fn_args = std::move(fn_call.FnArgGroup->Args);
      fn_args.Insert(fn_args.begin(), std::move(self_arg));

      // Create the function call with the new arguments.
      auto new_fn_call = MakeUnique<asts::PostfixExpressionOperatorFunctionCallAst>(
        AstClone(fn_call.GnArgGroup), AstClone(fn_call.FnArgGroup), nullptr);
      new_fn_call->FnArgGroup->Args = std::move(fn_args);

      // The forwarding receiver is a "GenOnce" call that resumes
      // itself, so its type is the borrow it yields. Infer it
      // with resumption allowed, whatever the surrounding
      // expression asked for (an "async" call suppresses it).
      {
        const auto _meta_guard = asts::meta::MetaGuard(meta);
        meta->PreventAutoGeneratorResume = false;
        new_fn_call->FnArgGroup->Args[0]->SetSelfType(self_expr->InferType(&sm, meta));
      }
      new_fn_call->Source.OriginalExpr = fn_call.Source.OriginalExpr;

      // Return the new ASTs.
      return {std::move(field_access), std::move(new_fn_call)};
    }

    auto CreateCallablePrototype(
      asts::TypeAst const &expr_type)
      -> Unique<asts::FunctionPrototypeAst> {
      // Extract the parameter and return types from the
      // expression type.
      auto ret_ty = expr_type.LastTypePart()->GnArgGroup->TypeAt("Out")->Val;
      auto param_tys = expr_type.LastTypePart()->GnArgGroup->TypeAt("Args")->Val->LastTypePart()->GnArgGroup->GetTypeArgs()
        | genex::views::transform([](auto *g) {
          return MakeUnique<asts::FunctionParameterRequiredAst>(nullptr, nullptr, g->Val);
        })
        | spp::views::cast_unique<asts::FunctionParameterAst>();

      // Create a function prototype based off of the parameter
      // and return type.
      // Todo: When might it be a coroutine, not a subroutine?
      // Todo: Do we set "cmp" here for the subroutine ever?
      auto dummy_param_group = MakeUnique<asts::FunctionParameterGroupAst>(
        nullptr, std::move(param_tys), nullptr);
      auto dummy_name = MakeUnique<asts::IdentifierAst>(
        0uz, "<anonymous>");
      auto dummy_overload = MakeUnique<asts::SubroutinePrototypeAst>(
        SPP_NO_ANNOTATIONS, nullptr, nullptr, std::move(dummy_name),
        nullptr, std::move(dummy_param_group),
        nullptr, std::move(ret_ty), nullptr);

      // Return the function prototype.
      return dummy_overload;
    }

    auto NamedArgsOnly(
      Vec<Unique<asts::GenericArgumentAst>> &&args)
      -> Vec<Unique<asts::GenericArgumentAst>> {
      //
      using namespace spp::asts;
      auto out = Vec<Unique<GenericArgumentAst>>();
      for (auto &&arg : args) {
        const auto named = arg->To<GenericArgumentTypeKeywordAst>() != nullptr
          or arg->To<GenericArgumentCompKeywordAst>() != nullptr;
        if (named) { out.EmplaceBack(std::move(arg)); }
      }
      return out;
    }

    /**
     * Determine whether a (stripped) parameter type refers to a generic that is "rigid" at the call site: ie a
     * generic parameter belonging to a scope that encloses the caller, and so is already fixed rather than being
     * inferred/substituted for this particular call.
     *
     * When a parameter's type is such a rigid generic (eg calling @code slice_ref(from: I, into: I)@endcode from within
     * a method of @code sup [V, I] SliceRef[V, I]@endcode), the argument must match that generic exactly. This is
     * different from the "matches anything" behaviour of @code RelaxedTypeEq@endcode, which is only appropriate when a
     * generic is genuinely free to be inferred for the call (eg a non-substitutable superclass generic in a sup-ext
     * block, which is not visible as a generic from the caller's scope).
     */
    auto IsRigidGenericAtCaller(
      asts::TypeAst const &param_type,
      scopes::Scope const &caller_scope)
      -> bool {
      const auto stripped = param_type.WithoutGenerics()->WithoutConvention();
      const auto sym = caller_scope.GetTypeSymbol(stripped.get());
      return sym != nullptr and sym->IsGeneric;
    }

    /**
     * Whether a name that @c RelaxedTypeEq had to bind in order to match is one the caller cannot choose. The same
     * rigidity test as @c IsRigidGenericAtCaller, applied to the bindings the relaxed match produced rather than to the
     * parameter's head type, so that a generic appearing *inside* a parameter type is covered too: the "w" of
     * @code that: &SizedInteger[w=w, signed=false]@endcode is fixed by whoever instantiated the enclosing block, even
     * though @c SizedInteger itself is not generic and the head-type test therefore says nothing about it.
     *
     * A generic that is genuinely free for the call - the callee's own parameter, or a superclass generic in a sup-ext
     * block that is not visible from the caller - is not found as a generic here, so the relaxed match keeps working
     * for the cases it exists to serve.
     */
    auto IsRigidBindingAtCaller(
      asts::TypeIdentifierAst const &bound_name,
      scopes::Scope const &caller_scope)
      -> bool {
      if (const auto type_sym = caller_scope.GetTypeSymbol(&bound_name); type_sym != nullptr) {
        return type_sym->IsGeneric;
      }
      const auto as_id = asts::IdentifierAst::FromType(bound_name);
      const auto comp_sym = caller_scope.GetVarSymbol(as_id.get());
      return comp_sym != nullptr and comp_sym->IsGeneric;
    }

    /**
     * Whether a prototype's signature is written in terms of @c Self , and so reads differently per implementer.
     */
    auto SignatureNamesSelf(
      asts::FunctionPrototypeAst const &fn_proto)
      -> bool {
      const auto names_self = [](asts::TypeAst const &type) {
        return genex::any_of(type.Iterator(), [](auto const &part) { return part->Name == "Self"; });
      };
      return names_self(*fn_proto.ReturnType)
        or genex::any_of(fn_proto.FnParamGroup->GetNonSelfParams(), [&](auto const *p) { return names_self(*p->Type); });
    }

    /**
     * The type named on the left of the call site, if the call was made on one at all: "Vec[Str]::new()" has
     * "Vec[Str]" here, while "obj.method()" and a module-level "f()" have nothing. Three copies of this derivation had
     * drifted apart across this module and "func_utils", one of them dereferencing both casts unguarded, so it is
     * written once.
     * @param meta Associated metadata, whose @c PostfixExpressionLhs is the call site's left-hand side.
     * @return The receiver type, or @c nullptr when the call was not made on one.
     */
    auto ReceiverTypeAtCallSite(
      asts::meta::CompilerMetaData const *meta)
      -> asts::TypeAst* {
      if (meta->PostfixExpressionLhs == nullptr) { return nullptr; }
      const auto postfix = meta->PostfixExpressionLhs->To<asts::PostfixExpressionAst>();
      return postfix != nullptr ? postfix->Lhs->To<asts::TypeAst>() : nullptr;
    }

    auto RetrieveOwnerGenericArgs(
      Shared<asts::TypeAst> const &fwd_type,
      asts::meta::CompilerMetaData const *meta)
      -> Vec<Unique<asts::GenericArgumentAst>> {
      // A forwarding type stands in for the owner, so its
      // generics are the ones that count.
      if (fwd_type != nullptr) {
        return NamedArgsOnly(std::move(fwd_type->LastTypePart()->GnArgGroup->Args));
      }

      // Otherwise take them from the type the call was made
      // on, if it was made on one at all - a module-level
      // function has no owner to inherit from.
      const auto receiver = ReceiverTypeAtCallSite(meta);
      const auto is_type = receiver != nullptr ? asts::AstCloneShared(receiver) : nullptr;
      if (is_type != nullptr) {
        return NamedArgsOnly(std::move(is_type->LastTypePart()->GnArgGroup->Args));
      }

      return {};
    }

    auto RetrieveAllOverloads(
      asts::IdentifierAst const *fn_name,
      scopes::Scope const *fn_owner_scope,
      scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta)
      -> OverloadCandidates {
      //
      using func_utils::IsTargetCallable;

      // For named functions (ie non-closures), get all the
      // function overload implementation scopes.
      auto all_overloads = fn_name != nullptr and fn_owner_scope != nullptr
        ? func_utils::GetAllFunctionScopes(*fn_name, fn_owner_scope, *sm, meta)
        : Vec<func_utils::FunctionOverload>{};
      if (not all_overloads.IsEmpty()) {
        return OverloadCandidates{false, nullptr, std::move(all_overloads)};
      }

      // If there are no scopes, assume that this is a closure
      // (do functional type check).
      const auto closure_fn_type = IsTargetCallable(*meta->PostfixExpressionLhs, *sm, meta);
      if (closure_fn_type != nullptr) {
        auto closure_fn_proto = CreateCallablePrototype(*closure_fn_type);
        all_overloads.EmplaceBack(func_utils::FunctionOverload{
          .FnScope = sm->CurrentScope,
          .Proto = closure_fn_proto.get(),
          .SupGenerics = asts::GenericArgumentGroupAst::NewEmpty(),
          .FwdType = nullptr
        });
        return OverloadCandidates{true, std::move(closure_fn_proto), std::move(all_overloads)};
      }

      // Otherwise, there are no scopes (handled in caller).
      return OverloadCandidates{false, nullptr, {}};
    }

    auto PropagateMethodToFunction(
      asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
      asts::TypeAst const &fn_owner_type,
      asts::IdentifierAst const &fn_name,
      asts::PostfixExpressionAst const &cast_lhs,
      scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta)
      -> PropagatedMethodCall {
      //

      // Get the function conversion of the method (free
      // function with self argument).
      auto [transformed_lhs, transformed_fn_call] = ConvertMethodToFuncForm(
        fn_owner_type, fn_name, cast_lhs, fn_call, *sm, meta);

      // Determine the overload based off the function
      // (uniform system).
      auto [overload, is_closure] = [&] {
        const auto _meta_guard = asts::meta::MetaGuard(meta);
        meta->PostfixExpressionLhs = transformed_lhs.get();
        return DetermineOverload(*transformed_fn_call, sm, meta);
      }();

      // Get the argument group with the "self" injection,
      // and bind it to the function call.
      fn_call.FnArgGroup = asts::AstClone(transformed_fn_call->FnArgGroup);

      // Create a mock postfix based on the transformation.
      transformed_lhs->Stage7_AnalyseSemantics(sm, meta);
      auto pf = MakeUnique<asts::PostfixExpressionAst>(
        std::move(transformed_lhs), std::move(transformed_fn_call));
      return PropagatedMethodCall{std::move(overload), is_closure, std::move(pf)};
    }

    /**
     * Pin "Self" to the receiver for this candidate, if the receiver is what "Self" stands for here. A method declared
     * on a class and called on an implementer of it needs "Self" bound to the implementer, not left as the declaring
     * class - that is what lets "Writer::write_all" call "self.write()" and reach the implementer's "write" rather
     * than the abstract one. The pin is merged into @p gn_args and also returned, because inference rebuilds the
     * argument list from the prototype's own generic parameters and drops a name that is not one of them.
     * @param fn_proto The candidate prototype.
     * @param fn_scope The scope the candidate was declared in.
     * @param fn_owner_type The type the call was made on.
     * @param gn_args The generic arguments for this candidate, merged into in place.
     * @param sm The scope manager, positioned at the call site.
     * @param meta Associated metadata.
     * @return The pinned receiver type to re-apply after inference, or @c nullptr when nothing was pinned.
     */
    auto PinSelfToReceiver(
      asts::FunctionPrototypeAst const &fn_proto,
      scopes::Scope const *fn_scope,
      asts::TypeAst const &fn_owner_type,
      asts::GenericArgumentGroupAst &gn_args,
      scopes::ScopeManager const *sm,
      asts::meta::CompilerMetaData const *meta)
      -> Shared<asts::TypeAst> {
      using type_compare::TypeEq;

      const auto declared_self = fn_scope->GetEnclosingSelfType(*meta);
      const auto declared_self_sym = declared_self != nullptr
        ? fn_scope->GetTypeSymbol(declared_self.get())
        : nullptr;
      const auto declared_on_abstract = declared_self_sym != nullptr and declared_self_sym->LinkedScope != nullptr
        and not type_members::GetUnimplementedAbstractMethods(*declared_self_sym->LinkedScope).IsEmpty();

      auto self_pin = Shared<asts::TypeAst>(nullptr);
      if (declared_self != nullptr and (SignatureNamesSelf(fn_proto) or declared_on_abstract)) {
        auto receiver = fn_owner_type.WithConvention(nullptr);

        // "Self" only stands for the receiver when the receiver really is an implementer of the class the method was
        // declared on - this is what stops a forwarding type triggering it.
        const auto receiver_sym = sm->CurrentScope->GetTypeSymbol(receiver->WithoutGenerics().get());
        const auto receiver_implements_declarer = receiver_sym != nullptr and receiver_sym->LinkedScope != nullptr
          and genex::any_of(receiver_sym->LinkedScope->SupTypes(), [&](auto const &sup) {
            return TypeEq(*declared_self, *sup, *fn_scope, *receiver_sym->LinkedScope);
          });

        if (receiver_implements_declarer and not TypeEq(*declared_self, *receiver, *fn_scope, *sm->CurrentScope)) {
          if (not receiver->IsSelfType() and not receiver_sym->IsGeneric) {
            self_pin = receiver;
          }
          auto self_arg = Vec<Unique<asts::GenericArgumentAst>>();
          self_arg.EmplaceBack(MakeUnique<asts::GenericArgumentTypeKeywordAst>(
            asts::generate::common_types::SelfType(0), nullptr, std::move(receiver)));
          gn_args.MergeGenerics(std::move(self_arg));
        }
      }
      return self_pin;
    }

    auto InferAllGenerics(
      asts::FunctionPrototypeAst const &fn_proto,
      asts::FunctionParameterGroupAst const &fn_params,
      asts::FunctionCallArgumentGroupAst &fn_args,
      asts::GenericArgumentGroupAst &gn_args,
      const bool is_variadic_fn,
      scopes::Scope const *fn_scope,
      scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      //
      using generic_bindings::EnforceGenericConstraintsAllArgs;
      using generic_bindings::InferGnArgs;
      using func_utils::NameFnArgs;

      // Name the positional function arguments. The generic arguments
      // were named by the caller, which has to do it before it merges
      // the owner's and the "sup" block's arguments in.
      NameFnArgs(fn_args, fn_params, *sm, gn_args.GetAllArgs());

      // The inference source is all the function arguments (except for
      // "self")
      auto generic_infer_source = fn_args.GetKeywordArgs()
        | genex::views::remove_if([](auto const &a) { return a->Name->Val == "self"; })
        | genex::views::transform([&](auto const &x) { return MakePair(x->Name, x->Val->InferType(sm, meta)); })
        | genex::to<Vec>();

      // The inference target is all of the function parameters (except
      // for "self").
      auto generic_infer_target = fn_params.GetNonSelfParams()
        | genex::views::transform([](auto *x) { return MakePair(x->ExtractName(), x->Type); })
        | genex::to<Vec>();

      // Infer all of the generics from the function arguments and
      // parameters.
      InferGnArgs(
        *fn_proto.GnParamGroup, gn_args,
        MakeShared<generic_bindings::InferenceSourceMap>(
          generic_infer_source.begin(), generic_infer_source.end()),
        MakeShared<generic_bindings::InferenceTargetMap>(
          generic_infer_target.begin(), generic_infer_target.end()),
        meta->PostfixExpressionLhs->InferType(sm, meta),
        *fn_scope,
        is_variadic_fn ? fn_proto.FnParamGroup->GetVariadicParams()->ExtractName() : nullptr,
        false, *sm, *meta);

      EnforceGenericConstraintsAllArgs(
        *fn_proto.GnParamGroup, gn_args, *fn_scope, *sm, *meta);
    }

    /**
     * Drop the parameters whose type resolved to @c Void . A generic parameter bound to @c Void is not a parameter at
     * all - "fun f[T](x: T)" instantiated with "T=Void" takes nothing - so it is removed before the argument lists are
     * lined up. Walked backwards because the container shrinks as it goes.
     * @param fn_proto The candidate prototype, whose parameter list is edited in place.
     * @param fn_scope The scope the candidate was declared in, which its types resolve in.
     */
    auto StripVoidParams(
      asts::FunctionPrototypeAst const &fn_proto,
      scopes::Scope const *fn_scope)
      -> void {
      auto &params = fn_proto.FnParamGroup->Params;
      for (auto i = params.Len(); i > 0uz; --i) {
        if (type_predicates::IsTypeVoid(*params[i - 1uz]->Type, *fn_scope)) {
          genex::actions::erase(params, params.begin() + static_cast<sys::ssize_t>(i - 1uz));
        }
      }
    }

    /**
     * Check the call's keyword argument names against the candidate's parameter names, in both directions: no
     * argument may name a parameter that does not exist, and no required parameter may go unnamed.
     * Todo: This duplicates "func_utils::EnforceNoInvalidFnArgs", which runs earlier over the pre-substitution
     *  parameter list. The two differ only in the nouns their errors use, so merging them changes diagnostics.
     * @param func_params The candidate's parameter group, used as the error's context when it has no parameters.
     * @param func_param_names Every parameter's name.
     * @param func_param_names_req The required parameters' names.
     * @param func_arg_names The call's keyword argument names.
     * @param fn_call The call being checked, used as the error's context for a missing argument.
     * @param sm The scope manager, positioned at the call site.
     */
    auto CheckArgNamesAgainstParams(
      asts::FunctionParameterGroupAst const &func_params,
      Vec<Shared<asts::IdentifierAst>> const &func_param_names,
      Vec<Shared<asts::IdentifierAst>> const &func_param_names_req,
      Vec<asts::IdentifierAst*> const &func_arg_names,
      asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
      scopes::ScopeManager const *sm)
      -> void {
      using errors::SppArgumentNameInvalidError;
      using errors::SppArgumentMissingError;

      const auto invalid_args = func_arg_names
        | genex::views::not_in(func_param_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

      const auto param_ctx = func_params.Params.IsEmpty()
        ? static_cast<asts::Ast const*>(&func_params)
        : static_cast<asts::Ast const*>(func_params.Params[0].get());
      RaiseIf<SppArgumentNameInvalidError>(
        not invalid_args.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*param_ctx, "parameter", *invalid_args[0], "argument"));

      const auto missing_params = func_param_names_req
        | genex::views::not_in(func_arg_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();
      RaiseIf<SppArgumentMissingError>(
        not missing_params.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*missing_params[0], "parameter", fn_call, "argument"));
    }

    auto ValidateArgsMatchParams(
      asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
      asts::FunctionPrototypeAst const &fn_proto,
      scopes::Scope const *fn_scope,
      asts::FunctionCallArgumentGroupAst const &func_args,
      scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      //
      using errors::SppArgumentNameInvalidError;
      using errors::SppArgumentMissingError;
      using errors::SppTypeMismatchError;
      using type_compare::TypeEq;
      using type_compare::RelaxedTypeEq;

      StripVoidParams(fn_proto, fn_scope);

      // Recreate the lists of function parameters, and their
      // names ("Void" removed, generics etc).
      const auto func_params = fn_proto.FnParamGroup.get();
      const auto func_param_names = fn_proto.FnParamGroup->Params
        | genex::views::transform([](auto &&x) { return x->ExtractName(); })
        | genex::to<Vec>();
      const auto func_param_names_req = fn_proto.FnParamGroup->GetRequiredParams()
        | genex::views::transform([](auto &&x) { return x->ExtractName(); })
        | genex::to<Vec>();
      const auto func_arg_names = func_args.GetKeywordArgs()
        | genex::views::transform([](auto const &x) { return x->Name.get(); })
        | genex::to<Vec>();

      CheckArgNamesAgainstParams(
        *func_params, func_param_names, func_param_names_req, func_arg_names, fn_call, sm);

      // Type check the arguments against the parameters. Sort
      // the arguments into parameter order first.
      auto sorted_func_arguments = func_args.GetKeywordArgs();
      genex::actions::sort(
        sorted_func_arguments,
        {}, [&](asts::FunctionCallArgumentKeywordAst *arg) {
          return genex::position(func_param_names, [&arg](auto const &param) { return *arg->Name == *param; });
        });

      for (auto [arg, param] : genex::views::zip(sorted_func_arguments, func_params->GetAllParams())) {
        auto p_type = fn_scope->GetTypeSymbol(param->Type.get())->FqName()->WithConvention(
          asts::AstClone(param->Type->GetConvention()));
        if (p_type->IsSelfType()) {
          // Guarded: this used to dereference both casts, so a "Self" parameter on a call whose left-hand side is not
          // a postfix-with-type crashed rather than failing the candidate.
          if (const auto receiver = ReceiverTypeAtCallSite(meta); receiver != nullptr) {
            p_type = asts::AstClone(receiver)->WithConvention(asts::AstClone(p_type->GetConvention()));
          }
        }

        auto a_type = arg->InferType(sm, meta);
        auto temp = type_compare::GenericInferenceMap();

        if (const auto variadic_param = param->To<asts::FunctionParameterVariadicAst>(); variadic_param != nullptr) {
          const auto variadic_gn_param = fn_proto.GetNonGenericImpl()->GnParamGroup->GetVariadicParams();
          const auto orig_name = dynamic_shared_cast<asts::TypeIdentifierAst>(variadic_param->Source.OriginalType);
          const auto is_variadic_generic_type = variadic_gn_param != nullptr
            and orig_name != nullptr
            and *orig_name == *dynamic_shared_cast<asts::TypeIdentifierAst>(variadic_gn_param->Name);

          if (not is_variadic_generic_type) {
            auto ts = Vec(a_type->LastTypePart()->GnArgGroup->Args.Len(), p_type);
            p_type = asts::generate::common_types::TupleType(param->PosStart(), std::move(ts));
            p_type->Stage7_AnalyseSemantics(sm, meta);
          }
        }

        // Special case for "self" parameters.
        if (const auto self_param = param->To<asts::FunctionParameterSelfAst>(); self_param != nullptr) {
          arg->Conv = asts::AstClone(self_param->Conv);
        }

        // Regular parameter without arg folding. The double check is
        // required for generics applied to the superclass in sup-ext
        // that cannot be substituted because they can be anything,
        // so reverse type check them with the "relaxed" variation.
        // This is the only place this is required.
        else if (not type_compare::ConventionEq(*p_type, *a_type)
          or not TypeEq(*p_type, *a_type, *fn_scope, *sm->CurrentScope)) {
          // If the parameter's type is a generic that is rigid at
          // the call site (defined in a scope enclosing the caller,
          // so already fixed), the argument must match it exactly.
          const auto param_is_rigid_generic = IsRigidGenericAtCaller(
            *p_type, *sm->CurrentScope);
          const auto relaxed_matched = not param_is_rigid_generic
            and RelaxedTypeEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope, temp);

          // A relaxed match that only held because it bound a
          // generic the caller cannot choose is not a match.
          const auto relaxed_bound_rigid = relaxed_matched and genex::any_of(temp, [&](auto const &binding) {
            return IsRigidBindingAtCaller(*binding.first, *sm->CurrentScope);
          });

          RaiseIf<SppTypeMismatchError>(
            not relaxed_matched or relaxed_bound_rigid,
            {fn_scope, sm->CurrentScope}, ERR_ARGS(*param, *p_type, *arg, *a_type));
        }

        // The argument may have matched its parameter by forwarding
        // ("&Vec[T]" satisfying a "&View[T]" parameter), in which
        // case the value the callee is handed is the forwarded-to
        // one, so the argument becomes that call. This is the
        // argument-position counterpart of a method being called on
        // the value its receiver forwards to.
        //
        // Todo: an argument accepted by the relaxed match above never reaches here, because that branch and this one are
        //  alternatives. A parameter written as "&Self" is relaxed-matched against anything, so "eq(&self, that: &Self)"
        //  on a "StrView" takes a "&Str" unforwarded and reads it through "StrView"'s "{ptr, length}" shape - which is
        //  why "Str == Str" is false for equal strings. Moving the check out of the "else" is not enough on its own;
        //  "TypeFwdEq" also returns false for this pair and it is not yet clear why.
        else if (type_compare::TypeFwdEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope)) {
          if (auto fwd_call = type_utils::BuildFwdCall(*arg->Val, *a_type, sm, meta); fwd_call != nullptr) {
            arg->Val = std::move(fwd_call);
          }
        }
      }
    }

    /**
     * Narrow a set of matching overloads to the one whose return type the context asked for. Done separately from the
     * per-candidate matching above so that a call with no return-type match still reports the argument mismatches it
     * had, rather than an empty candidate set. Only an unambiguous single match narrows; anything else is left alone
     * for the ordinary ambiguity reporting to deal with.
     * @param pass_overloads The overloads that matched on arguments, narrowed in place.
     * @param sm The scope manager, positioned at the call site.
     * @param meta Associated metadata, carrying the requested return type.
     */
    auto NarrowByReturnType(
      Vec<PassedOverload> &pass_overloads,
      scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      using type_compare::TypeEq;
      using type_utils::ResolveAndSubstituteSelfType;

      if (meta->ReturnTypeOverloadResolverType == nullptr) { return; }

      auto return_matches = Vec<PassedOverload>();
      for (auto &&matched : pass_overloads) {
        auto ret = asts::AstCloneShared(matched.Proto->ReturnType);
        auto tm = scopes::ScopeManager(sm->GlobalScope, const_cast<scopes::Scope*>(matched.FnScope));
        ret = ResolveAndSubstituteSelfType(*ret, *matched.FnScope, tm, *meta);

        if (TypeEq(*ret, *meta->ReturnTypeOverloadResolverType, *matched.FnScope, *sm->CurrentScope)) {
          return_matches.EmplaceBack(std::move(matched));
        }
      }

      if (return_matches.Len() == 1) { pass_overloads = std::move(return_matches); }
    }

    auto ManageMatchedOverloads(
      asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
      Vec<PassedOverload> const &pass_overloads,
      Vec<FailedOverload> const &fail_overloads,
      asts::FunctionCallArgumentGroupAst const &arg_group,
      scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      // If there are no pass overloads, raise an error.
      using namespace std::string_literals;
      if (pass_overloads.IsEmpty()) {
        auto failed_signatures_and_errors = "\n" + (fail_overloads
          | genex::views::transform([](auto const &f) {
            return "    - "s + f.Proto->PrintSignature("") + ": "s + f.Reason;
          })
          | genex::views::intersperse("\n"_str)
          | genex::views::join
          | genex::to<Str>());

        auto arg_usage_signature = arg_group.Args
          | genex::views::transform([sm, meta](auto const &x) {
            return x->GetSelfType() == nullptr ? x->InferType(sm, meta)->ToString() : "Self";
          })
          | genex::views::intersperse(", "_str)
          | genex::views::join
          | genex::to<Str>();

        auto sub_errors = fail_overloads
          | genex::views::transform([](auto const &f) { return f.Error; })
          | genex::to<Vec>();

        Raise<errors::SppFunctionCallNoValidSignaturesError>(
          {sm->CurrentScope}, ERR_ARGS(fn_call, failed_signatures_and_errors, arg_usage_signature),
          std::move(sub_errors));
      }

      // If there are multiple pass overloads, raise an error.
      if (pass_overloads.Len() > 1) {
        auto signatures = "\n" + (pass_overloads
          | genex::views::transform([](auto const &x) { return "    - "s + x.Proto->PrintSignature(""); })
          | genex::views::intersperse("\n"_str)
          | genex::views::join
          | genex::to<Str>());

        auto arg_usage_signature = arg_group.Args
          | genex::views::transform([sm, meta](auto const &x) {
            return x->GetSelfType() == nullptr ? x->InferType(sm, meta)->ToString() : "Self";
          })
          | genex::views::intersperse(", "_str)
          | genex::views::join
          | genex::to<Str>();

        Raise<errors::SppFunctionCallOverloadAmbiguousError>(
          {sm->CurrentScope}, ERR_ARGS(fn_call, signatures, arg_usage_signature));
      }
    }

    /**
     * Rewrite each generic argument to what it actually names at the call site, then drop the ones that only restate
     * their own parameter. A type argument naming a bound generic becomes the type it is bound to; a comp argument
     * naming a bound comp generic becomes the value read back off its symbol. An unbound parameter names nothing yet
     * and is left alone, which is what keeps a template's signature written in its own terms.
     * @param combined_generics The argument group to normalise, rewritten in place.
     * @param sm The scope manager, positioned at the call site.
     */
    auto NormaliseGenericArgs(
      asts::GenericArgumentGroupAst &combined_generics,
      scopes::ScopeManager const *sm)
      -> void {
      // An unbound parameter is what it looks like and is left alone: its symbol carries no class prototype, which is
      // what separates it from a parameter bound to a real type.
      for (auto *arg : combined_generics.Args
           | genex::views::ptr
           | genex::views::cast_dynamic<asts::GenericArgumentTypeKeywordAst*>()) {
        const auto val_sym = sm->CurrentScope->GetTypeSymbol(arg->Val.get());
        if (val_sym == nullptr or not val_sym->IsGeneric or val_sym->Type == nullptr) { continue; }
        if (val_sym->LinkedScope == nullptr or val_sym->LinkedScope->TySym == nullptr) { continue; }
        arg->Val = val_sym->LinkedScope->TySym->FqName();
      }

      // The same for a comp-time argument naming a bound comp generic. A binding is a variable symbol carrying the
      // argument it was bound from, so what the name resolves to is read back off that.
      for (auto *arg : combined_generics.Args
           | genex::views::ptr
           | genex::views::cast_dynamic<asts::GenericArgumentCompKeywordAst*>()) {
        const auto val_ident = arg->Val->To<asts::IdentifierAst>();
        if (val_ident == nullptr) { continue; }

        const auto val_sym = sm->CurrentScope->GetVarSymbol(val_ident);
        if (val_sym == nullptr or val_sym->MemInfo->AstCompTime == nullptr) { continue; }

        const auto bound_arg = val_sym->MemInfo->AstCompTime->To<asts::GenericArgumentCompKeywordAst>();
        if (bound_arg == nullptr or bound_arg->Val == nullptr) { continue; }
        arg->Val = asts::AstClone(bound_arg->Val);
      }

      // Drop the arguments that only restate their parameter. What is left is what this instantiation actually pins,
      // and if that is nothing then there is no instantiation to make.
      combined_generics.Args |= genex::actions::remove_if(
        [](auto const &a) { return generic_bindings::BindsToItself(*a); });
    }

    auto PotentiallyGenerateGenericSubstitutedPrototype(
      asts::FunctionPrototypeAst *fn_proto,
      scopes::Scope const *fn_scope,
      asts::GenericArgumentGroupAst &generic_args,
      Shared<asts::TypeAst> const &variadic_pack_type,
      scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta)
      -> Tup<asts::FunctionPrototypeAst*, scopes::Scope const*> {
      //
      using errors::SppSecondClassBorrowViolationError;
      using monomorphization_utils::CreateGenericFunScope;
      using type_predicates::IsTypeBorrowed;

      // Inference has already produced a binding for every one of
      // this prototype's generic parameters, including the ones it
      // inherited from the enclosing "sup" block.
      auto &combined_generics = generic_args;

      NormaliseGenericArgs(combined_generics, sm);

      // Separate variadic instantiation by the types going into the
      // variadic function parameter.
      if (variadic_pack_type != nullptr) {
        auto pack_name = MakeUnique<asts::TypeIdentifierAst>(
          variadic_pack_type->PosStart(),
          "VariadicPackOf" + fn_proto->FnParamGroup->GetVariadicParams()->ExtractName()->Val,
          nullptr);
        combined_generics.Args.EmplaceBack(MakeUnique<asts::GenericArgumentTypeKeywordAst>(
          std::move(pack_name), nullptr, asts::AstClone(variadic_pack_type)));
      }

      // Consider if we need to create a generic substituted
      // function prototype.
      if (not combined_generics.Args.IsEmpty()) {
        // Reuse the instantiation for these exact arguments if one
        // already exists.
        if (auto [existing_scope, existing_proto] = fn_proto->FindGenericSubstitution(combined_generics);
          existing_proto != nullptr) {
          return {existing_proto, existing_scope};
        }

        auto new_fn_proto = asts::AstClone(fn_proto);
        new_fn_proto->SetNonGenericImpl(fn_proto);
        new_fn_proto->DetachLlvmFuncSlot();

        // Create the new function scope for the generic implementation.
        const auto generic_syms = sm->CurrentScope->GetExtendedGenericSymbols(combined_generics.GetAllArgs());
        const auto new_fn_scope = CreateGenericFunScope(
          *fn_scope, asts::GenericArgumentGroupAst(nullptr, AstCloneVec(combined_generics.Args), nullptr),
          generic_syms, sm, meta);
        auto tm = scopes::ScopeManager(sm->GlobalScope, new_fn_scope);

        // Drop only the parameters this substitution actually bound.
        new_fn_proto->GnParamGroup->Params |= genex::actions::remove_if([&](auto const &param) {
          return genex::any_of(combined_generics.Args, [&](auto const &arg) {
            return arg->ViewName() == param->Name->ToString();
          });
        });

        auto &generic_sub_slot = asts::AstBody(
          fn_scope->AstNode)[0]->To<asts::FunctionPrototypeAst>()->RegisteredGenericSubstitutions().back();

        // Substitute and analyse the function parameters and return
        // type.
        for (auto *p : new_fn_proto->FnParamGroup->GetNonSelfParams()) {
          p->Type = p->Type->SubstituteGenerics(combined_generics.GetAllArgs());
          p->Type->Stage7_AnalyseSemantics(&tm, meta);
        }

        ReattachCallableConstraints(*new_fn_proto, *fn_proto, new_fn_scope, combined_generics, tm, meta);
        RetypeInheritedSymbols(*new_fn_proto, new_fn_scope, combined_generics, variadic_pack_type, tm, meta);

        new_fn_proto->ReturnType = new_fn_proto->ReturnType->SubstituteGenerics(combined_generics.GetAllArgs());
        new_fn_proto->ReturnType->Stage7_AnalyseSemantics(&tm, meta);

        // Check the new return type isn't a borrow type.
        RaiseIf<SppSecondClassBorrowViolationError>(
          IsTypeBorrowed(*new_fn_proto->ReturnType, tm),
          {sm->CurrentScope},
          ERR_ARGS(*new_fn_proto->ReturnType, *new_fn_proto->ReturnType, "substituted function return type"));

        generic_sub_slot.IsConcrete = ComputeIsConcrete(
          combined_generics, *new_fn_proto, new_fn_scope, tm, sm, meta);

        // Save the generic implementation against the base function,
        // and update the active scope and prototype.
        const auto new_fn_proto_ptr = new_fn_proto.get();
        generic_sub_slot.Proto = std::move(new_fn_proto);
        return {new_fn_proto_ptr, new_fn_scope};
      }

      return {fn_proto, fn_scope};
    }
  }
}

#define SPP_FN_RES_ERR_WRAPPER(error_type, message)                          \
  catch (error_type const &e) {                                              \
    fail_overloads.EmplaceBack(FailedOverload{fn_proto, e.what(), message}); \
    while (meta->Depth() > original_meta_depth) { meta->Restore(); }         \
  }

auto spp::analyse::utils::overload_utils::DetermineOverload(
  asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Pair<PassedOverload, bool> {
  //
  using scopes::ScopeManager;
  using errors::SppFunctionCallTooManyArgumentsError;
  using type_compare::TypeEq;
  using type_utils::ResolveAndSubstituteSelfType;

  auto lhs = meta->PostfixExpressionLhs;

  // Todo: Workaround for aliased variable symbols being used
  //  as function targets, due to scope lookup.
  auto temp = Shared<asts::ExpressionAst>(nullptr);
  if (const auto id = lhs->To<asts::IdentifierAst>()) {
    const auto mod_scope = sm->CurrentScope->ParentModule();
    const auto x = mod_scope != nullptr ? mod_scope->GetVarSymbol(id) : sm->CurrentScope->GetVarSymbol(id);
    if (x and x->MemInfo->AstCompTime) {
      temp = x->FqName();
      lhs = temp.get();
    }
  }

  // Extract metadata about the target function's overloads
  // such as the function's owner and scope.
  const auto [fn_owner_type, fn_owner_scope, fn_name] = GetFuncOwnerTypeAndFuncName(
    *lhs, *sm, meta);

  const auto is_postfix = meta->PostfixExpressionLhs->To<asts::PostfixExpressionAst>();
  const auto is_runtime = is_postfix
    ? is_postfix->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>()
    : nullptr;

  // If we are resolving a method, then use the free function
  // equivalent. For example, convert "1.add(2)" to
  // "S32::add(1, 2)".
  if (is_runtime != nullptr and fn_owner_type != nullptr and fn_name != nullptr) {
    auto propagated = PropagateMethodToFunction(
      fn_call, *fn_owner_type, *fn_name, *is_postfix, sm, meta);
    fn_call.SetTransformedAst(std::move(propagated.TransformedAst));
    return {std::move(propagated.Overload), propagated.IsClosure};
  }

  // Get all the overloads to deal with, and handle closure
  // mechanics. The owner scope is passed as a pointer because
  // it is legitimately null for a callee that is not a name.
  auto candidates = RetrieveAllOverloads(
    fn_name.get(), fn_owner_scope, sm, meta);
  auto pass_overloads = Vec<PassedOverload>{};
  auto fail_overloads = Vec<FailedOverload>{};
  auto original_meta_depth = meta->Depth();

  // Check each provided overload for a complete match.
  for (auto &candidate : candidates.Overloads) {
    auto *&fn_proto = candidate.Proto;
    auto const *&fn_scope = candidate.FnScope;

    // Extract generic/function parameter information from the
    // overload.
    const auto fn_params = fn_proto->FnParamGroup.get();
    const auto gn_params = fn_proto->GnParamGroup.get();
    auto fn_args = asts::AstClone(fn_call.FnArgGroup);
    auto gn_args = asts::AstClone(fn_call.GnArgGroup);
    const auto is_variadic_fn =
      fn_proto->FnParamGroup->GetVariadicParams() != nullptr;

    try {
      // Cannot check for "too few" arguments here because of
      // potential "T=Void" + "x: T" removal. Check if there
      // are too many arguments (for a non-variadic function).
      RaiseIf<SppFunctionCallTooManyArgumentsError>(
        fn_args->Args.Len() > fn_params->Params.Len() and not is_variadic_fn, {fn_scope},
        ERR_ARGS(*fn_proto, fn_proto->FnParamGroup->Params.Len(), fn_call, fn_call.FnArgGroup->Args.Len()));

      // Every generic argument this call is resolved with, merged
      // once, here. Precedence runs highest first: what the call
      // wrote, then what the owner type (or the forwarding type
      // it was reached through) pins, then what the enclosing "sup"
      // block declares - the first binding offered for a name wins.
      generic_bindings::NameGnArgs(*gn_args, *gn_params, *fn_proto->Name, *sm, *meta);
      gn_args->MergeGenerics(RetrieveOwnerGenericArgs(candidate.FwdType, meta));
      gn_args->MergeGenerics(std::move(candidate.SupGenerics->Args));

      const auto self_pin = PinSelfToReceiver(
        *fn_proto, fn_scope, *fn_owner_type, *gn_args, sm, meta);

      InferAllGenerics(
        *fn_proto, *fn_params, *fn_args, *gn_args, is_variadic_fn, fn_scope, sm, meta);

      // Inference rebuilds the argument list from the prototype's
      // generic parameters, so the pin above - whose name is not
      // one of them - is dropped on the way out. Put it back.
      if (self_pin != nullptr) {
        auto self_arg = Vec<Unique<asts::GenericArgumentAst>>();
        self_arg.EmplaceBack(MakeUnique<asts::GenericArgumentTypeKeywordAst>(
          asts::generate::common_types::SelfType(0), nullptr, std::move(self_pin)));
        gn_args->MergeGenerics(std::move(self_arg));
      }

      // "InferAllGenerics" has run "NameFnArgs", so the trailing
      // arguments of a variadic call are already collapsed into one
      // tuple-valued argument. Its type is what the callee's variadic
      // parameter actually receives.
      auto variadic_pack_type = Shared<asts::TypeAst>(nullptr);
      if (is_variadic_fn) {
        const auto variadic_name = fn_proto->FnParamGroup->GetVariadicParams()->ExtractName();
        for (auto const &a : fn_args->GetKeywordArgs()) {
          if (a->Name->Val != variadic_name->Val) { continue; }
          variadic_pack_type = a->Val->InferType(sm, meta);
          for (auto *part : variadic_pack_type->TypeParts()) { part->ClearSourceWritten(); }
          break;
        }
      }

      std::tie(fn_proto, fn_scope) = PotentiallyGenerateGenericSubstitutedPrototype(
        fn_proto, fn_scope, *gn_args, variadic_pack_type, sm, meta);
      ValidateArgsMatchParams(
        fn_call, *fn_proto, fn_scope, *fn_args, sm, meta);
      pass_overloads.EmplaceBack(
        PassedOverload{fn_scope, fn_proto, std::move(fn_args)});
    }

    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppFunctionCallAbstractFunctionError, "calling an abstract function")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppFunctionCallTooManyArgumentsError, "too many arguments")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppArgumentNameInvalidError, "invalid argument name")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppArgumentMissingError, "missing required argument")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppTypeMismatchError, "type mismatch")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppGenericParameterConflictError, "inferred generic parameter conflict")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppGenericParameterNotInferredError, "generic parameter not inferred")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppGenericArgumentTooManyError, "too many generic arguments")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppGenericConstraintError, "generic constraint not satisfied")
  }

  NarrowByReturnType(pass_overloads, sm, meta);

  ManageMatchedOverloads(
    fn_call, pass_overloads, fail_overloads,
    *fn_call.FnArgGroup, sm, meta);

  // Store the closure if it was generated as part of the
  // overload resolution.
  if (candidates.ClosureProto) {
    fn_call.SetClosureDummyProto(std::move(candidates.ClosureProto));
  }
  return {std::move(pass_overloads[0]), candidates.IsClosure};
}
