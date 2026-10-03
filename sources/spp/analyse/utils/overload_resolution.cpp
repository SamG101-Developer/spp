module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.overload_resolution;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.packs;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.subroutine_prototype_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import sys;

namespace spp::analyse::utils::overload_resolution {
  namespace {
    /// Reject the first keyword argument naming no parameter, reported against the first parameter (or the argument
    /// itself, when there are none).
    auto EnforceFnArgNamesKnown(
      Vec<Shared<IdentifierAst>> const &param_names,
      Vec<IdentifierAst*> const &arg_names,
      FunctionParameterGroupAst const &params,
      ScopeManager const &sm)
      -> void {
      for (auto const *arg_name : arg_names) {
        if (genex::any_of(param_names, [arg_name](auto const &p) { return *p == *arg_name; })) { continue; }
        auto const &param_ctx = params.Params.IsEmpty()
          ? static_cast<Ast const&>(*arg_name)
          : static_cast<Ast const&>(*params.Params[0]);
        Raise<errors::SppArgumentNameInvalidError>(
          {sm.CurrentScope}, ERR_ARGS(param_ctx, StrView("fn param"), *arg_name, StrView("fn arg")));
      }
    }

    /// The owning scope of a function, where the scope's ast
    /// is the function, and the parent is the "sup $FuncName
    /// ext ..."
    auto OwningBlockOf(
      Scope const &found_in, FunctionPrototypeAst const *fn) -> Scope const* {
      const auto block = genex::find_if(
        found_in.Children, [fn](auto const &child) { return fn_values::FnBlockOf(*child).second == fn; });
      return block != found_in.Children.end() ? block->get() : nullptr;
    }

    /// When analysing lists of function overloads that can be
    /// called, we need to prune off base class functions that
    /// have overrides present, otherwise we get false
    /// ambiguities being reported. The depth difference is what
    /// is used to determine the override. The overridden ones
    /// are collected and removed after the scan, as removing
    /// them from the list being scanned invalidates the scan.
    auto PruneOverriddenOverloads(
      Vec<FnOverload> &overload_scopes, Scope const *target_scope,
      ScopeManager &sm, meta::CompilerMetaData *meta) -> void {
      auto overridden = Set<FunctionPrototypeAst const*>();
      for (auto const &o1 : overload_scopes) {
        for (auto const &o2 : overload_scopes) {
          // This depth difference checker ensures the derived version is kept.
          if (o1.Proto != o2.Proto
            and target_scope->DepthDiff(o1.FnScope) < target_scope->DepthDiff(o2.FnScope)) {
            // The prototype comes off the ast, which a generic "sup"
            // block's instantiations share with its template, so its
            // own scope is the template's, under the template's
            // unbound parameters. The instantiation it was found
            // through clones the block's member scopes along with it,
            // and its copy of the function's block resolves the
            // signature against the instantiation's bindings instead.
            // Without one - the template itself, or a blanket block
            // attached unsubstituted - the template's block is the right
            // one.
            const auto own_block = OwningBlockOf(*o1.FnScope, o1.Proto);
            auto const &sig_scope = own_block != nullptr ? *own_block : *o1.Proto->GetAstScope()->Parent;
            auto conflict = fn_values::CheckForConflictingOverride(sig_scope, o2.FnScope, *o1.Proto, sm, meta);
            if (conflict != nullptr) { overridden.insert(conflict); }
          }
        }
      }
      overload_scopes |= genex::actions::remove_if([&overridden](auto const &info) {
        return overridden.contains(info.Proto);
      });
    }

    /// Point each overload at the "sup" block that actually
    /// declares it, rather than the scope it was found through.
    /// Todo: This is a bit of a hack, but it works. Tidy module
    ///  and eliminate this.
    auto NarrowToOwningBlock(
      Vec<FnOverload> &overload_scopes,
      auto const &is_valid_ext_scope)
      -> void {
      for (auto &info : overload_scopes) {
        // "Any block will do" as a fallback, but there may be
        // none at all, in which case the scope stays as it was.
        auto owning_block = OwningBlockOf(*info.FnScope, info.Proto);
        if (owning_block == nullptr) {
          const auto blocks = info.FnScope->Children
            | genex::views::ptr
            | genex::views::filter(is_valid_ext_scope)
            | genex::to<Vec>();
          if (not blocks.IsEmpty()) { owning_block = blocks[0]; }
        }
        if (owning_block != nullptr) { info.FnScope = owning_block; }
      }
    }

    struct OverloadCandidates {
      bool IsClosure;
      Unique<FunctionPrototypeAst> ClosureProto;
      Vec<overload_resolution::FnOverload> Overloads;
    };

    struct PropagatedMethodCall {
      PassedOverload Overload;
      bool IsClosure;
      Unique<PostfixExpressionAst> TransformedLhs;
      Unique<FunctionPrototypeAst> ClosureProto;
    };

    auto GetFnOwnerTypeAndFnName(
      ExpressionAst const &lhs,
      ScopeManager &sm,
      meta::CompilerMetaData *meta)
      -> Tup<Shared<TypeAst>, Scope const*, Shared<IdentifierAst>> {
      //
      using member_lookup::RaiseMissingIdentifierAndClosestOptions;

      // Define some expression casts that are used commonly.
      const auto postfix_lhs = lhs.To<PostfixExpressionAst>();
      const auto runtime_field = postfix_lhs
        ? postfix_lhs->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()
        : nullptr;
      const auto static_field = postfix_lhs
        ? postfix_lhs->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>()
        : nullptr;

      // Specific casts.
      const auto postfix_lhs_as_type = postfix_lhs ? postfix_lhs->Lhs->To<TypeAst>() : nullptr;
      const auto lhs_as_ident = lhs.To<IdentifierAst>();

      // If the lhs is an identifier, it must be a variable
      // symbol, not a namespace symbol.
      if (lhs_as_ident and sm.CurrentScope->FindVarSymbol(lhs_as_ident) == nullptr) {
        RaiseMissingIdentifierAndClosestOptions(*lhs_as_ident, sm.CurrentScope->GetAllVarSymbols(), {}, sm);
      }

      // Variables that will be set in each branch, and
      // returned. These are used to determine what variation
      // of function call is being performed.
      auto fn_owner_type = Shared<TypeAst>(nullptr);
      auto fn_owner_scope = static_cast<Scope const*>(nullptr);
      auto fn_name = Shared<IdentifierAst>(nullptr);

      // Runtime access into an object: "object.method()".
      // No namespacing involved.
      if (postfix_lhs != nullptr and runtime_field != nullptr) {
        fn_owner_type = postfix_lhs->Lhs->InferType(&sm, meta);
        fn_name = runtime_field->Name;
        fn_owner_scope = sm.CurrentScope->FindTypeSymbol(fn_owner_type.get())->LinkedScope;
      }

      // Static access into a type: "Type::method()" or
      // "ns::Type::method()".
      else if (static_field != nullptr and postfix_lhs_as_type != nullptr) {
        fn_owner_type = AstCloneShared(postfix_lhs_as_type);
        fn_name = static_field->Name;
        fn_owner_scope = sm.CurrentScope->FindTypeSymbol(fn_owner_type.get())->LinkedScope;
      }

      // Direct access into a namespaced free function:
      // "std::io::print(variable)".
      else if (postfix_lhs != nullptr and static_field != nullptr) {
        fn_owner_scope = sm.CurrentScope->ConvertPostfixToNestedScope(postfix_lhs->Lhs.get());
        fn_name = static_field->Name;

        // Add a name check here because we need to get
        // the type off of it before it is even analysed.
        const auto fn_owner_sym = fn_owner_scope->FindVarSymbol(fn_name.get());
        if (fn_owner_sym == nullptr) {
          RaiseMissingIdentifierAndClosestOptions(*fn_name, fn_owner_scope->GetAllVarSymbols(), {}, sm);
        }
        fn_owner_type = fn_owner_sym->Type;
      }

      // Direct access into a non-namespaced function:
      // "function()":
      else if (lhs_as_ident != nullptr) {
        fn_owner_type = nullptr;
        fn_name = AstCloneShared(lhs_as_ident);

        // A name declared inside the function (a function-type
        // variable) is a value being called, not a module function
        // spelled the same.
        const auto mod_scope = sm.CurrentScope->GetParentModule();
        const auto sym = sm.CurrentScope->FindVarSymbol(lhs_as_ident);
        const auto is_local = sym != nullptr and sym->ScopeDefinedIn != mod_scope;
        fn_owner_scope = is_local ? nullptr : mod_scope;
      }

      // Non-callable AST.
      else {
        fn_owner_type = nullptr;
        fn_name = nullptr;
        fn_owner_scope = nullptr;
      }

      // A value of a named function's own type is that function, so
      // calling it is calling the function - resolved like the name,
      // overloads and generics included, not through a pointer.
      if (fn_owner_type == nullptr and fn_owner_scope == nullptr) {
        const auto lhs_ref = const_cast<ExpressionAst&>(lhs).InferTypeRef(&sm, meta);
        if (auto [value_fn_name, value_fn_scope] = fn_values::GetFnValueName(lhs_ref);
          value_fn_name != nullptr) {
          fn_name = std::move(value_fn_name);
          fn_owner_scope = value_fn_scope;
        }
      }

      return {fn_owner_type, fn_owner_scope, fn_name};
    }

    auto ConvertMethodToFnForm(
      TypeAst const &function_owner_type,
      IdentifierAst const &function_name,
      PostfixExpressionAst const &lhs,
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      ScopeManager &sm,
      meta::CompilerMetaData *meta)
      -> Pair<Unique<PostfixExpressionAst>, Unique<PostfixExpressionOperatorFunctionCallAst>> {
      // A method reached through a forwarding type is invoked
      // on the forwarded-to value, not on the object that forwards
      // to it: "w.greet()" calls "greet" on "w.fwd_ref()". The
      // member access has already built that call, so use it as
      // the receiver; a method found on the object's own type uses
      // the object itself.
      // Todo: Check this for when we use a method on a type who has a forwarding type, but the forward isn't used.
      const auto member_access = lhs.Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
      const auto fwd_receiver = member_access != nullptr ? member_access->GetFwdReceiver() : nullptr;
      const auto self_expr = fwd_receiver != nullptr ? fwd_receiver : lhs.Lhs.get();
      auto self_arg_val = AstClone(self_expr);

      // Create the static method access (without the function
      // call and args).
      auto field = MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(
        nullptr, AstClone(&function_name));
      auto field_access = MakeUnique<PostfixExpressionAst>(
        AstClone(&function_owner_type), std::move(field));

      // Create an argument for "self" and inject it into the
      // current arguments.
      auto self_arg = MakeUnique<FunctionCallArgumentPositionalAst>(
        nullptr, nullptr, std::move(self_arg_val));
      auto fn_args = std::move(fn_call.FnArgGroup->Args);
      fn_args.Insert(fn_args.begin(), std::move(self_arg));

      // Create the function call with the new arguments.
      auto new_fn_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
        AstClone(fn_call.GnArgGroup), AstClone(fn_call.FnArgGroup), nullptr);
      new_fn_call->FnArgGroup->Args = std::move(fn_args);

      // The forwarding receiver is a "GenOnce" call that resumes
      // itself, so its type is the borrow it yields.
      new_fn_call->FnArgGroup->Args[0]->SetSelfType(self_expr->InferType(&sm, meta));
      new_fn_call->Source.OriginalExpr = fn_call.Source.OriginalExpr;

      // Return the new ASTs.
      return {std::move(field_access), std::move(new_fn_call)};
    }

    auto CreateCallablePrototype(
      TypeAst const &expr_ty)
      -> Unique<FunctionPrototypeAst> {
      // Extract the parameter and return types from the
      // expression type. A type that carries neither is not one
      // a call can be built from (like an alias that was never
      // resolved, say) and the caller reports it as having no
      // valid signatures rather than crashing on it. As alias
      // analysis improves, this can be removed.
      const auto gn_args = expr_ty.LastTypePart()->GnArgGroup.get();
      const auto out_arg = gn_args != nullptr ? gn_args->At("Out") : nullptr;
      const auto args_arg = gn_args != nullptr ? gn_args->At("Args") : nullptr;
      if (out_arg == nullptr or args_arg == nullptr
        or out_arg->IsCompArg() or args_arg->IsCompArg()
        or args_arg->TypeVal->LastTypePart()->GnArgGroup == nullptr) { return nullptr; }

      auto ret_ty = out_arg->TypeVal;
      auto p_tys = args_arg->TypeVal->LastTypePart()->GnArgGroup->GetTypeArgs()
        | genex::views::transform([](auto *g) {
          return MakeUnique<FunctionParameterRequiredAst>(nullptr, nullptr, g->TypeVal);
        })
        | spp::views::cast_unique<FunctionParameterAst>();

      // Create a function prototype based off of the parameter
      // and return type.
      // Todo: When might it be a coroutine, not a subroutine?
      // Todo: Do we set "cmp" here for the subroutine ever?
      auto dummy_param_group = MakeUnique<FunctionParameterGroupAst>(
        nullptr, std::move(p_tys), nullptr);
      auto dummy_name = MakeUnique<IdentifierAst>(
        0uz, "<anonymous>");
      auto dummy_overload = MakeUnique<SubroutinePrototypeAst>(
        SPP_NO_ANNOTATIONS, nullptr, nullptr, std::move(dummy_name),
        nullptr, std::move(dummy_param_group),
        nullptr, std::move(ret_ty), nullptr);

      // Return the function prototype.
      return dummy_overload;
    }

    /**
     * Whether a prototype's signature is written in terms of @c Self , and so reads differently per implementer.
     */
    auto SignatureNamesSelf(
      FunctionPrototypeAst const &fn_proto)
      -> bool {
      // We need this so that for example when a TcpSocket method
      // is called that belongs to Socket, the "self=TcpSocket"
      // IR is available, not "self=Socket" + weird slicing / owned
      // value mismatch - for borrows it's fine because opaque ptrs.
      // Todo: a borrowed "self" is not fine. The pointer is opaque, but the body is compiled once against the
      //  declaring class - so its field GEPs use the base layout (which "SortMembersForSppLayout" reorders
      //  independently of the derived one), and a call inside it resolves against the base's overload set rather
      //  than the receiver's override. Both are only reachable when the base declares no abstract method, since
      //  "declared_on_abstract" in "PinSelfToReceiver" otherwise mints the per-receiver copy. Red tests:
      //  "regression::tst::codegen::test_inherited_borrowed_self_method_reads_the_derived_layout" and
      //  "test_inherited_default_method_reaches_the_override". See "docs/dyn-dispatch-design.md".
      const auto self_param = fn_proto.FnParamGroup->GetSelfParam();
      if (self_param != nullptr and self_param->Conv == nullptr) { return true; }

      return type_predicates::DoesTypeNameSelf(*fn_proto.ReturnType)
        or genex::any_of(
          fn_proto.FnParamGroup->GetNonSelfParams(),
          [&](auto const *p) { return type_predicates::DoesTypeNameSelf(*p->Type); });
    }

    /**
     * The type named on the left of the call site, if the call was made on one at all: "Vec[Str]::new()" has
     * "Vec[Str]" here, while "obj.method()" and a module-level "f()" have nothing. Three copies of this derivation had
     * drifted apart across this module and "fn_values", one of them dereferencing both casts unguarded, so it is
     * written once.
     * @param meta Associated metadata, whose @c PostfixExpressionLhs is the call site's left-hand side.
     * @return The receiver type, or @c nullptr when the call was not made on one.
     */
    auto ReceiverTypeAtCallSite(
      meta::CompilerMetaData const *meta)
      -> TypeAst* {
      if (meta->PostfixExpressionLhs == nullptr) { return nullptr; }
      const auto postfix = meta->PostfixExpressionLhs->To<PostfixExpressionAst>();
      return postfix != nullptr ? postfix->Lhs->To<TypeAst>() : nullptr;
    }

    auto RetrieveAllOverloads(
      IdentifierAst const *fn_name,
      Scope const *fn_owner_scope,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> OverloadCandidates {
      //
      using fn_values::IsTargetCallable;

      // For named functions (ie non-closures), get all the
      // function overload implementation scopes.
      auto all_overloads = fn_name != nullptr and fn_owner_scope != nullptr
        ? overload_resolution::GetAllFnScopes(*fn_name, fn_owner_scope, *sm, meta)
        : Vec<overload_resolution::FnOverload>{};
      if (not all_overloads.IsEmpty()) {
        return OverloadCandidates{false, nullptr, std::move(all_overloads)};
      }

      // If there are no scopes, assume that this is a closure
      // (do functional type check).
      const auto closure_fn_type = fn_values::IsTargetCallable(*meta->PostfixExpressionLhs, *sm, meta);
      if (auto closure_fn_proto = closure_fn_type != nullptr
        ? CreateCallablePrototype(*closure_fn_type)
        : nullptr; closure_fn_proto != nullptr) {
        all_overloads.EmplaceBack(overload_resolution::FnOverload{
          .FnScope = sm->CurrentScope,
          .Proto = closure_fn_proto.get(),
          .SupGns = GenericArgumentGroupAst::NewEmpty(),
          .FwdType = nullptr
        });
        return OverloadCandidates{true, std::move(closure_fn_proto), std::move(all_overloads)};
      }

      // Otherwise, there are no scopes (handled in caller).
      return OverloadCandidates{false, nullptr, {}};
    }

    /**
     * The expression a call names its target by: the written left-hand side, unless it is a name for a module-level
     * function reached as a constant, which is named by its qualified form instead. The second element keeps that form
     * alive.
     * Todo: Workaround for aliased variable symbols being used as function targets, due to scope lookup.
     */
    auto CalleeExpr(
      ScopeManager const &sm,
      meta::CompilerMetaData const *meta)
      -> Pair<ExpressionAst const*, Shared<ExpressionAst>> {
      auto const *const lhs = meta->PostfixExpressionLhs;
      if (const auto id = lhs->To<IdentifierAst>()) {
        // A name declared inside the function (a function-type
        // variable) is a value being called, not a module function
        // spelled the same.
        const auto local = sm.CurrentScope->FindVarSymbol(id);
        if (local == nullptr or local->IsCompTime()) {
          const auto mod_scope = sm.CurrentScope->GetParentModule();
          const auto x = mod_scope != nullptr ? mod_scope->FindVarSymbol(id) : local;
          if (x and x->IsCompTime()) {
            auto qualified = x->FqName();
            return {qualified.get(), std::move(qualified)};
          }
        }
      }
      return {lhs, nullptr};
    }

    auto PropagateMethodToFn(
      PostfixExpressionOperatorFunctionCallAst &fn_call,
      TypeAst const &fn_owner_type,
      IdentifierAst const &fn_name,
      PostfixExpressionAst const &cast_lhs,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> PropagatedMethodCall {
      //

      // Get the function conversion of the method (free
      // function with self argument).
      auto [transformed_lhs, transformed_fn_call] = ConvertMethodToFnForm(
        fn_owner_type, fn_name, cast_lhs, fn_call, *sm, meta);

      // Determine the overload based off the function
      // (uniform system).
      auto [overload, is_closure] = [&] {
        const auto _meta_guard = meta::MetaGuard(meta);
        meta->PostfixExpressionLhs = transformed_lhs.get();
        return DetermineOverload(*transformed_fn_call, sm, meta);
      }();

      // The call takes the argument group with the "self" injection, and keeps the function form's left-hand side
      // (and the callable it built, which the overload may point into).
      fn_call.FnArgGroup = std::move(transformed_fn_call->FnArgGroup);
      transformed_lhs->Stage7_AnalyseSemantics(sm, meta);
      return PropagatedMethodCall{
        std::move(overload), is_closure, std::move(transformed_lhs), transformed_fn_call->TakeClosureDummyProto()};
    }

    /**
     * The generics of the "sup" block a candidate was written in, bound by matching the block's pattern against the
     * type the call was made on: "sup [U] Vec[U]" called on a "Vec[S32]" binds "U", however the class spells its own
     * parameter, and a blanket block ("sup [T] T", attached unsubstituted) binds "T" to the whole receiver. A method
     * inherited from a class the receiver extends ("sup [T] Iterable[T]") is matched through the receiver's class
     * super types. A trailing pack ("sup [First, ..Rest] Tup[First, Rest]") is bound by the match itself, to the tuple of
     * the receiver's arguments from its position on.
     * @param fn_scope The scope the candidate was declared in; the "sup" block it was written in is found above it.
     * @param receiver The type the call was made on (or the forwarding type it was reached through), or @c nullptr .
     * @param sm The scope manager, positioned at the call site.
     * @return The bindings, to be merged below what the call wrote.
     */
    auto ReceiverGns(
      Scope const *fn_scope,
      TypeAst const *receiver,
      ScopeManager const &sm)
      -> Vec<Unique<GenericArgumentAst>> {
      if (receiver == nullptr or fn_scope == nullptr) { return {}; }

      // A method is lowered into its own "sup $F ext FunXxx" block, inside the one it was written in; the generics are
      // on that.
      const auto sup_pattern_of = [](Scope const *scope) -> Shared<TypeAst> {
        if (scope == nullptr or scope->AstNode == nullptr) { return nullptr; }
        const auto is_sup = scope->AstNode->To<SupPrototypeFunctionsAst>() != nullptr
          or scope->AstNode->To<SupPrototypeExtensionAst>() != nullptr;
        return is_sup ? AstName(scope->AstNode) : nullptr;
      };
      auto const *sup_scope = fn_scope;
      auto pattern = sup_pattern_of(sup_scope);
      while (pattern != nullptr and pattern->IsCompilerGeneratedType()) {
        sup_scope = sup_scope->Parent;
        pattern = sup_pattern_of(sup_scope);
      }
      if (pattern == nullptr) { return {}; }
      const auto receiver_type = receiver->WithConvention(nullptr);

      // The receiver itself, then the classes it extends, each named where it resolves.
      auto candidates = Vec<Pair<Shared<TypeAst>, Scope const*>>{{receiver_type, sm.CurrentScope}};
      if (auto const *const receiver_sym = sm.CurrentScope->FindTypeSymbol(receiver_type.get());
        receiver_sym != nullptr and receiver_sym->LinkedScope != nullptr) {
        candidates.AppendRange(type_members::SuperClsNames(receiver_sym->LinkedScope->GetSupScopes()));
      }
      // An instantiated block binds some of its generics already, and only a class that agrees with them is its
      // receiver: "D" extending "Base[Str]" and "Base[Bool]" reaches the "Base[Bool]" block only through "Base[Bool]",
      // though "Base[Str]" matches its pattern "Base[B]" first. What it leaves unbound ("Rest" of a tuple block, whose
      // pack is not bound on attach) still comes from the receiver.
      const auto agrees = [&](type_compare::GenericInferenceMap const &inferred, Scope const &candidate_scope) {
        for (auto const &[name, val] : inferred) {
          auto const *const typed = val->To<TypeAst>();
          auto *const own = sup_scope->FindTypeSymbol(name.get(), true);
          if (typed == nullptr or own == nullptr or own->Kind != TypeKind::GnTypeArg or own->AsBound() == own) {
            continue;
          }
          if (not type_compare::TypeEq(
            TypeRef::Of(*typed, candidate_scope), TypeRef::Of(*own, *sup_scope), candidate_scope, *sup_scope)) {
            return false;
          }
        }
        return true;
      };

      auto bound = Vec<Unique<GenericArgumentAst>>();
      for (auto const &[candidate, candidate_scope] : candidates) {
        auto inferred = type_compare::GenericInferenceMap();
        if (type_compare::RelaxedTypeEq(*candidate, *pattern, *candidate_scope, *sup_scope, inferred, false, false)
          and agrees(inferred, *candidate_scope)) {
          bound = std::move(GenericArgumentGroupAst::FromMap(inferred)->Args);
          break;
        }
      }
      return bound;
    }

    /**
     * What "Self" stands for at this call, from the call itself: the type the call was made on ("Type::method(..)", which
     * "a.method(..)" is rewritten to), unless the candidate was reached by forwarding, when it belongs to the type
     * forwarded to. The one place a call decides it; the pin below and "PassedOverload::SelfType" both read it. Taken as
     * the type the receiver resolves to, not as written, and unmarked: a written node carries its own access marks,
     * and a type built from it would answer access checks as if written at the call.
     * @return The receiver's type, or @c nullptr when the call has none that "Self" stands for.
     */
    auto CallSelf(
      FnOverload const &candidate,
      ScopeManager const *sm,
      meta::CompilerMetaData const *meta)
      -> Shared<TypeAst> {
      auto const *const receiver = ReceiverTypeAtCallSite(meta);
      if (receiver == nullptr or candidate.FwdType != nullptr) { return nullptr; }
      auto const *const receiver_sym = sm->CurrentScope->FindTypeSymbol(receiver);
      auto self_type = AstCloneShared(receiver_sym != nullptr ? receiver_sym->FqName() : receiver->WithConvention(nullptr));
      for (auto *part : self_type->TypeParts()) { part->ClearSourceWritten(); }
      return self_type;
    }

    /**
     * Pin "Self" to the call's receiver for this candidate, if the receiver is what "Self" stands for here. A method
     * declared on a class and called on an implementer of it needs "Self" bound to the implementer, not left as the
     * declaring class - that is what lets "Writer::write_all" call "self.write()" and reach the implementer's "write"
     * rather than the abstract one. Inference always sees the pin; only a concrete receiver is part of the solution,
     * and so of the instantiation it keys.
     * @param fn_proto The candidate prototype.
     * @param fn_scope The scope the candidate was declared in.
     * @param call_self What "Self" stands for from the call ("CallSelf"), or @c nullptr .
     * @param solver The candidate's generic solver, given the pin.
     * @param sm The scope manager, positioned at the call site.
     * @param meta Associated metadata.
     */
    auto PinSelfToReceiver(
      FunctionPrototypeAst const &fn_proto,
      Scope const *fn_scope,
      Shared<TypeAst> const &call_self,
      generic_inference::GenericSolver &solver,
      ScopeManager const *sm,
      meta::CompilerMetaData const *meta)
      -> void {
      using type_compare::TypeEq;

      const auto declared_self = fn_scope->FindEnclosingSelfType(*meta);
      if (call_self == nullptr or declared_self == nullptr) { return; }
      const auto declared_self_sym = fn_scope->FindTypeSymbol(declared_self.get());
      const auto declared_on_abstract = declared_self_sym != nullptr and declared_self_sym->LinkedScope != nullptr
        and not type_members::GetUnimplementedAbstractMethods(*declared_self_sym->LinkedScope).IsEmpty();
      if (not SignatureNamesSelf(fn_proto) and not declared_on_abstract) { return; }

      const auto receiver_sym = sm->CurrentScope->FindHeadSymbol(*call_self);
      if (receiver_sym == nullptr or TypeEq(*declared_self, *call_self, *fn_scope, *sm->CurrentScope)) { return; }
      const auto concrete = not call_self->IsSelfType() and not receiver_sym->IsGn();
      auto self_arg = Vec<Unique<GenericArgumentAst>>();
      self_arg.EmplaceBack(GenericArgumentAst::NewType(generate::common_types::SelfType(0), AstCloneShared(call_self)));
      solver.Give(std::move(self_arg), concrete);
    }

    /**
     * Decide which of the call's arguments each parameter takes, without touching them. A keyword argument goes to
     * the parameter it names, and each positional one to the next parameter not yet named; the variadic parameter takes
     * all that remain, as a tuple. A variadic parameter given nothing takes an empty pack, so "fun f[..Ts](..a: Ts)"
     * called with nothing binds "Ts" to "()" rather than leaving it uninferred. An optional parameter left out gets its
     * default, translated out of the callee's terms with what is bound so far ("alloc: A = A()" would otherwise arrive
     * as an "A()" the caller has no "A" for) and analysed where it was written. The slots are in parameter order, which
     * LLVM matches by position.
     * @param fn_args The call's arguments, read only.
     * @param fn_params The candidate's parameters.
     * @param bindings What the candidate's generics are bound to so far, which its defaults are read with; none when
     * nothing is known, and a default is its declaration's own.
     * @param callee_scope Where the candidate is declared, which a default is analysed in.
     * @param sm The scope manager, positioned at the call site.
     * @param meta Associated metadata.
     * @return A slot per parameter that is given anything.
     */
    auto BindSlots(
      FunctionCallArgumentGroupAst const &fn_args,
      FunctionParameterGroupAst const &fn_params,
      std::optional<scopes::GenericSubst> const &bindings,
      Scope *callee_scope,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> Vec<ParamSlot> {
      const auto a_names = fn_args.GetKeywordArgs()
        | genex::views::transform([](auto *x) { return x->Name.get(); })
        | genex::to<Vec>();
      const auto all_p_names = fn_params.GetAllParams()
        | genex::views::transform([](auto *x) { return x->ExtractName(); })
        | genex::to<Vec>();
      EnforceFnArgNamesKnown(all_p_names, a_names, fn_params, *sm);

      // Each argument, keyed by the parameter it is for. Positional arguments come before keyword ones, so the "i"th
      // argument is the "i"th positional one until they run out.
      auto p_names = all_p_names
        | genex::views::not_in(a_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();
      const auto make_slot = [](Shared<IdentifierAst> name, Vec<std::size_t> args, const bool is_pack) {
        auto slot = ParamSlot{};
        slot.Name = std::move(name);
        slot.Args = std::move(args);
        slot.IsPack = is_pack;
        return slot;
      };
      auto taken = Vec<ParamSlot>();
      for (auto const &[i, arg] : fn_args.Args | genex::views::ptr | genex::views::enumerate) {
        if (auto const *const kw_arg = arg->To<FunctionCallArgumentKeywordAst>(); kw_arg != nullptr) {
          taken.EmplaceBack(make_slot(kw_arg->Name, {i}, false));
          continue;
        }
        auto name = MakeShared<IdentifierAst>(arg->PosStart(), Str(p_names.Front()->Val));
        p_names |= genex::actions::pop_front();
        if (p_names.IsEmpty() and fn_params.GetVariadicParam() != nullptr) {
          auto rest = Vec<std::size_t>();
          for (auto j = i; j < fn_args.Args.Len(); ++j) { rest.EmplaceBack(j); }
          taken.EmplaceBack(make_slot(std::move(name), std::move(rest), true));
          break;
        }
        taken.EmplaceBack(make_slot(std::move(name), {i}, false));
      }

      auto slots = Vec<ParamSlot>();
      for (auto const *param : fn_params.GetAllParams()) {
        const auto param_name = param->ExtractName();
        const auto given = genex::find_if(taken, [&](auto const &slot) {
          return slot.Name != nullptr and slot.Name->Val == param_name->Val;
        });
        if (given != taken.end()) {
          slots.EmplaceBack(std::move(*given));
          continue;
        }
        if (param->To<FunctionParameterVariadicAst>() != nullptr) {
          slots.EmplaceBack(make_slot(param_name, {}, true));
          continue;
        }

        const auto optional_param = param->To<FunctionParameterOptionalAst>();
        if (optional_param == nullptr or optional_param->DefaultVal == nullptr) { continue; }
        auto const &written = optional_param->Source.OriginalDefaultVal != nullptr
          ? optional_param->Source.OriginalDefaultVal
          : optional_param->DefaultVal;
        // Read where it is declared, whose own bindings (an instantiated "sup" block's) are in view there.
        auto default_val = not bindings.has_value()
          ? AstClone(optional_param->DefaultVal)
          : AstClone(written->ReadExpr(ExprSubst::In(*callee_scope, *bindings)));
        if (bindings.has_value()) {
          const auto outer_scope = sm->CurrentScope;
          if (callee_scope != nullptr) { sm->CurrentScope = callee_scope; }
          default_val->Stage7_AnalyseSemantics(sm, meta);
          sm->CurrentScope = outer_scope;
        }
        slots.EmplaceBack(make_slot(param_name, {}, false));
        slots.Back().Default = std::move(default_val);
      }

      // Type each slot as what it is given: the argument (with its convention, or the receiver's type for an injected
      // "self"), the tuple of a pack's values (conventions dropped, as a tuple holds no borrow), or the default.
      for (auto &slot : slots) {
        if (slot.Default != nullptr) { slot.Type = slot.Default->InferType(sm, meta); }
        else if (slot.IsPack) {
          const auto vals = slot.Args
            | genex::views::transform([&](auto i) { return fn_args.Args[i]->Val.get(); })
            | genex::to<Vec>();
          slot.Type = TupleLiteralAst::TypeOfElements(vals, vals.IsEmpty() ? 0uz : vals[0]->PosStart(), sm, meta);
        }
        else { slot.Type = fn_args.Args[slot.Args[0]]->InferType(sm, meta); }
      }
      return slots;
    }

    /**
     * Where a slot's value is written, for an error: its argument (convention included), the first of a pack's, its
     * default, or for an empty pack the parameter's name.
     */
    auto SlotSite(
      ParamSlot const &slot,
      FunctionCallArgumentGroupAst const &fn_args)
      -> Ast const& {
      if (slot.Default != nullptr) { return *slot.Default; }
      return slot.Args.IsEmpty() ? static_cast<Ast const&>(*slot.Name) : *fn_args.Args[slot.Args[0]];
    }

    /**
     * The value a slot is given, for a source span: its argument, the first of a pack's, or its default.
     */
    auto SlotValue(
      ParamSlot const &slot,
      FunctionCallArgumentGroupAst const &fn_args)
      -> ExpressionAst const* {
      if (slot.Default != nullptr) { return slot.Default.get(); }
      return slot.Args.IsEmpty() ? nullptr : fn_args.Args[slot.Args[0]]->Val.get();
    }

    /**
     * Solve the candidate's generics from what each parameter (except "self") is given, each pointed at where it came
     * from so a conflict names it rather than "<generated code>", against the parameter's declared type. The solver
     * holds the solution to the generic constraints.
     * @return The solved generic arguments.
     */
    auto InferAllGns(
      FunctionPrototypeAst const &fn_proto,
      FunctionCallArgumentGroupAst const &fn_args,
      Vec<ParamSlot> const &slots,
      generic_inference::GenericSolver &solver,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> Unique<GenericArgumentGroupAst> {
      for (auto const *param : fn_proto.FnParamGroup->GetNonSelfParams()) {
        const auto name = param->ExtractName();
        const auto slot = genex::find_if(slots, [&](auto const &s) { return s.Name->Val == name->Val; });
        if (slot == slots.end()) { continue; }

        // A default infers only what it closes ("alloc: A = GlobalAlloc()"). One still naming the callee's own generics
        // ("y: Vec[T] = Vec[T]::new()") can only restate them, so it is made after solving instead ("CheckCandidate").
        if (slot->Default != nullptr and not type_predicates::IsTypeConcrete(*slot->Type, *sm->CurrentScope)) {
          continue;
        }
        const auto val = SlotValue(*slot, fn_args);
        solver.Unify(name, val != nullptr ? slot->Type->WithSourceSpanAt(*val) : slot->Type, param->Type);
      }

      const auto variadic_param = fn_proto.FnParamGroup->GetVariadicParam();
      solver.Solve(
        *meta->PostfixExpressionLhs->InferType(sm, meta),
        variadic_param != nullptr ? variadic_param->ExtractName() : nullptr);
      auto gn_args = GenericArgumentGroupAst::NewEmpty();
      gn_args->Args = solver.TakeArgs();
      return gn_args;
    }

    /**
     * Drop the parameters whose type resolved to @c Void . A generic parameter bound to @c Void is not a parameter at
     * all - "fun f[T](x: T)" instantiated with "T=Void" takes nothing - so it is removed before the argument lists are
     * lined up. Walked backwards because the container shrinks as it goes.
     * @param fn_proto The candidate prototype, whose parameter list is edited in place.
     * @param fn_scope The scope the candidate was declared in, which its types resolve in.
     */
    auto StripVoidParams(
      FunctionPrototypeAst const &fn_proto,
      Scope const *fn_scope)
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
     * argument may name a parameter that does not exist, and no required parameter may go unnamed. The first is
     * checked again after "BindSlots" because a parameter whose type substitutes to "Void" has been stripped since.
     * @param func_params The candidate's parameter group, used as the error's context when it has no parameters.
     * @param func_param_names Every parameter's name.
     * @param func_param_names_req The required parameters' names.
     * @param func_arg_names The call's keyword argument names.
     * @param fn_call The call being checked, used as the error's context for a missing argument.
     * @param sm The scope manager, positioned at the call site.
     */
    auto CheckArgNamesAgainstParams(
      FunctionParameterGroupAst const &func_params,
      Vec<Shared<IdentifierAst>> const &func_param_names,
      Vec<Shared<IdentifierAst>> const &func_param_names_req,
      Vec<IdentifierAst*> const &func_arg_names,
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      ScopeManager const *sm)
      -> void {
      using errors::SppArgumentMissingError;

      EnforceFnArgNamesKnown(func_param_names, func_arg_names, func_params, *sm);

      const auto missing_params = func_param_names_req
        | genex::views::not_in(func_arg_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();
      RaiseIf<SppArgumentMissingError>(
        not missing_params.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*missing_params[0], "parameter", fn_call, "argument"));
    }

    /**
     * Check what each parameter is given against its type, recording the coercions the chosen overload's arguments
     * will need rather than applying them: "self" taking the declared convention, and a value handed over through its
     * forwarding type. The call's arguments are only read.
     * @param fn_call The call being checked, the context of a missing-argument error.
     * @param fn_proto The candidate, instantiated; its "Void" parameters are stripped.
     * @param fn_scope The scope the candidate resolves its types in.
     * @param fn_args The call's arguments.
     * @param slots What each parameter is given, updated with the coercions.
     * @param self_type What "Self" stands for at this call ("PassedOverload::SelfType").
     * @param sm The scope manager, positioned at the call site.
     * @param meta Associated metadata.
     */
    auto ValidateArgsMatchParams(
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      FunctionPrototypeAst const &fn_proto,
      Scope const *fn_scope,
      FunctionCallArgumentGroupAst const &fn_args,
      Vec<ParamSlot> &slots,
      TypeAst const *self_type,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      //
      using errors::SppTypeMismatchError;
      using type_compare::TypeEq;
      using type_compare::RelaxedTypeEq;

      // A value of type "Void" is no value, and a parameter of that type no parameter, so both drop out of the match.
      StripVoidParams(fn_proto, fn_scope);
      const auto slot_is_void = [&](ParamSlot const &slot) {
        const auto ref = slot.Default == nullptr and not slot.IsPack
          ? fn_args.Args[slot.Args[0]]->Val->InferTypeRef(sm, meta)
          : TypeRef::Of(*slot.Type->WithoutConvention(), *sm->CurrentScope);
        return type_predicates::IsTypeVoid(ref, *sm->CurrentScope);
      };
      auto given = Vec<ParamSlot*>();
      for (auto &slot : slots) {
        if (not slot_is_void(slot)) { given.EmplaceBack(&slot); }
      }

      // "self" is required too: a runtime call injects it, but "Type::method()" has to pass it, or there is no receiver.
      const auto func_params = fn_proto.FnParamGroup.get();
      const auto func_param_names = func_params->Params
        | genex::views::transform([](auto &&x) { return x->ExtractName(); })
        | genex::to<Vec>();
      auto func_param_names_req = func_params->GetRequiredParams()
        | genex::views::transform([](auto &&x) { return x->ExtractName(); })
        | genex::to<Vec>();
      if (const auto self_param = func_params->GetSelfParam(); self_param != nullptr) {
        func_param_names_req.Insert(func_param_names_req.begin(), self_param->ExtractName());
      }
      const auto func_arg_names = given
        | genex::views::transform([](auto const *slot) { return slot->Name.get(); })
        | genex::to<Vec>();
      CheckArgNamesAgainstParams(*func_params, func_param_names, func_param_names_req, func_arg_names, fn_call, sm);

      // The slots are in parameter order, and every parameter left has one, so the two line up.
      for (auto [slot, param] : genex::views::zip(given, func_params->GetAllParams())) {

        // A "self" parameter carries no type check of its own: the receiver is what chose this overload to begin with,
        // so there is nothing left to compare it against. It only needs the convention the prototype declares.
        if (const auto self_param = param->To<FunctionParameterSelfAst>(); self_param != nullptr) {
          // Except in the static form, "Type::method(receiver)", where the receiver is written as an ordinary argument
          // and nothing has checked it yet: it has to be the type the method was reached through, or one extending
          // it. A "Self" or generic receiver names nothing concrete to hold it to.
          auto const &arg = *fn_args.Args[slot->Args[0]];
          if (const auto receiver = ReceiverTypeAtCallSite(meta);
            receiver != nullptr and arg.GetSelfType() == nullptr and not receiver->IsSelfType()) {
            const auto receiver_sym = sm->CurrentScope->FindTypeSymbol(receiver);
            const auto a_type = arg.Val->InferType(sm, meta)->WithoutConvention();
            const auto a_sym = sm->CurrentScope->FindTypeSymbol(a_type.get());
            if (receiver_sym != nullptr and a_sym != nullptr
              and not receiver_sym->IsGn() and not a_sym->IsGn()) {
              const auto receiver_tmpl = TypeRef::OfKind(*receiver_sym, *sm->CurrentScope).Template();
              auto matches = TypeRef::OfKind(*a_sym, *sm->CurrentScope).Template() == receiver_tmpl;
              if (not matches and a_sym->LinkedScope != nullptr) {
                matches = genex::any_of(a_sym->LinkedScope->GetSupScopes(), [&](auto const *sup_scope) {
                  return sup_scope->LinkedTypeSymbol != nullptr
                    and TypeRef::OfKind(*sup_scope).Template() == receiver_tmpl;
                });
              }
              RaiseIf<SppTypeMismatchError>(
                not matches, {fn_scope, sm->CurrentScope}, ERR_ARGS(*param, *receiver, arg, *a_type));
            }
          }
          slot->IsSelf = true;
          slot->SelfConv = AstClone(self_param->Conv);
          continue;
        }

        auto p_type = fn_scope->FindTypeSymbol(param->Type.get())->FqName()->WithConvention(
          AstClone(param->Type->GetConvention()));
        // "Self" is what the call decided it stands for ("PassedOverload::SelfType"): the owning type when the method
        // was reached by forwarding, since "&Str" calling "StrView::eq" must be handed a "StrView", not a "Str" read
        // through "StrView"'s "{ptr, length}" shape (the "Str == Str" bug).
        if (self_type != nullptr and type_predicates::DoesTypeNameSelf(*p_type)) {
          p_type = self_type::SubstituteSelf(*p_type, self_type);
        }
        auto const &a_type = slot->Type;

        if (const auto variadic_param = param->To<FunctionParameterVariadicAst>(); variadic_param != nullptr) {
          const auto variadic_gn_param = fn_proto.GetNonGnImpl()->GnParamGroup->GetVariadicParam();
          const auto orig_name = dynamic_shared_cast<TypeIdentifierAst>(variadic_param->Source.OriginalType);
          const auto is_variadic_generic_type = variadic_gn_param != nullptr
            and orig_name != nullptr
            and *orig_name == *dynamic_shared_cast<TypeIdentifierAst>(variadic_gn_param->Name);

          // "..a: T" takes a tuple of "T"s, one per element the call passed; "..a: Ts" (a variadic generic) is that
          // tuple itself.
          if (not is_variadic_generic_type) {
            auto ts = Vec(packs::TypePackElements(*a_type).Len(), p_type);
            p_type = generate::common_types::TupleType(param->PosStart(), std::move(ts));
            p_type->Stage7_AnalyseSemantics(sm, meta);
          }
        }

        // A value handed over through its forwarding type is the forwarded-to value, so the argument becomes that
        // call once this overload is chosen.
        const auto forwards = [&] {
          return type_compare::TypeFwdEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope)
            and marker_sups::CanForward(TypeRef::Of(*a_type, *sm->CurrentScope), *sm->CurrentScope);
        };

        // An argument satisfies its parameter either outright, or by binding a generic the call is free to choose.
        if (not type_compare::ConventionEq(*p_type, *a_type)
          or not type_compare::Assignable(*p_type, *a_type, *fn_scope, *sm->CurrentScope)) {
          // Operands go argument-first here, which is the order "RelaxedTypeEq" infers the parameter's generics from
          // the argument in rather than the other way round; its internal convention check is inverted to match. A
          // parameter whose generic is already fixed by a scope enclosing the caller is not free to choose, so it has
          // to match exactly; and a relaxed match that only held by binding such a generic is not a match either.
          auto inferred = type_compare::GenericInferenceMap();
          const auto relaxed_matched = type_compare::ConventionEq(*p_type, *a_type)
            and RelaxedTypeEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope, inferred);

          // Forwarding is the last resort, tried only once the argument has failed to match any other way - so it
          // never displaces a relaxed match that would have bound the parameter's generics correctly.
          if (not relaxed_matched and forwards()) {
            slot->IsForwarded = true;
            continue;
          }
          RaiseIf<SppTypeMismatchError>(
            not relaxed_matched, {fn_scope, sm->CurrentScope}, ERR_ARGS(*param, *p_type, SlotSite(*slot, fn_args), *a_type));
          continue;
        }

        // The types matched, and may still have matched by forwarding ("&Vec[T]" satisfying a "&View[T]" parameter).
        // This is the argument-position counterpart of a method being called on the value its receiver forwards to.
        // An argument the relaxed match accepted never reaches here; "Self" picking the owner when the receiver only
        // forwards to it (above) is what keeps "Str == Str" from reading a "&Str" as a "StrView". Trying the forward
        // before the relaxed match instead segfaults a third of the suite.
        slot->IsForwarded = forwards();
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
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      using type_compare::TypeEq;

      if (meta->ReturnTypeOverloadResolverType == nullptr) { return; }

      auto return_matches = Vec<PassedOverload>();
      for (auto &&matched : pass_overloads) {
        auto ret = AstCloneShared(matched.Proto->ReturnType);
        auto tm = ScopeManager(sm->GlobalScope, const_cast<Scope*>(matched.FnScope));
        ret = self_type::SubstituteSelf(*ret, matched.SelfType.get());
        ret = self_type::SubstituteSelf(*ret, matched.FnScope->FindEnclosingSelfType(*meta).get(), &tm, meta);

        const auto ret_ref = TypeRef::Of(*ret, *matched.FnScope);
        if (type_compare::Assignable(
          ret_ref, *meta->ReturnTypeOverloadResolverType, *matched.FnScope, *sm->CurrentScope)) {
          return_matches.EmplaceBack(std::move(matched));
        }
      }

      if (return_matches.Len() == 1) { pass_overloads = std::move(return_matches); }
    }

    auto ManageMatchedOverloads(
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      Vec<PassedOverload> const &pass_overloads,
      Vec<FailedOverload> const &fail_overloads,
      FunctionCallArgumentGroupAst const &arg_group,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      // How the call's arguments read, for either error below.
      using namespace std::string_literals;
      const auto arg_usage_signature = [&arg_group, sm, meta] {
        return arg_group.Args
          | genex::views::transform([sm, meta](auto const &x) {
            return x->GetSelfType() == nullptr ? x->InferType(sm, meta)->ToString() : "Self";
          })
          | genex::views::intersperse(", "_str)
          | genex::views::join
          | genex::to<Str>();
      };

      // If there are no pass overloads, raise an error.
      if (pass_overloads.IsEmpty()) {
        auto failed_signatures_and_errors = "\n" + (fail_overloads
          | genex::views::transform([](auto const &f) {
            return "    - "s + f.Proto->PrintSignature("") + ": "s + f.Reason;
          })
          | genex::views::intersperse("\n"_str)
          | genex::views::join
          | genex::to<Str>());

        auto sub_errors = fail_overloads
          | genex::views::transform([](auto const &f) { return f.Error; })
          | genex::to<Vec>();

        Raise<errors::SppFunctionCallNoValidSignaturesError>(
          {sm->CurrentScope}, ERR_ARGS(fn_call, failed_signatures_and_errors, arg_usage_signature()),
          std::move(sub_errors));
      }

      // If there are multiple pass overloads, raise an error.
      if (pass_overloads.Len() > 1) {
        auto signatures = "\n" + (pass_overloads
          | genex::views::transform([](auto const &x) { return "    - "s + x.Proto->PrintSignature(""); })
          | genex::views::intersperse("\n"_str)
          | genex::views::join
          | genex::to<Str>());

        Raise<errors::SppFunctionCallOverloadAmbiguousError>(
          {sm->CurrentScope}, ERR_ARGS(fn_call, signatures, arg_usage_signature()));
      }
    }


    /**
     * Check one candidate against the call, without editing the call: bind its generics (what the call wrote, then
     * what the receiver and the enclosing "sup" block pin), decide what each parameter is given, infer the rest of the
     * generics from that, instantiate the candidate for them, and check each parameter against what it is given. A
     * candidate that does not fit raises; one that does comes back with its slots, which only the chosen candidate
     * applies to the call ("ElaborateCall").
     * @param fn_call The call.
     * @param candidate The candidate, whose "sup" block's generics are taken.
     * @param fn_owner_type The type the call was made on, or @c nullptr .
     * @param sm The scope manager, positioned at the call site.
     * @param meta Associated metadata.
     * @return The candidate as it matched.
     */
    auto CheckCandidate(
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      FnOverload &candidate,
      TypeAst const *fn_owner_type,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> PassedOverload {
      using errors::SppFunctionCallTooManyArgumentsError;
      auto *fn_proto = candidate.Proto;
      auto const *fn_scope = candidate.FnScope;
      auto const &fn_args = *fn_call.FnArgGroup;
      const auto is_variadic_fn = fn_proto->FnParamGroup->GetVariadicParam() != nullptr;

      // Cannot check for "too few" arguments here because of potential "T=Void" + "x: T" removal. Check if there are
      // too many arguments (for a non-variadic function).
      RaiseIf<SppFunctionCallTooManyArgumentsError>(
        fn_args.Args.Len() > fn_proto->FnParamGroup->Params.Len() and not is_variadic_fn, {fn_scope},
        ERR_ARGS(*fn_proto, fn_proto->FnParamGroup->Params.Len(), fn_call, fn_args.Args.Len()));

      // Every generic argument this call is resolved with, in layers. Precedence runs highest first: what the call
      // wrote, then what the owner type (or the forwarding type it was reached through) pins, then what the enclosing
      // "sup" block declares, then a pinned "Self" - the first binding offered for a name wins.
      auto solver = generic_inference::GenericSolver(*fn_proto->GnParamGroup, *fn_scope, *sm, *meta);
      solver.Give(std::move(generic_inference::NamedGnArgs(
        *fn_call.GnArgGroup, *fn_proto->GnParamGroup, *fn_proto->Name, *sm, *meta)->Args));
      solver.Give(ReceiverGns(fn_scope, candidate.FwdType != nullptr ? candidate.FwdType.get() : fn_owner_type, *sm));
      solver.Give(std::move(candidate.SupGns->Args));
      const auto call_self = CallSelf(candidate, sm, meta);
      PinSelfToReceiver(*fn_proto, fn_scope, call_self, solver, sm, meta);

      // A default is read with what is known so far, "Self" as the call pinned it.
      const auto known = solver.GetKnownArgs();
      auto default_bindings = std::optional<scopes::GenericSubst>();
      if (not known.IsEmpty()) {
        default_bindings = type_resolution::BindArgs(*fn_proto->GnParamGroup, known, *sm->CurrentScope);
        if (const auto pin = genex::find_if(known, [](auto const *arg) {
          return arg->TypeName() != nullptr and arg->IsTypeArg() and arg->TypeName()->IsSelfType();
        }); pin != known.end()) { type_resolution::BindSelf(*default_bindings, *(*pin)->TypeVal, *sm->CurrentScope); }
      }
      auto slots = BindSlots(
        fn_args, *fn_proto->FnParamGroup, default_bindings, const_cast<Scope*>(fn_scope), sm, meta);
      auto gn_args = InferAllGns(*fn_proto, fn_args, slots, solver, sm, meta);

      // What the variadic parameter actually receives: the tuple of the trailing arguments.
      auto variadic_pack_type = Shared<TypeAst>(nullptr);
      if (is_variadic_fn) {
        const auto variadic_name = fn_proto->FnParamGroup->GetVariadicParam()->ExtractName();
        const auto pack = genex::find_if(slots, [&](auto const &slot) { return slot.Name->Val == variadic_name->Val; });
        if (pack != slots.end()) {
          variadic_pack_type = AstCloneShared(pack->Type);
          for (auto *part : variadic_pack_type->TypeParts()) { part->ClearSourceWritten(); }
        }
      }

      auto *const template_proto = fn_proto;
      std::tie(fn_proto, fn_scope) = monomorphization::PotentiallyGenerateGnSubstitutedPrototype(
        fn_proto, fn_scope, *gn_args, variadic_pack_type, sm, meta);
      candidate.Proto = fn_proto;
      candidate.FnScope = fn_scope;

      // A default the call takes is made again once everything is solved. The one "BindSlots" made (and inference
      // read) had only what was bound before inference; the instantiation's own parameter carries it translated with
      // the whole solution ("Vec[T]::new()" as the default of an "A" inferred from a later argument).
      if (fn_proto != template_proto) {
        for (auto &slot : slots) {
          if (slot.Default == nullptr) { continue; }
          const auto param = genex::find_if(fn_proto->FnParamGroup->Params, [&](auto const &p) {
            return p->ExtractName()->Val == slot.Name->Val;
          });
          if (param == fn_proto->FnParamGroup->Params.end()) { continue; }
          auto const *const optional = (*param)->To<FunctionParameterOptionalAst>();
          if (optional == nullptr or optional->DefaultVal == nullptr) { continue; }
          auto default_val = AstClone(optional->DefaultVal);
          const auto outer_scope = sm->CurrentScope;
          sm->CurrentScope = const_cast<Scope*>(fn_scope);
          default_val->Stage7_AnalyseSemantics(sm, meta);
          sm->CurrentScope = outer_scope;
          slot.Default = std::move(default_val);
          slot.Type = slot.Default->InferType(sm, meta);
        }
      }

      // What "Self" stands for at this call ("PassedOverload::SelfType"): what the call decided ("CallSelf"), else the
      // owner, read from the instantiation's own scope, where the owner's generics are bound - unmarked, as it is not
      // written at this call.
      auto self_type = call_self;
      if (self_type == nullptr and fn_scope != nullptr) {
        if (self_type = fn_scope->FindEnclosingSelfType(*meta); self_type != nullptr) {
          self_type = AstCloneShared(self_type);
          for (auto *part : self_type->TypeParts()) { part->ClearSourceWritten(); }
        }
      }

      ValidateArgsMatchParams(fn_call, *fn_proto, fn_scope, fn_args, slots, self_type.get(), sm, meta);
      return PassedOverload{fn_scope, fn_proto, std::move(slots), std::move(self_type)};
    }
  }
}

#define SPP_FN_RES_ERR_WRAPPER(error_type, message)                          \
  catch (error_type const &e) {                                              \
    fail_overloads.EmplaceBack(FailedOverload{fn_proto, e.what(), message}); \
    while (meta->Depth() > original_meta_depth) { meta->Restore(); }         \
  }

auto spp::analyse::utils::overload_resolution::DetermineOverload(
  PostfixExpressionOperatorFunctionCallAst &fn_call,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Pair<PassedOverload, bool> {
  //
  using scopes::ScopeManager;
  using errors::SppFunctionCallTooManyArgumentsError;

  const auto [lhs, _callee_holder] = CalleeExpr(*sm, meta);

  // Extract metadata about the target function's overloads
  // such as the function's owner and scope.
  const auto [fn_owner_type, fn_owner_scope, fn_name] = GetFnOwnerTypeAndFnName(
    *lhs, *sm, meta);

  const auto is_postfix = meta->PostfixExpressionLhs->To<PostfixExpressionAst>();
  const auto is_runtime = is_postfix
    ? is_postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()
    : nullptr;

  // If we are resolving a method, then use the free function
  // equivalent. For example, convert "1.add(2)" to
  // "S32::add(1, 2)".
  if (is_runtime != nullptr and fn_owner_type != nullptr and fn_name != nullptr) {
    auto propagated = PropagateMethodToFn(
      fn_call, *fn_owner_type, *fn_name, *is_postfix, sm, meta);
    fn_call.SetTransformedLhs(std::move(propagated.TransformedLhs));
    if (propagated.ClosureProto != nullptr) { fn_call.SetClosureDummyProto(std::move(propagated.ClosureProto)); }
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
    auto *const &fn_proto = candidate.Proto;
    try {
      pass_overloads.EmplaceBack(CheckCandidate(fn_call, candidate, fn_owner_type.get(), sm, meta));
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

  // Only now is it known which overload the call makes, so only
  // now is its instantiation required; the other candidates' were
  // built just to check the call against.
  pass_overloads[0].Proto->RequireGnSubstitution();

  // Store the closure if it was generated as part of the
  // overload resolution.
  if (candidates.ClosureProto) {
    fn_call.SetClosureDummyProto(std::move(candidates.ClosureProto));
  }
  return {std::move(pass_overloads[0]), candidates.IsClosure};
}

auto spp::analyse::utils::overload_resolution::ElaborateCall(
  FunctionCallArgumentGroupAst &args,
  PassedOverload &overload,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // The values are moved out of the written arguments into one keyword argument per slot. Nothing analysed is
  // dropped: the scopes an argument's analysis created (a loop's, a closure's) name its value as their "AstNode".
  auto written = std::move(args.Args);
  args.Args.Clear();
  for (auto &slot : overload.Slots) {
    auto conv = Unique<ConventionAst>(nullptr);
    auto self_type = Shared<TypeAst>(nullptr);
    auto val = Unique<ExpressionAst>(nullptr);
    if (slot.Default != nullptr) { val = std::move(slot.Default); }
    else if (slot.IsPack) {
      auto elems = slot.Args
        | genex::views::transform([&](auto i) { return std::move(written[i]->Val); })
        | genex::to<Vec>();
      val = MakeUnique<TupleLiteralAst>(nullptr, std::move(elems), nullptr);
    }
    else {
      auto &arg = *written[slot.Args[0]];
      conv = std::move(arg.Conv);
      self_type = arg.GetSelfType();
      val = std::move(arg.Val);
    }

    // "self" takes the prototype's convention, whose tokens are placed on the receiver the call was made through.
    if (slot.IsSelf) {
      conv = std::move(slot.SelfConv);
      if (auto *const m = conv != nullptr ? conv->To<ConventionMutAst>() : nullptr) {
        m->TokBorrow->PatchPos(val->PosStart());
        m->TokMut->PatchPos(val->PosStart());
      }
      else if (auto *const r = conv != nullptr ? conv->To<ConventionRefAst>() : nullptr) {
        r->TokBorrow->PatchPos(val->PosStart());
      }
    }

    // A value handed over through its forwarding type becomes the forwarding call.
    if (slot.IsForwarded) {
      val = marker_sups::BuildFwdCall(std::move(val), TypeRef::Of(*slot.Type, *sm->CurrentScope), sm, meta);
    }

    auto kw_arg = MakeUnique<FunctionCallArgumentKeywordAst>(slot.Name, nullptr, std::move(conv), std::move(val));
    kw_arg->SetSelfType(std::move(self_type));
    args.Args.EmplaceBack(std::move(kw_arg));
  }
}

auto spp::analyse::utils::overload_resolution::ExpectedArgTypes(
  PostfixExpressionOperatorFunctionCallAst const &fn_call,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Vec<Shared<TypeRef>> {
  //
  using type_compare::TypeEq;
  auto out = Vec<Shared<TypeRef>>(fn_call.FnArgGroup->Args.Len());

  // An expansion ("..t") has not been unpacked yet, so argument
  // positions do not yet line up with parameters.
  const auto expands = genex::any_of(fn_call.FnArgGroup->Args, [](auto const &arg) {
    auto const *const pos = arg->template To<FunctionCallArgumentPositionalAst>();
    return pos != nullptr and pos->TokUnpack != nullptr;
  });
  if (expands) { return out; }

  // The candidates are looked up the way resolution will, but any
  // error doing so is resolution's to report, once the arguments
  // are analysed: expectations are only a hint towards it.
  try {
    const auto [lhs, _callee_holder] = CalleeExpr(*sm, meta);
    const auto [fn_owner_type, fn_owner_scope, fn_name] = GetFnOwnerTypeAndFnName(*lhs, *sm, meta);
    auto const *const postfix = lhs->To<PostfixExpressionAst>();
    const auto is_method = fn_owner_type != nullptr and fn_name != nullptr and postfix != nullptr
      and postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr;
    const auto candidates = RetrieveAllOverloads(fn_name.get(), fn_owner_scope, sm, meta);
    if (candidates.Overloads.IsEmpty()) { return out; }

    for (auto const &[i, arg] : fn_call.FnArgGroup->Args | genex::views::ptr | genex::views::enumerate) {
      auto const *const kw_arg = arg->To<FunctionCallArgumentKeywordAst>();
      auto expected = Shared<TypeRef>(nullptr);
      auto expected_scope = static_cast<Scope const*>(nullptr);
      auto agreed = true;

      for (auto const &candidate : candidates.Overloads) {
        // A method's receiver is not among the written arguments.
        const auto params = is_method
          ? candidate.Proto->FnParamGroup->GetNonSelfParams()
          : candidate.Proto->FnParamGroup->GetAllParams();
        auto const *param = static_cast<FunctionParameterAst const*>(nullptr);
        if (kw_arg == nullptr) { param = i < params.Len() ? params[i] : nullptr; }
        else { for (auto *p : params) { if (*p->ExtractName() == *kw_arg->Name) { param = p; } } }

        // No parameter here, a pack, a generic or a "Self": what the
        // argument must be is not known without the argument.
        if (param == nullptr or param->To<FunctionParameterVariadicAst>() != nullptr
          or type_predicates::DoesTypeNameSelf(*param->Type)
          or not type_predicates::IsTypeConcrete(*param->Type, *candidate.FnScope)) {
          agreed = false;
          break;
        }

        const auto ref = MakeShared<TypeRef>(TypeRef::Of(*param->Type, *candidate.FnScope));
        if (expected != nullptr and not TypeEq(*expected, *ref, *expected_scope, *candidate.FnScope)) {
          agreed = false;
          break;
        }
        expected = ref;
        expected_scope = candidate.FnScope;
      }
      if (agreed) { out[i] = expected; }
    }
  }
  catch (errors::SemanticError const &) {
    return Vec<Shared<TypeRef>>(fn_call.FnArgGroup->Args.Len());
  }
  return out;
}

auto spp::analyse::utils::overload_resolution::GetAllFnScopes(
  IdentifierAst const &target_fn_name, Scope const *target_scope,
  ScopeManager &sm, meta::CompilerMetaData *meta) -> Vec<FnOverload> {
  // If the name is empty (non-symbolic call) then return
  // "no scopes". If the target scope is nullptr, then the
  // functions are being superimposed over a generic type.
  if (target_fn_name.Val.empty() or target_scope == nullptr) { return {}; }

  // Get the function-type name from the function: "func()"
  // => "$Func".
  const auto mapped_name = target_fn_name.ToFnIdentifier();
  auto overload_scopes = Vec<FnOverload>();

  auto is_valid_ext_scope = [mapped_name=mapped_name.get()](auto const *scope) {
    const auto ext = AstAs<SupPrototypeExtensionAst>(scope->AstNode);
    if (ext == nullptr) { return false; }
    const auto ext_name = dynamic_shared_cast<TypeIdentifierAst>(ext->Name);
    return ext_name != nullptr and ext_name->Name == mapped_name->Val;
  };

  // Check for namespaced (module-level) functions (they
  // will have no inheritable generics as they are free
  // functions, not inside a "sup" block).
  if (target_scope->LinkedNamespaceSymbol != nullptr) {
    for (auto *ancestor_scope : target_scope->GetAncestors()) {
      for (auto const *sup_scope : ancestor_scope->Children
           | genex::views::ptr
           | genex::views::filter(is_valid_ext_scope)) {
        overload_scopes.EmplaceBack(FnOverload{
          .FnScope = sup_scope,
          .Proto = fn_values::FnBlockOf(*sup_scope).second,
          .SupGns = GenericArgumentGroupAst::NewEmpty(),
          .FwdType = nullptr
        });
      }
    }
  }

  // Functions belonging to a type will have inheritance
  // generics from "sup [...] Type { ... }"
  else {
    // If a class scope was provided, get all the sup scopes
    // attached to it, otherwise use the specific sup scope
    // exclusively.
    // From the super scopes, check each one for the structure
    // "sup $Func ext FunXXX { ... }" super-imposition.
    // Todo: use the "is_valid_ext_scope"?
    const auto collect_from = [&](Scope const *const sup_scope) -> void {
      for (auto *sup_ast : AstBody(sup_scope->AstNode)
           | genex::views::cast_dynamic<SupPrototypeExtensionAst*>()) {
        if (sup_ast->Name->ToUnchecked<TypeIdentifierAst>()->Name == mapped_name->Val) {
          overload_scopes.EmplaceBack(FnOverload{
            .FnScope = sup_scope,
            .Proto = AstBody(sup_ast)[0]->To<FunctionPrototypeAst>(),
            .SupGns = MakeUnique<GenericArgumentGroupAst>(nullptr, sup_scope->GetGns(), nullptr),
            .FwdType = nullptr
          });
        }
      }
    };

    // A class scope contributes every super scope attached to it, read from its own list rather than copied out of it;
    // anything else contributes itself, exclusively. Each "Scope*" converts to "Scope const*" as it is read, which a
    // vector of one cannot do for a vector of the other.
    if (AstAs<ClassPrototypeAst>(target_scope->AstNode) != nullptr) {
      for (auto const *sup_scope : target_scope->GetSupScopes()) { collect_from(sup_scope); }
    }
    else {
      collect_from(target_scope);
    }

    PruneOverriddenOverloads(overload_scopes, target_scope, sm, meta);
    NarrowToOwningBlock(overload_scopes, is_valid_ext_scope);
  }

  // Next, get scopes from "forwarding types" (ie FwdRef
  // and FwdMut return types). Forwarding is a fallback, so
  // the receiver's own methods shadow the forwarded-to ones;
  // without this, a type whose forwarded-to type also forwards
  // (eg "NonNull[Str]" -> "&Str" -> "&StrView") sees both
  // "fwd_ref" overloads and the forwarding call is ambiguous.
  if (target_scope->LinkedTypeSymbol != nullptr and meta->CurrentStage >= meta::CompilerStage::kAnalyseSemantics and
    overload_scopes.IsEmpty()) {
    // Either forwarding type carries the methods. "FwdMut" was bound and then never read, so a type superimposing only
    // "FwdMut" got no forwarded methods here, while "BuildFwdCall" would happily build a "fwd_mut()" call for it in
    // argument position - the two forwarding paths disagreed.
    const auto [fwd_ref_sup, fwd_mut_sup] = marker_sups::FindFwdSups(
      TypeRef::Of(*target_scope->LinkedTypeSymbol, *sm.CurrentScope), *sm.CurrentScope);
    const auto fwd_ref = marker_sups::FwdTargetOf(fwd_ref_sup);
    const auto fwd_mut = marker_sups::FwdTargetOf(fwd_mut_sup);
    if (fwd_ref.Symbol != nullptr or fwd_mut.Symbol != nullptr) {
      auto *const inner_sym = fwd_ref.Symbol != nullptr ? fwd_ref.Symbol : fwd_mut.Symbol;
      auto inner_scopes = inner_sym != nullptr
        ? GetAllFnScopes(target_fn_name, inner_sym->LinkedScope, sm, meta)
        : Vec<FnOverload>{};
      for (auto &i : inner_scopes) {
        i.FwdType = AstCloneShared(inner_sym->FqName());
      }
      overload_scopes.AppendRange(std::move(inner_scopes));
    }
  }

  // Remove duplicate overloads that are the same pointer (ie
  // exact protos). Todo: Likely a bandaid over an issue.
  auto unique_overloads = Vec<FnOverload>();
  for (auto &&info : overload_scopes) {
    const auto already_seen = genex::any_of(unique_overloads, [&info](auto const &seen) {
      return seen.Proto == info.Proto and seen.FnScope == info.FnScope;
    });
    if (not already_seen) { unique_overloads.EmplaceBack(std::move(info)); }
  }

  // Return all the found function scopes.
  return unique_overloads;
}
