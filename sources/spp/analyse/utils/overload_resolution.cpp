module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.overload_resolution;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.fn_signatures;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.packs;
import spp.analyse.utils.self_type;
import spp.analyse.utils.sup_blocks;
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
import spp.asts.postfix_expression_operator_ast;
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
import spp.utils.ice;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import sys;

namespace spp::analyse::utils::overload_resolution {
  namespace {
    /// [CHECKED]
    /// Reject the first keyword argument naming no parameter,
    /// reported against the first parameter (or the argument
    /// itself, when there are none).
    auto EnforceFnArgNamesKnown(
      Vec<Shared<IdentifierAst>> const &param_names, Vec<IdentifierAst*> const &arg_names,
      FunctionParameterGroupAst const &params) -> void {
      // Loop through each arg name, and raise an error if
      // there is not a parameter that can match the argument's
      // name.
      for (auto arg_name : arg_names) {
        if (genex::any_of(param_names, [arg_name](auto const &p) { return *p == *arg_name; })) { continue; }
        auto const &param_ctx = params.Params.empty()
          ? static_cast<Ast const&>(*arg_name)
          : static_cast<Ast const&>(*params.Params[0]);

        Raise<errors::SppArgumentNameInvalidError>(
          ERR_ARGS(param_ctx, StrView("fn param"), *arg_name, StrView("fn arg")));
      }
    }

    /// A scope's methods by name, with what they were read off: the
    /// generation last known current, and the super scopes they were
    /// gathered from ("type_members::GetUnimplementedAbstractMethods"
    /// keeps its answer the same way).
    struct MethodTable {
      std::uint64_t Generation = 0;
      Vec<Scope*> Sups;
      Map<std::uint32_t, Vec<MethodEntry>> ByName;
    };

    /// Stable: a table is held while its methods are compared,
    /// which can read (and add) another scope's. The method
    /// tables are a mapping of a scope to a struct of TODO.
    auto _MethodTables() -> StableMap<Scope const*, MethodTable>& {
      static auto tables = StableMap<Scope const*, MethodTable>();
      return tables;
    }

    /// [CHECKED]
    /// When a type's sup scopes are explored, they each in turn
    /// contain a number of sup asts (which are pre-processed
    /// functions). Find the matching child scope who maps to
    /// the ast, and return that scope. Todo: Rename?
    auto BlockOf(
      Scope const &sup_scope, SupPrototypeExtensionAst const &sup_ast) -> Scope const* {
      // Iterate the child scopes looking for one whose attached
      // node is the requested "sup_ast" node
      const auto block = genex::find_if(sup_scope.Children, [&](auto const &child) {
        return child->AstNode == &sup_ast;
      });
      return block != sup_scope.Children.end() ? block->get() : nullptr;
    }

    /// [CHECKED]
    /// Given the different ways a function call might arrive,
    /// we have different behaviour to get its type, scope and
    /// name. For example, "x.m(a)" needs to run inference for
    /// the type, where-as "T::m(a, x)" already has it.
    auto GetFnOwnerTypeAndFnName(
      ExpressionAst const &lhs, ScopeManager &sm, meta::CompilerMetaData *meta)
      -> Tup<Shared<TypeAst>, Scope const*, Shared<IdentifierAst>> {
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
        fn_owner_scope = sm.CurrentScope->FindNsScope(postfix_lhs->Lhs.get());
        fn_name = static_field->Name;

        // Add a name check here because we need to get
        // the type off of it before it is even analysed.
        const auto fn_owner_sym = fn_owner_scope->FindVarSymbol(fn_name.get());
        if (fn_owner_sym == nullptr) {
          RaiseMissingIdentifierAndClosestOptions(*fn_name, fn_owner_scope->GetAllVarSymbols(), {});
        }
        fn_owner_type = fn_owner_sym->Type;
      }

      // Direct access into a non-namespaced function:
      // "function()":
      else if (lhs_as_ident != nullptr) {
        // It must be a variable symbol, not a namespace symbol.
        const auto sym = sm.CurrentScope->FindVarSymbol(lhs_as_ident);
        if (sym == nullptr) {
          RaiseMissingIdentifierAndClosestOptions(
            *lhs_as_ident, sm.CurrentScope->GetAllVarSymbols(), {});
        }
        fn_name = AstCloneShared(lhs_as_ident);

        // A name declared inside the function (a function-type
        // variable) is a value being called, not a module function
        // spelled the same. A function found nearer than the module
        // (a method of the type being extended, through its "sup"
        // scope) is not a bare call's target: the module's function
        // of that name is, if it has one, looked up where it is
        // declared (through a "use" import, its target's scope).
        const auto mod_scope = sm.CurrentScope->GetParentModule();
        auto mod_sym = mod_scope != nullptr ? mod_scope->FindVarSymbol(lhs_as_ident) : nullptr;
        if (mod_sym != nullptr and mod_sym->IsCompTime() and (sym == mod_sym or sym->Kind == VariableKind::FnMock)) {
          while (mod_sym->AliasSymbol != nullptr) { mod_sym = mod_sym->AliasSymbol.get(); }
          fn_owner_scope = mod_sym->ScopeDefinedIn;
        }
        else {
          fn_owner_scope = sym->ScopeDefinedIn == mod_scope ? mod_scope : nullptr;
        }
      }

      // Non-callable AST.
      else {
        fn_owner_type = nullptr;
        fn_name = nullptr;
        fn_owner_scope = nullptr;
      }

      // A value of a named function's own type is that function,
      // so calling it is calling the function - resolved like
      // the name, overloads and generics included, not through
      // a pointer. Todo: Tidy up.
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

    /// [CHECKED]
    /// Given a written expression being called, for example
    /// "a.b.c()", the "written" part is "a.b.c". Relevant
    /// information is extracted to form a "call site" object.
    auto CallSiteOf(
      ExpressionAst &written, ScopeManager &sm,
      meta::CompilerMetaData *meta) -> CallSite {
      // Build an empty call site and attach the written
      // expression into it.
      auto site = CallSite();
      site.Written = &written;

      // Get the owning type (nullptr for free functions), the
      // function scope, and its name, based on the written
      // expression.
      tie(site.OwnerType, site.OwnerScope, site.FnName) = GetFnOwnerTypeAndFnName(written, sm, meta);

      // If we are dealing with a "." or "::" in the left-side
      // of the method, keep a "."'s member access and receiver
      // (a method call), and set the "CalledOn" to a "::"'s
      // explicit type, otherwise the "."'s inferred type.
      if (const auto postfix = written.To<PostfixExpressionAst>(); postfix != nullptr) {
        const auto member_access = postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
        if (member_access != nullptr and site.OwnerType != nullptr and site.FnName != nullptr) {
          site.MemberAccess = member_access;
          site.Receiver = postfix->Lhs.get();
        }
        site.CalledOn = site.IsMethod() ? site.OwnerType.get() : postfix->Lhs->To<TypeAst>();
      }
      return site;
    }

    /// [CHECKED]
    /// Give a method call its receiver as the "self" argument.
    /// For "x.m(a)", it is checked as "T::m(self=x, other=a)",
    /// ie "x" is the receiver. A method reached via forwarding
    /// is invoked on the forwarded to value. So for the call
    /// "vector.index_ref(0_uz)", it extends with forwarding to
    /// becomes "vector.fwd_ref().index_ref(0_uz)" which then
    /// translates into the call "View::index_ref(vector, 0_uz)"
    auto InjectReceiver(
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      CallSite const &site) -> void {
      // If a mapped forwarding method has been created, then
      // this is what becomes the "receiver", as the method is
      // on that target type (the View for example). Only
      // "IsMethod" calls reach here, so the site has both.
      const auto fwd_receiver = site.MemberAccess->GetFwdReceiver();
      const auto self_expr = fwd_receiver != nullptr
        ? fwd_receiver
        : site.Receiver;

      // Wrap the "self" expression into a positional argument
      // and inject it into the 0th argument slot. This argument
      // group now has an "injected receiver".
      auto self_arg = MakeUnique<FunctionCallArgumentPositionalAst>(
        nullptr, nullptr, AstClone(self_expr));
      auto &args = fn_call.FnArgGroup->Args;
      args.insert(args.begin(), std::move(self_arg));
    }

    /// Each indirect callee's prototype, by its function type's
    /// identity: built from the type alone, so shared by every call.
    auto _IndirectPrototypes() -> Map<TypeId, Unique<FunctionPrototypeAst>>& {
      static auto protos = Map<TypeId, Unique<FunctionPrototypeAst>>();
      return protos;
    }

    auto CreateIndirectPrototype(
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
     * The prototype a value of function type "fn_type" is called indirectly as: its parameters and return type, made
     * once per type. Null for a type that carries neither, or names nothing here.
     */
    auto IndirectPrototypeOf(
      TypeAst const &fn_type,
      Scope const &scope)
      -> FunctionPrototypeAst* {
      const auto id = TypeRef::Of(fn_type, scope).Id;
      if (id == nullptr) { return nullptr; }
      auto &protos = _IndirectPrototypes();
      if (const auto hit = protos.find(id); hit != protos.end()) { return hit->second.get(); }
      auto proto = CreateIndirectPrototype(fn_type);
      auto *const out = proto.get();
      if (out != nullptr) { protos.emplace(id, std::move(proto)); }
      return out;
    }

    /// Based on the function call at a provided call site,
    /// retrieve all the overload candidates that could match
    /// the call (by symbol name, no argument checks yet).
    auto RetrieveAllOverloads(
      CallSite const &site, ScopeManager *sm,
      meta::CompilerMetaData *meta) -> OverloadCandidates {
      // For named functions (ie non-closures), get all the
      // function overload implementation scopes. Empty list
      // for a value called through its function type.
      auto all_overloads = site.FnName != nullptr and site.OwnerScope != nullptr
        ? GetAllFnScopes(*site.FnName, site.OwnerScope, *sm, meta)
        : Vec<FnOverload>{};

      // If the overload list is not empty, ie the target is
      // a named function, return the overload candidates.
      if (not all_overloads.empty()) {
        return OverloadCandidates{
          .IndirectFnType = nullptr,
          .Overloads = std::move(all_overloads)
        };
      }

      // If there are no scopes, the callee is a value called
      // through its function type (a closure, a "Fun*" field
      // or parameter, a constrained generic).
      auto indirect = fn_values::IndirectFnTypeOf(*site.Written, *sm, meta);
      if (const auto proto = indirect != nullptr ? IndirectPrototypeOf(*indirect, *sm->CurrentScope) : nullptr;
        proto != nullptr) {
        all_overloads.emplace_back(overload_resolution::FnOverload{
          .FnScope = sm->CurrentScope,
          .Proto = proto,
          .ReachedThrough = nullptr,
          .FwdType = nullptr
        });
        return OverloadCandidates{std::move(indirect), std::move(all_overloads)};
      }

      // Otherwise, there are no scopes (handled in caller).
      return OverloadCandidates{nullptr, {}};
    }

    /**
     * What "scope" and the scopes around it bind their generics to, by identity: each parameter bound there (an
     * instantiated "sup" block's, its class's) to its argument. Nearest first, so an inner binding of a parameter wins.
     */
    auto BoundGnsOf(
      Scope const &scope)
      -> scopes::GenericSubst {
      auto out = scopes::GenericSubst();
      for (auto const *s : scope.GetAncestors()) {
        for (auto const *t : s->GetAllTypeSymbols(true)) {
          const auto pid = t->BindsParamId;
          if (t->Kind != TypeKind::GnTypeArg or pid == 0) { continue; }
          if (genex::any_of(out.TypeParams, [pid](auto const &x) { return x.first == pid; })) { continue; }
          if (const auto id = s->TypeIdOfSymbol(*t); id != nullptr and id != scopes::ParamTypeId(pid)) {
            out.TypeParams.emplace_back(pid, id);
          }
        }
        for (auto const *v : s->GetAllVarSymbols(true)) {
          const auto pid = v->BindsParamId;
          if (v->Kind != VariableKind::GnCompArg or pid == 0) { continue; }
          if (genex::any_of(out.CompParams, [pid](auto const &x) { return x.first == pid; })) { continue; }
          if (const auto id = s->CompIdOfSymbol(*v); id != nullptr and id != scopes::ParamCompId(pid)) {
            out.CompParams.emplace_back(pid, id);
          }
        }
      }
      return out;
    }

    /// The generics of a "sup" block that a candidate was written
    /// in, bound by matching the block's pattern against the type
    /// the call was made on. For example, "sup [U] Vec[U]" called
    /// on a "Vec[S32]" binds "U=S32".
    auto ReceiverGns(
      Scope const *fn_scope, TypeAst const *receiver,
      ScopeManager const &sm) -> GenericSubst {
      if (receiver == nullptr or fn_scope == nullptr) { return {}; }

      // A method is lowered into its own "sup $f ext FunXxx"
      // block, inside the one it was written in; the generics
      // are on that.
      const auto sup_scope = sup_blocks::EnclosingSupBlock(*fn_scope);
      if (sup_scope == nullptr) { return {}; }
      const auto pattern = AstName(sup_scope->AstNode);
      const auto receiver_type = receiver->WithConvention(nullptr);

      // An instantiated block binds some of its generics already, and only a class that agrees with them is its
      // receiver: "D" extending "Base[Str]" and "Base[Bool]" reaches the "Base[Bool]" block only through "Base[Bool]",
      // though "Base[Str]" matches its pattern "Base[B]" first. By identity, the pattern keys a bound generic as its
      // value, so the match itself asks for agreement, and binds only what the block leaves unbound ("Rest" of a tuple
      // block, whose pack is not bound on attach). What the block binds is passed on as it binds it ("BoundGnsOf", the
      // layer given after this one).
      const auto pattern_id = sup_scope->TypeIdOf(*pattern);
      if (pattern_id == nullptr) { return {}; }
      auto const *const ext = sup_scope->AstNode->To<SupPrototypeExtensionAst>();
      auto const *const fns = sup_scope->AstNode->To<SupPrototypeFunctionsAst>();
      auto const *const params = ext != nullptr
        ? ext->GnParamGroup.get()
        : fns != nullptr
        ? fns->GnParamGroup.get()
        : nullptr;
      if (params == nullptr) { Ice(*sup_scope->AstNode, *sup_scope, "sup block has no generic parameter group"); }
      auto bound = scopes::GenericSubst();
      const auto matches = [&](const scopes::TypeId candidate_id, Scope const &candidate_scope) {
        auto inferred = scopes::GenericSubst();
        if (candidate_id == nullptr or not scopes::UnifyTypeIds(
          candidate_id, pattern_id, candidate_scope, *sup_scope, inferred, false, false)) { return false; }
        bound = scopes::BindingsFor(inferred, *params);
        return true;
      };

      // The receiver itself, then the classes it extends, each keyed where it resolves, and only once the ones before
      // it have not matched.
      if (matches(sm.CurrentScope->TypeIdOf(*receiver_type), *sm.CurrentScope)) { return bound; }
      if (auto const *const receiver_sym = sm.CurrentScope->FindTypeSymbol(receiver_type.get());
        receiver_sym != nullptr and receiver_sym->LinkedScope != nullptr) {
        for (auto const *const cls_scope : type_members::SuperClsScopes(receiver_sym->LinkedScope->GetSupScopes())) {
          if (matches(cls_scope->TypeIdOfSymbol(*cls_scope->LinkedTypeSymbol), *cls_scope)) { break; }
        }
      }
      return bound;
    }

    /// [CHECKED]
    /// Decide what "Self" means at this function call. Either
    /// the type of "x" in "x.m(a)", "Type" in "Type::m(a)", or
    /// for forwarding calls, the forward type - "View[T]" for
    /// "Vec[T]" etc.
    auto CallSelf(
      FnOverload const &candidate, CallSite const &site,
      ScopeManager const *sm) -> Shared<TypeAst> {
      // The default choice is the type that the function was
      // called on, or the forward type for a forwarded candidate
      // (whose method runs on the forwarded to value). Get its
      // type symbol.
      const auto receiver = candidate.FwdType != nullptr
        ? candidate.FwdType.get()
        : site.CalledOn;
      if (receiver == nullptr) { return nullptr; }

      // Get the symbol, and return the fully qualified name
      // on it. Todo: Will this ever be nullptr?
      const auto receiver_sym = sm->CurrentScope->FindTypeSymbol(receiver);
      auto self_type = AstCloneShared(receiver_sym != nullptr
        ? receiver_sym->FqName()
        : receiver->WithConvention(nullptr));
      self_type->ClearPartsSourceWritten();
      return self_type;
    }

    /// [CHECKED]
    /// After using "CallSelf" to determine the true value for
    /// "Self" for a function call, accounting for forwarding too,
    /// pass this value for Self into the generic solver. This
    /// allows "Self" to be bound to the implementer when called
    /// as a base class method, for example, "Clone::clone"
    /// returning "Self", called on "Str", needs to return "Self".
    /// Also applies to parameter types, and when nested as generic
    /// args, so goes into the solver.
    auto PinSelfToReceiver(
      Scope const *fn_scope,
      Shared<TypeAst> const &call_self, generic_inference::GenericSolver &solver,
      ScopeManager const *sm, meta::CompilerMetaData const *meta) -> void {
      using type_compare::TypeEq;

      // Get the "Self" type from the scope that the function
      // is defined in.
      const auto declared_self = fn_scope->FindEnclosingSelfType(*meta);
      if (call_self == nullptr or declared_self == nullptr) { return; }

      // Any function called on a type other than the one that
      // declared it is pinned, whatever it reads "Self" through:
      // a "self" parameter, its signature, or only its body (an
      // associated function calling "Self::f()"). Pinned, it is
      // compiled per receiver, so what it calls through "Self"
      // is the receiver's (an override, an implementation of an
      // abstract method) and not the declaring class's.
      const auto receiver_sym = sm->CurrentScope->FindHeadSymbol(*call_self);
      if (receiver_sym == nullptr or TypeEq(
        TypeRef::Of(*declared_self, *fn_scope), TypeRef::Of(*call_self, *sm->CurrentScope), *fn_scope,
        *sm->CurrentScope)) { return; }
      const auto concrete = not call_self->IsSelfType() and not receiver_sym->IsGn();
      solver.PinSelf(AstCloneShared(call_self), concrete);
    }

    /**
     * A parameter's default, analysed where its callee is declared, whose own bindings (an instantiated "sup" block's)
     * are in view there: on a manager standing in that scope, so the call site's walk is left as it was, on an error
     * too.
     */
    auto AnalysedDefault(
      Unique<ExpressionAst> val,
      Scope const &scope,
      ScopeManager const &sm,
      meta::CompilerMetaData *meta)
      -> Unique<ExpressionAst> {
      auto tm = sm.NewManagerAt(const_cast<Scope*>(&scope));
      val->Stage7_AnalyseSemantics(&tm, meta);
      return val;
    }

    /// Create a list of "param slots", which bind each argument
    /// to the correct corresponding parameter, by keyword,
    /// then positionals left over. A variadic parameter swallows
    /// all the left over arguments should it exist, as a tuple.
    /// No args in the variadic leaves "..Ts" as (), not
    /// uninferred. Optional parameters with no arguments get the
    /// default value, but generically substituted ("A()" =>
    /// "GlobalAlloc()" etc.). The slots are in parameter order,
    /// which LLVM matches by position.
    auto BindSlots(
      FunctionCallArgumentGroupAst const &fn_args, FunctionParameterGroupAst const &fn_params,
      Opt<GenericSubst> const &bindings, Scope const &callee_scope,
      ScopeManager *sm, meta::CompilerMetaData *meta) -> Vec<ParamSlot> {
      // Get the argument names from the provided keyword arguments,
      // and then extract the parameter names too. Check there are
      // no invalidly named keyword arguments.
      const auto a_names = fn_args.GetKeywordArgs()
        | genex::views::transform([](auto *x) { return x->Name.get(); })
        | genex::to<Vec>();

      const auto all_p_names = fn_params.GetAllParams()
        | genex::views::transform([](auto *x) { return x->ExtractName(); })
        | genex::to<Vec>();

      EnforceFnArgNamesKnown(all_p_names, a_names, fn_params);

      // Each argument, keyed by the parameter it is for: a keyword
      // one by the parameter it names, and the positional ones by
      // the parameters no keyword names, in order.
      const auto params = fn_params.GetAllParams();
      const auto param_named = [&params](IdentifierAst const &name) -> FunctionParameterAst const* {
        const auto p = genex::find_if(params, [&](auto x) { return *x->ExtractName() == name; });
        return p != params.end() ? *p : nullptr;
      };

      // The positionally matching parameters are the parameter
      // that no argument names.
      auto positional = params
        | genex::views::filter([&](auto p) {
          return genex::none_of(a_names, [&](auto n) { return *p->ExtractName() == *n; });
        })
        | genex::to<Vec>();

      const auto make_slot = [](
        FunctionParameterAst const *param, Shared<IdentifierAst> name,
        Vec<std::size_t> args, const bool is_pack) {
        auto slot = ParamSlot{};
        slot.Param = param;
        slot.Name = std::move(name);
        slot.Args = std::move(args);
        slot.IsPack = is_pack;
        return slot;
      };

      auto taken = Vec<ParamSlot>();
      for (auto const &[i, arg] : fn_args.Args | genex::views::ptr | genex::views::enumerate) {

        // Take the keyword arguments by their name and matching
        // parameter. Index stored for LLVM.
        if (auto const *const kw_arg = arg->To<FunctionCallArgumentKeywordAst>(); kw_arg != nullptr) {
          taken.emplace_back(make_slot(param_named(*kw_arg->Name), kw_arg->Name, {i}, false));
          continue;
        }

        // Otherwise, take the first available parameter for a
        // positional argument, and pop it from the list.
        const auto param = positional.front();
        auto name = MakeShared<IdentifierAst>(arg->PosStart(), Str(param->ExtractName()->Val));
        positional |= genex::actions::pop_front();

        // If this is the last parameter left to match positional
        // arguments, but it is a variadic parameter, swallow the
        // rest of the arguments in a tuple.
        if (positional.empty() and fn_params.GetVariadicParam() != nullptr) {
          auto rest = Vec<std::size_t>();
          for (auto j = i; j < fn_args.Args.size(); ++j) { rest.emplace_back(j); }
          taken.emplace_back(make_slot(param, std::move(name), std::move(rest), true));
          break;
        }

        // Otherwise just store the positionally mapped argument.
        taken.emplace_back(make_slot(param, std::move(name), {i}, false));
      }

      auto slots = Vec<ParamSlot>();
      for (const auto param : params) {
        const auto param_name = param->ExtractName();
        const auto given = genex::find_if(taken, [&](auto const &slot) { return slot.Param == param; });
        if (given != taken.end()) {
          slots.emplace_back(std::move(*given));
          continue;
        }
        if (param->To<FunctionParameterVariadicAst>() != nullptr) {
          slots.emplace_back(make_slot(param, param_name, {}, true));
          continue;
        }

        const auto optional_param = param->To<FunctionParameterOptionalAst>();
        if (optional_param == nullptr or optional_param->DefaultVal == nullptr) { continue; }
        auto const &written = optional_param->Source.OriginalDefaultVal != nullptr
          ? optional_param->Source.OriginalDefaultVal
          : optional_param->DefaultVal;
        // Read where it is declared ("AnalysedDefault").
        auto default_val = not bindings.has_value()
          ? AstClone(optional_param->DefaultVal)
          : AnalysedDefault(
            AstClone(written->ReadExpr(ExprSubst::In(callee_scope, *bindings))), callee_scope, *sm, meta);
        slots.emplace_back(make_slot(param, param_name, {}, false));
        slots.back().Default = std::move(default_val);
      }

      // Type each slot as what it is given: the argument (with its convention, or the receiver's type for an injected
      // "self"), the tuple of a pack's values (conventions dropped, as a tuple holds no borrow), or the default.
      for (auto &slot : slots) {
        if (slot.Default != nullptr) { slot.Type = slot.Default->InferType(sm, meta); }
        else if (slot.IsPack) {
          const auto vals = slot.Args
            | genex::views::transform([&](auto i) { return fn_args.Args[i]->Val.get(); })
            | genex::to<Vec>();
          slot.Type = TupleLiteralAst::TypeOfElements(vals, vals.empty() ? 0uz : vals[0]->PosStart(), sm, meta);
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
      return slot.Args.empty() ? static_cast<Ast const&>(*slot.Name) : *fn_args.Args[slot.Args[0]];
    }

    /**
     * Solve the candidate's generics from what each parameter (except "self") is given, each pointed at where it came
     * from so a conflict names it rather than "<generated code>", against the parameter's declared type. The solver
     * holds the solution to the generic constraints, read from it as its identity or as arguments.
     */
    auto InferAllGns(
      FunctionPrototypeAst const &fn_proto,
      FunctionCallArgumentGroupAst const &fn_args,
      Vec<ParamSlot> const &slots,
      CallSite const &site,
      generic_inference::GenericSolver &solver,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      for (auto const *param : fn_proto.FnParamGroup->GetNonSelfParams()) {
        const auto name = param->ExtractName();
        const auto slot = genex::find_if(slots, [param](auto const &s) { return s.Param == param; });
        if (slot == slots.end()) { continue; }

        // A default infers only what it closes ("alloc: A = GlobalAlloc()"). One still naming the callee's own generics
        // ("y: Vec[T] = Vec[T]::new()") can only restate them, so it is made after solving instead ("CheckCandidate").
        if (slot->Default != nullptr and not type_predicates::IsTypeConcrete(*slot->Type, *sm->CurrentScope)) {
          continue;
        }
        solver.Unify(name, slot->Type->WithSourceSpanAt(SlotSite(*slot, fn_args)), param->Type);
      }

      const auto variadic_param = fn_proto.FnParamGroup->GetVariadicParam();
      solver.Solve(
        *site.Written->InferType(sm, meta),
        variadic_param != nullptr ? variadic_param->ExtractName() : nullptr);
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
     */
    auto CheckArgNamesAgainstParams(
      FunctionParameterGroupAst const &func_params,
      Vec<Shared<IdentifierAst>> const &func_param_names,
      Vec<Shared<IdentifierAst>> const &func_param_names_req,
      Vec<IdentifierAst*> const &func_arg_names,
      PostfixExpressionOperatorFunctionCallAst const &fn_call)
      -> void {
      using errors::SppArgumentMissingError;

      EnforceFnArgNamesKnown(func_param_names, func_arg_names, func_params);

      const auto missing_params = func_param_names_req
        | genex::views::not_in(func_arg_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();
      RaiseIf<SppArgumentMissingError>(
        not missing_params.empty(),
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
     * @param self_id What "Self" stands for at this call ("PassedOverload::SelfId").
     * @param template_scope The scope the candidate's template declares its parameters in.
     * @param inferred The candidate's generics inferred from the arguments ("GenericSolver::InferredParams").
     * @param site The call.
     * @param sm The scope manager, positioned at the call site.
     * @param meta Associated metadata.
     */
    auto ValidateArgsMatchParams(
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      FunctionPrototypeAst const &fn_proto,
      Scope const *fn_scope,
      FunctionCallArgumentGroupAst const &fn_args,
      Vec<ParamSlot> &slots,
      TypeId self_id,
      Scope const &template_scope,
      Vec<std::uint64_t> const &inferred,
      CallSite const &site,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      //
      using errors::SppTypeMismatchError;

      // A value of type "Void" is no value, and a parameter of that type no parameter, so both drop out of the match:
      // the parameters are read through that view, never edited ("RemoveVoidParams" is the prototype's own to do).
      const auto func_params = fn_proto.FnParamGroup.get();
      const auto params = func_params->GetAllParams()
        | genex::views::filter([fn_scope](auto const *p) {
          return not FunctionParameterGroupAst::IsVoidParam(*p, *fn_scope);
        })
        | genex::to<Vec>();
      const auto slot_is_void = [&](ParamSlot const &slot) {
        const auto ref = slot.Default == nullptr and not slot.IsPack
          ? fn_args.Args[slot.Args[0]]->Val->InferTypeRef(sm, meta)
          : TypeRef::Of(*slot.Type->WithoutConvention(), *sm->CurrentScope);
        return type_predicates::IsTypeVoid(ref, *sm->CurrentScope);
      };
      auto given = Vec<ParamSlot*>();
      for (auto &slot : slots) {
        if (not slot_is_void(slot)) { given.emplace_back(&slot); }
      }

      // "self" is required too: a runtime call injects it, but "Type::method()" has to pass it, or there is no receiver.
      const auto func_param_names = params
        | genex::views::transform([](auto const *x) { return x->ExtractName(); })
        | genex::to<Vec>();
      const auto func_param_names_req = params
        | genex::views::filter([](auto const *x) {
          return x->template To<FunctionParameterRequiredAst>() != nullptr
            or x->template To<FunctionParameterSelfAst>() != nullptr;
        })
        | genex::views::transform([](auto const *x) { return x->ExtractName(); })
        | genex::to<Vec>();
      const auto func_arg_names = given
        | genex::views::transform([](auto const *slot) { return slot->Name.get(); })
        | genex::to<Vec>();
      CheckArgNamesAgainstParams(
        *func_params, func_param_names, func_param_names_req, func_arg_names, fn_call);

      // An argument whose template parameter names a generic inferred from the arguments chose that parameter's type,
      // rather than being held to it ("MatchRank::Bound").
      const auto chose_its_type = [&](ParamSlot const &slot) {
        if (slot.Param == nullptr or inferred.empty()) { return false; }
        const auto declared = template_scope.TypeIdOf(*slot.Param->Type);
        if (declared == nullptr) { return false; }
        auto const &named = scopes::ParamsNamedBy(declared);
        const auto is_inferred = [&](auto pid) { return genex::contains(inferred, pid); };
        return genex::any_of(named.TypeParams, is_inferred) or genex::any_of(named.CompParams, is_inferred);
      };

      // The slots are in parameter order, and every parameter left has one, so the two line up.
      for (auto [slot, param] : genex::views::zip(given, params)) {
        // A "self" parameter carries no type check of its own: the receiver is what chose this overload to begin with,
        // so there is nothing left to compare it against. It only needs the convention the prototype declares.
        if (const auto self_param = param->To<FunctionParameterSelfAst>(); self_param != nullptr) {
          // Except in the static form, "Type::method(receiver)", where the receiver is written as an ordinary argument
          // and nothing has checked it yet: it has to be the type the method was reached through, or one extending
          // it. A "Self" or generic receiver names nothing concrete to hold it to.
          auto const &arg = *fn_args.Args[slot->Args[0]];
          if (auto const *const receiver = site.CalledOn;
            not site.IsMethod() and receiver != nullptr and not receiver->IsSelfType()) {
            const auto receiver_sym = sm->CurrentScope->FindTypeSymbol(receiver);
            auto const *const a_sym = arg.Val->InferTypeRef(sm, meta).Symbol;
            if (receiver_sym != nullptr and a_sym != nullptr
              and not receiver_sym->IsGn() and not a_sym->IsGn()) {
              const auto receiver_tmpl = TypeRef::ForKindCheck(*receiver_sym, *sm->CurrentScope).Template();
              const auto matches = type_predicates::IsOrExtendsTemplate(*a_sym, *sm->CurrentScope, receiver_tmpl);
              // Named only for the error, and held here: "ERR_ARGS" keeps references, not the copy.
              if (not matches) {
                const auto a_type = arg.Val->InferType(sm, meta)->WithoutConvention();
                Raise<SppTypeMismatchError>(ERR_ARGS(*param, *receiver, arg, *a_type));
              }
            }
          }
          slot->IsSelf = true;
          slot->SelfConv = AstClone(self_param->Conv);
          continue;
        }

        // An instantiation's parameter is its type by identity, looked
        // up as it is rather than read through the instantiation's own
        // bindings, and shown as spelled; so is one written naming
        // "Self", keyed where it is declared. "Self" in it is what the
        // call decided ("PassedOverload::SelfId"): the owning type when
        // the method was reached by forwarding, since "&Str" calling
        // "StrView::eq" must be handed a "StrView", not a "Str" read
        // through "StrView"'s "{ptr, length}" shape (the "Str == Str"
        // bug). One nothing pins, and a variadic parameter's tuple, are
        // read from the parameter as written.
        const auto is_variadic = param->To<FunctionParameterVariadicAst>() != nullptr;
        auto p_sig = self_type::TypeIdWithSelf(
          is_variadic ? nullptr : param->InstanceTypeId, *param->Type, is_variadic ? nullptr : fn_scope, self_id);
        if (p_sig != nullptr and p_sig->HasSelf) { p_sig = nullptr; }

        // The parameter's type as shown, built only where one is wanted: an error, and a variadic parameter's tuple. A
        // variadic parameter's element type is read as written, with "Self" what the call decided.
        const auto spelled = [&]() -> Shared<TypeAst> {
          if (p_sig != nullptr) {
            if (auto shown = sm->CurrentScope->TypeAstOf(p_sig); shown != nullptr) { return shown; }
          }
          else if (is_variadic) {
            const auto element = self_type::TypeIdWithSelf(nullptr, *param->Type, fn_scope, self_id);
            if (auto shown = element != nullptr ? sm->CurrentScope->TypeAstOf(element) : nullptr; shown != nullptr) {
              return shown;
            }
          }
          return fn_scope->FindTypeSymbol(param->Type.get())->FqName()->WithConvention(
            AstClone(param->Type->GetConvention()));
        };
        auto const &a_type = slot->Type;

        // "..a: T" takes a tuple of "T"s, one per element the call passed; "..a: Ts" (a variadic generic) is that
        // tuple itself. Anything else is matched as written.
        auto p_written = Shared<TypeAst const>(param->Type);
        if (const auto variadic_param = param->To<FunctionParameterVariadicAst>(); variadic_param != nullptr) {
          const auto variadic_gn_param = fn_proto.GetNonGnImpl()->GnParamGroup->GetVariadicParam();
          const auto orig_name = shared_cast<TypeIdentifierAst>(variadic_param->Source.OriginalType);
          const auto is_variadic_generic_type = variadic_gn_param != nullptr
            and orig_name != nullptr
            and *orig_name == *shared_cast<TypeIdentifierAst>(variadic_gn_param->Name);
          auto p_tuple = spelled();
          if (not is_variadic_generic_type) {
            auto ts = Vec(packs::TypePackElements(*a_type).size(), p_tuple);
            p_tuple = generate::common_types::TupleType(param->PosStart(), std::move(ts));
            p_tuple->Stage7_AnalyseSemantics(sm, meta);
          }
          p_written = std::move(p_tuple);
        }
        const auto p_ref = [&] {
          return p_sig != nullptr
            ? TypeRef::Of(p_sig, *fn_scope)
            : TypeRef::Of(*p_written, *fn_scope);
        };

        // The argument's type as resolved, worked out once, and only if something below asks for it.
        auto a_ref_once = Opt<TypeRef>();
        const auto a_ref = [&]() -> TypeRef const& {
          if (not a_ref_once.has_value()) { a_ref_once = TypeRef::Of(*a_type, *sm->CurrentScope); }
          return *a_ref_once;
        };

        // A value handed over through its forwarding type is the forwarded-to value, so the argument becomes that
        // call once this overload is chosen.
        const auto forwards = [&] {
          return type_compare::TypeFwdEq(a_ref(), p_ref(), *sm->CurrentScope, *fn_scope)
            and marker_sups::CanForward(a_ref(), *sm->CurrentScope);
        };

        // An argument satisfies its parameter either outright, or by binding a generic the call is free to choose.
        const auto conventions_agree = type_compare::ConventionEq(*p_written, *a_type);
        if (not conventions_agree
          or not type_compare::Assignable(p_ref(), a_ref(), *fn_scope, *sm->CurrentScope)) {
          // The argument is the given side, the parameter the pattern whose generics it binds, by identity. A
          // parameter whose generic is already fixed by a scope enclosing the caller keys as what it is fixed to, so
          // it has to match exactly.
          const auto a_id = sm->CurrentScope->TypeIdOf(*a_type);
          const auto p_id = p_sig != nullptr ? p_sig : fn_scope->TypeIdOf(*p_written);
          auto inferred = scopes::GenericSubst();
          const auto binds_to_match = conventions_agree and a_id != nullptr
            and p_id != nullptr and scopes::UnifyTypeIds(a_id, p_id, *sm->CurrentScope, *fn_scope, inferred);

          // Forwarding is the last resort, tried only once the argument has failed to match any other way - so it
          // never displaces a match by binding that would have bound the parameter's generics correctly.
          if (not binds_to_match and forwards()) {
            slot->IsForwarded = true;
            slot->Rank = MatchRank::Forwarded;
            continue;
          }
          slot->Rank = MatchRank::Bound;
          if (not binds_to_match) {
            // Held here: "ERR_ARGS" keeps references.
            const auto shown = param->To<FunctionParameterVariadicAst>() != nullptr ? p_written : spelled();
            Raise<SppTypeMismatchError>(ERR_ARGS(*param, *shown, SlotSite(*slot, fn_args), *a_type));
          }
          continue;
        }

        // The types matched, and may still have matched by forwarding ("&Vec[T]" satisfying a "&View[T]" parameter).
        // This is the argument-position counterpart of a method being called on the value its receiver forwards to.
        // An argument a match by binding accepted never reaches here; "Self" picking the owner when the receiver only
        // forwards to it (above) is what keeps "Str == Str" from reading a "&Str" as a "StrView". Trying the forward
        // before the match by binding instead segfaults a third of the suite.
        slot->IsForwarded = forwards();
        slot->Rank = slot->IsForwarded
          ? MatchRank::Forwarded
          : chose_its_type(*slot)
          ? MatchRank::Bound
          : type_compare::TypeEq(p_ref(), TypeRef::Of(*a_type, *sm->CurrentScope), *fn_scope, *sm->CurrentScope)
          ? MatchRank::Exact
          : MatchRank::Widened;
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
      if (meta->ReturnTypeOverloadResolverType == nullptr) { return; }

      // Which return the type asked for; the overloads are only moved once it is known that exactly one does.
      auto return_matches = Vec<std::size_t>();
      for (auto i = 0uz; i < pass_overloads.size(); ++i) {
        auto const &matched = pass_overloads[i];
        // Its return type by identity, "Self" what the call decided.
        auto const &proto = *matched.Proto;
        const auto declared = proto.InstanceReturnTypeId != nullptr
          ? proto.InstanceReturnTypeId
          : matched.FnScope->TypeIdOf(*proto.ReturnType);
        const auto ret_id = scopes::SubstituteSelfId(declared, matched.SelfId);
        const auto ret_ref = TypeRef::Of(ret_id, *matched.FnScope);
        if (type_compare::Assignable(
          ret_ref, *meta->ReturnTypeOverloadResolverType, *matched.FnScope, *sm->CurrentScope)) {
          return_matches.emplace_back(i);
        }
      }

      if (return_matches.size() == 1) {
        auto narrowed = Vec<PassedOverload>();
        narrowed.emplace_back(std::move(pass_overloads[return_matches[0]]));
        pass_overloads = std::move(narrowed);
      }
    }

    /**
     * How loosely a prototype takes its arguments' count, for overloads matching every argument alike: at a fixed
     * arity (0), through a variadic parameter of one element type, concrete or generic ("..x: S32", "..x: T", 1), or
     * through one over a generic pack, taking any mix of types ("..x: Ts", 2). Read off the template: an
     * instantiation drops the generics it binds.
     */
    auto ArityLooseness(
      FunctionPrototypeAst const &proto)
      -> std::uint8_t {
      auto const *const tmpl = proto.GetNonGnImpl();
      auto const *const variadic = tmpl->FnParamGroup->GetVariadicParam();
      if (variadic == nullptr) { return 0; }
      auto const *const pack = tmpl->GnParamGroup->GetVariadicParam();
      const auto element = shared_cast<TypeIdentifierAst>(variadic->Type);
      const auto pack_name = pack != nullptr ? shared_cast<TypeIdentifierAst>(pack->Name) : nullptr;
      return element != nullptr and pack_name != nullptr and *element == *pack_name ? 2 : 1;
    }

    /**
     * Keep the overloads no other beats. One beats another when none of the call's arguments matched it worse
     * ("MatchRank") and one matched it better - compared argument by argument, a variadic parameter's rank standing for
     * each argument it takes - or, matching every argument alike, when it takes their count more tightly
     * ("ArityLooseness"). Several left is an ambiguity ("ManageMatchedOverloads").
     * @param pass_overloads The overloads that matched, narrowed in place.
     * @param arg_count The number of the call's arguments (its receiver included).
     */
    auto SelectBestRanked(
      Vec<PassedOverload> &pass_overloads,
      const std::size_t arg_count)
      -> void {
      if (pass_overloads.size() < 2) { return; }
      const auto ranks_of = [arg_count](PassedOverload const &o) {
        auto ranks = Vec<MatchRank>(arg_count, MatchRank::Exact);
        for (auto const &slot : o.Slots) {
          for (const auto i : slot.Args) { ranks[i] = slot.Rank; }
        }
        return ranks;
      };
      const auto ranks = pass_overloads | genex::views::transform(ranks_of) | genex::to<Vec>();
      const auto looseness = pass_overloads
        | genex::views::transform([](auto const &o) { return ArityLooseness(*o.Proto); })
        | genex::to<Vec>();
      const auto beats = [&](std::size_t a, std::size_t b) {
        auto better = false;
        for (auto i = 0uz; i < arg_count; ++i) {
          if (ranks[a][i] > ranks[b][i]) { return false; }
          better |= ranks[a][i] < ranks[b][i];
        }
        return better or looseness[a] < looseness[b];
      };

      auto beaten = Vec<bool>(pass_overloads.size(), false);
      for (auto a = 0uz; a < pass_overloads.size(); ++a) {
        for (auto b = 0uz; b < pass_overloads.size() and not beaten[a]; ++b) { beaten[a] = a != b and beats(b, a); }
      }
      auto best = Vec<PassedOverload>();
      for (auto a = 0uz; a < pass_overloads.size(); ++a) {
        if (not beaten[a]) { best.emplace_back(std::move(pass_overloads[a])); }
      }
      pass_overloads = std::move(best);
    }

    auto ManageMatchedOverloads(
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      Vec<PassedOverload> const &pass_overloads,
      Vec<FailedOverload> const &fail_overloads,
      FunctionCallArgumentGroupAst const &arg_group,
      const bool has_receiver,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      // How the call's arguments read, for either error below; a method's receiver as "Self".
      using namespace std::string_literals;
      const auto arg_usage_signature = [&arg_group, has_receiver, sm, meta] {
        return arg_group.Args
          | genex::views::ptr
          | genex::views::enumerate
          | genex::views::transform([has_receiver, sm, meta](auto const &x) {
            auto const &[i, arg] = x;
            return i == 0 and has_receiver ? "Self"_str : arg->InferType(sm, meta)->ToString();
          })
          | genex::views::intersperse(", "_str)
          | genex::views::join
          | genex::to<Str>();
      };

      // If there are no pass overloads, raise an error.
      if (pass_overloads.empty()) {
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
          ERR_ARGS(fn_call, failed_signatures_and_errors, arg_usage_signature()),
          std::move(sub_errors));
      }

      // If there are multiple pass overloads, raise an error.
      if (pass_overloads.size() > 1) {
        auto signatures = "\n" + (pass_overloads
          | genex::views::transform([](auto const &x) { return "    - "s + x.Proto->PrintSignature(""); })
          | genex::views::intersperse("\n"_str)
          | genex::views::join
          | genex::to<Str>());

        Raise<errors::SppFunctionCallOverloadAmbiguousError>(ERR_ARGS(fn_call, signatures, arg_usage_signature()));
      }
    }

    /// The core checker to see if a function prototype is
    /// compatible with the function call made against it as a
    /// candidate. Explicit generics from the call + sup enclosing
    /// generics + argument->parameter generic inference are
    /// combined in the generic solver to instantiate a possibly
    /// generic function. Each parameter/argument pair is then
    /// checked for compatibility/availability (number of
    /// arguments vs parameters must be compatible too).
    auto CheckCandidate(
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      FnOverload &candidate, CallSite const &site, const bool is_indirect,
      ScopeManager *sm, meta::CompilerMetaData *meta) -> PassedOverload {
      using errors::SppFunctionCallTooManyArgumentsError;
      auto fn_proto = candidate.Proto;
      auto fn_scope = candidate.FnScope;
      const auto is_variadic_fn = fn_proto->FnParamGroup->GetVariadicParam() != nullptr;
      auto const &fn_args = *fn_call.FnArgGroup;

      // Cannot check for "too few" arguments here because of
      // potential "T=Void" + "x: T" removal. Check if there are
      // too many arguments (for a non-variadic function).
      RaiseIf<SppFunctionCallTooManyArgumentsError>(
        fn_args.Args.size() > fn_proto->FnParamGroup->Params.size() and not is_variadic_fn,
        ERR_ARGS(*fn_proto, fn_proto->FnParamGroup->Params.size(), fn_call, fn_args.Args.size()));

      // Add a generic solver for the function generics. The order
      // of precedence is that the call wrote explicitly, then the
      // "sup" enclosed generics on the receiver, then the pre-bound
      // scope generics. So for "SomeType[A=S32]()::func[B=Str]",
      // "A=S32" is a receiver arg, "B=Str" is a explicit arg, and
      // any generic symbols in scope are the pre-bound ones.
      auto solver = generic_inference::GenericSolver(*fn_proto->GnParamGroup, *fn_scope, *sm, *meta);
      solver.Give(std::move(generic_inference::NamedGnArgs(
        *fn_call.GnArgGroup, *fn_proto->GnParamGroup, *fn_proto->Name, *sm, *meta)->Args));
      solver.Give(
        ReceiverGns(fn_scope, candidate.FwdType != nullptr ? candidate.FwdType.get() : site.OwnerType.get(), *sm),
        *sm->CurrentScope);
      if (candidate.ReachedThrough != nullptr) {
        solver.Give(BoundGnsOf(*candidate.ReachedThrough), *sm->CurrentScope);
      }

      // Determine the true value for "Self" for this function
      // call, and then pin that self into the receiver. A value
      // called through its function type ("h.f()", "f" a field)
      // is not pinned: its prototype is built from that type,
      // so it is declared by no class and is not instantiated
      // per receiver.
      const auto call_self = CallSelf(candidate, site, sm);
      if (not is_indirect) { PinSelfToReceiver(fn_scope, call_self, solver, sm, meta); }

      // Get the solver's currently known bindings. A default is
      // read with what is known so far, "Self" as the call
      // pinned it. Bind the slots, and infer all the generics
      // from the solver.
      const auto default_bindings = solver.KnownBindings(*fn_proto->GnParamGroup);
      auto slots = BindSlots(
        fn_args, *fn_proto->FnParamGroup, default_bindings, const_cast<Scope&>(*fn_scope), sm, meta);
      InferAllGns(*fn_proto, fn_args, slots, site, solver, sm, meta);
      const auto inferred = solver.InferredParams();
      auto const &template_scope = *fn_scope;

      // What the variadic parameter actually receives: the tuple
      // of the trailing arguments.
      auto variadic_pack_type = Shared<TypeAst>(nullptr);
      if (is_variadic_fn) {
        const auto variadic_param = fn_proto->FnParamGroup->GetVariadicParam();
        const auto pack = genex::find_if(slots, [&](auto const &slot) { return slot.Param == variadic_param; });
        if (pack != slots.end()) {
          variadic_pack_type = AstCloneShared(pack->Type);
          variadic_pack_type->ClearPartsSourceWritten();
        }
      }

      // Found or made from the solution's identity: no arguments
      // are built.
      auto template_proto = fn_proto;
      tie(fn_proto, fn_scope) = monomorphization::FindOrMakeFnInstance(
        fn_proto, fn_scope, solver.SolvedArgsId(), variadic_pack_type, sm, meta);
      candidate.Proto = fn_proto;
      candidate.FnScope = fn_scope;

      // A default the call takes is made again once everything is solved. The one "BindSlots" made (and inference
      // read) had only what was bound before inference; the instantiation's own parameter carries it translated with
      // the whole solution ("Vec[T]::new()" as the default of an "A" inferred from a later argument).
      // The instantiation's parameters are the template's less any now "Void", so its own is found by name, the
      // identity a keyword argument gives it.
      if (fn_proto != template_proto) {
        for (auto &slot : slots) {
          if (slot.Default == nullptr) { continue; }
          const auto param = genex::find_if(fn_proto->FnParamGroup->Params, [&](auto const &p) {
            return p->ExtractName()->Val == slot.Name->Val;
          });
          if (param == fn_proto->FnParamGroup->Params.end()) { continue; }
          auto const *const optional = (*param)->To<FunctionParameterOptionalAst>();
          if (optional == nullptr or optional->DefaultVal == nullptr) { continue; }
          slot.Default = AnalysedDefault(AstClone(optional->DefaultVal), *fn_scope, *sm, meta);
          slot.Type = slot.Default->InferType(sm, meta);
        }
      }

      // What "Self" stands for at this call ("PassedOverload::SelfId"): what the call decided ("CallSelf"), else the
      // owner, read from the instantiation's own scope, where the owner's generics are bound.
      auto self_id = call_self != nullptr ? sm->CurrentScope->TypeIdOf(*call_self) : nullptr;
      if (self_id == nullptr and fn_scope != nullptr) {
        if (const auto owner = fn_scope->FindEnclosingSelfType(*meta); owner != nullptr) {
          self_id = fn_scope->TypeIdOf(*owner);
        }
      }

      ValidateArgsMatchParams(
        fn_call, *fn_proto, fn_scope, fn_args, slots, self_id, template_scope, inferred, site, sm, meta);
      return PassedOverload{fn_scope, fn_proto, std::move(slots), self_id};
    }
  }
}

namespace spp::analyse::utils::overload_resolution {
  namespace {
    /**
     * Whether "error" is an "E".
     */
    template <typename E>
    auto IsError(
      errors::SemanticError const &error)
      -> bool {
      return dynamic_cast<E const*>(&error) != nullptr;
    }

    /**
     * Why an error a candidate's check raised passes the candidate over, as "no valid signatures" lists it; none for
     * an error that is the call's own, which is raised as it is.
     */
    auto RejectionReasonOf(
      errors::SemanticError const &error)
      -> Opt<StrView> {
      using namespace errors;
      if (IsError<SppFunctionCallAbstractFunctionError>(error)) { return "calling an abstract function"; }
      if (IsError<SppFunctionCallTooManyArgumentsError>(error)) { return "too many arguments"; }
      if (IsError<SppArgumentNameInvalidError>(error)) { return "invalid argument name"; }
      if (IsError<SppArgumentMissingError>(error)) { return "missing required argument"; }
      if (IsError<SppTypeMismatchError>(error)) { return "type mismatch"; }
      if (IsError<SppGenericParameterConflictError>(error)) { return "inferred generic parameter conflict"; }
      if (IsError<SppGenericParameterNotInferredError>(error)) { return "generic parameter not inferred"; }
      if (IsError<SppGenericArgumentTooManyError>(error)) { return "too many generic arguments"; }
      if (IsError<SppGenericConstraintError>(error)) { return "generic constraint not satisfied"; }
      return Nullopt;
    }
  }
}

SPP_MOD_BEGIN
OverloadSolver::OverloadSolver(
  PostfixExpressionOperatorFunctionCallAst &fn_call,
  ExpressionAst &callee, ScopeManager &sm,
  meta::CompilerMetaData &meta) :
  _Call(fn_call),
  _Sm(&sm),
  _Meta(&meta),
  _Site(CallSiteOf(callee, sm, &meta)),
  _Candidates(RetrieveAllOverloads(_Site, &sm, &meta)) {
}

OverloadSolver::~OverloadSolver() = default;

/// [CHECKED]
auto OverloadSolver::ExpectedArgTypes() const -> Vec<Shared<TypeRef>> {
  // Build the "out" list the same size as the number of args
  // going in (1-to-1 arg/type map, or nullptr, so same size).
  using type_compare::TypeEq;
  auto const &fn_call = _Call;
  auto out = Vec<Shared<TypeRef>>(fn_call.FnArgGroup->Args.size());

  // An expansion ("..t") has not been unpacked yet, so argument
  // positions do not yet line up with parameters. Cannot give
  // a list of types yet.
  const auto expands = genex::any_of(fn_call.FnArgGroup->Args, [](auto const &arg) {
    const auto pos = arg->template To<FunctionCallArgumentPositionalAst>();
    return pos != nullptr and pos->TokUnpack != nullptr;
  });
  if (expands or _Candidates.Overloads.empty()) { return out; }

  // Cross-check for every argument, against every candidate;
  // can we get an agreement on the expected type for this
  // argument.
  for (auto const &[i, arg] : fn_call.FnArgGroup->Args | genex::views::ptr | genex::views::enumerate) {
    const auto kw_arg = arg->To<FunctionCallArgumentKeywordAst>();
    auto expected = Shared<TypeRef>(nullptr);
    auto expected_scope = static_cast<Scope const*>(nullptr);
    auto agreed = true;

    for (auto const &candidate : _Candidates.Overloads) {
      // A method's receiver is not among the written arguments.
      const auto params = _Site.IsMethod()
        ? candidate.Proto->FnParamGroup->GetNonSelfParams()
        : candidate.Proto->FnParamGroup->GetAllParams();

      // Match the argument by position or keyword to a parameter.
      // This saves the corresponding parameter for this argument.
      auto param = static_cast<FunctionParameterAst const*>(nullptr);
      if (kw_arg == nullptr) { param = i < params.size() ? params[i] : nullptr; }
      else {
        for (const auto p : params) { if (*p->ExtractName() == *kw_arg->Name) { param = p; } } // break?
      }

      // No parameter here, a pack, a generic or a "Self": what
      // the argument must be is not known without the argument,
      // so a multi-overload function call as the argument is an
      // ambiguous result.
      if (param == nullptr or param->To<FunctionParameterVariadicAst>() != nullptr
        or type_predicates::DoesTypeNameSelf(*param->Type)
        or not type_predicates::IsTypeConcrete(*param->Type, *candidate.FnScope)) {
        agreed = false;
        break;
      }

      // If we already have an expected type for this slot (from
      // another overload), and this overload disagrees, then
      // we need to rely on the argument itself again.
      const auto ref = MakeShared<TypeRef>(TypeRef::Of(*param->Type, *candidate.FnScope));
      if (expected != nullptr and not TypeEq(*expected, *ref, *expected_scope, *candidate.FnScope)) {
        agreed = false;
        break;
      }

      // Otherwise, we can mark the expected type based off of
      // the agreeing overloads.
      expected = ref;
      expected_scope = candidate.FnScope;
    }

    // Given all the overloads agreed about the argument, mark
    // its expected type. Otherwise, leave it as nullptr.
    if (agreed) { out[i] = expected; }
  }
  return out;
}

auto OverloadSolver::Resolve() -> Pair<PassedOverload, Shared<TypeAst const>> {
  auto const &fn_call = _Call;
  const auto sm = _Sm;
  const auto meta = _Meta;
  auto const &site = _Site;
  auto &candidates = _Candidates;

  // A method called on a value takes that value as "self":
  // "1.add(2)" is checked as "S32::add(1, 2)". A runtime
  // access naming no method (a closure held in an field)
  // is called as the value it is, with no receiver.
  const auto has_receiver = site.IsMethod() and candidates.IndirectFnType == nullptr;
  if (has_receiver) { InjectReceiver(fn_call, site); }
  auto pass_overloads = Vec<PassedOverload>{};
  auto fail_overloads = Vec<FailedOverload>{};

  // Check each provided overload for a complete match.
  // Save the "passed" overloads, and reject the failed
  // ones (store for potential error).
  for (auto &candidate : candidates.Overloads) {
    auto checked = _Check(candidate);
    if (checked.has_value()) { pass_overloads.emplace_back(std::move(*checked)); }
    else { fail_overloads.emplace_back(std::move(checked.error())); }
  }

  NarrowByReturnType(pass_overloads, sm, meta);
  SelectBestRanked(pass_overloads, fn_call.FnArgGroup->Args.size());

  ManageMatchedOverloads(
    fn_call, pass_overloads, fail_overloads,
    *fn_call.FnArgGroup, has_receiver, sm, meta);

  // Only now is it known which overload the call makes, so only
  // now is its instantiation required; the other candidates' were
  // built just to check the call against.
  pass_overloads[0].Proto->RequireGnSubstitution(fn_call.Source.OriginalExpr, sm->CurrentScope);
  pass_overloads[0].HasReceiver = has_receiver;

  return {std::move(pass_overloads[0]), candidates.IndirectFnType};
}

auto OverloadSolver::_Check(
  FnOverload &candidate)
  -> Expected<PassedOverload, FailedOverload> {
  // A check that raises part way has its "meta" changes put back
  // by the guards it unwinds through.
  try {
    return CheckCandidate(_Call, candidate, _Site, _Candidates.IndirectFnType != nullptr, _Sm, _Meta);
  }
  catch (errors::SemanticError const &e) {
    const auto reason = RejectionReasonOf(e);
    if (not reason.has_value()) { throw; }
    return Unexpected(FailedOverload{candidate.Proto, e.what(), Str(*reason)});
  }
}

SPP_MOD_END

auto spp::analyse::utils::overload_resolution::ElaborateCall(
  FunctionCallArgumentGroupAst &args,
  PassedOverload &overload,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // The values are moved out of the written arguments into one keyword argument per slot. Nothing analysed is
  // dropped: the scopes an argument's analysis created (a loop's, a closure's) name its value as their "AstNode".
  auto written = std::move(args.Args);
  args.Args.clear();
  for (auto &slot : overload.Slots) {
    auto conv = Unique<ConventionAst>(nullptr);
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

    args.Args.emplace_back(
      MakeUnique<FunctionCallArgumentKeywordAst>(slot.Name, nullptr, std::move(conv), std::move(val)));
  }
}

/// [CHECKED]
auto spp::analyse::utils::overload_resolution::MethodsOf(
  Scope const &scope, IdentifierAst const &name) -> Vec<MethodEntry> {
  // Create an entry for the scope in the method table. This
  // auto creates the value for this key, which is a
  // "MethodTable". Built-in generation caching is reused.
  auto &table = _MethodTables()[&scope];
  const auto generation = scopes::TypeStructureGeneration();
  const auto is_cls = AstAs<ClassPrototypeAst>(scope.AstNode) != nullptr;

  // For a new generation, the cache must be invalidated, so
  // rebuild it; for a class, get all its method on all the
  // sup scopes. For a specific sup scope, just use that sup
  // scope.
  if (table.Generation != generation) {
    const auto sups = is_cls
      ? Vec<Scope*>(scope.GetSupScopes())
      : Vec{const_cast<Scope*>(&scope)};

    if (not genex::equals(table.Sups, sups)) {
      table.Sups = sups; // Update the scope list to use.
      table.ByName.clear(); // Clear the cached entries.
    }

    table.Generation = generation;
  }

  // Intern the function name (an identifier), which is the
  // key of the method table. If found in the cache, pull it
  // out.
  const auto key = static_cast<std::uint32_t>(name.NameId());
  if (const auto hit = table.ByName.find(key); hit != table.ByName.end()) {
    return hit->second;
  }

  // Each "sup $name ext FunXxx" in each "sup" scope, in its
  // own block.
  const auto mock = name.ToFnIdentifier();
  auto found = Vec<MethodEntry>();
  for (const auto sup_scope : table.Sups) {
    // Within a type's sup scopes, they will each carry a number
    // of sup-asts, which are pre-processed functions, each in
    // turn containing the actual function.
    for (const auto sup_ast : AstBody(sup_scope->AstNode) | genex::views::cast_dynamic<SupPrototypeExtensionAst*>()) {
      if (sup_ast->Name->As<TypeIdentifierAst>()->Name != mock->Val) { continue; }

      // Now we know the name is correct for the target function
      // name ("sup $name ext ..." where the target name is
      // "$name"), we can get the function prototype, and the
      // scope for it.
      const auto proto = AstBody(sup_ast)[0]->To<FunctionPrototypeAst>();
      const auto block = BlockOf(*sup_scope, *sup_ast);
      if (block == nullptr) { Ice(*sup_ast, *sup_scope, "method has no block in the scope it was reached through"); }

      found.emplace_back(MethodEntry{
        .FnScope = block,
        .Proto = proto,
        .ReachedThrough = sup_scope
      });
    }
  }

  // Overrides: the derived of two with the same signature is
  // kept. Its signature is read in its own block, the other's
  // where it was reached.
  auto hidden = Set<FunctionPrototypeAst const*>();
  for (auto const &derived : found) {
    for (auto const &base : found) {
      // For a override to be an override, the "derived" scope
      // must in-fact be a derived scope in the sup-tree. Also
      // skip for exact by-pointer prototype matches.
      if (derived.Proto == base.Proto
        or scope.DepthDiff(derived.ReachedThrough) >= scope.DepthDiff(base.ReachedThrough)) { continue; }

      // If the signature matches, then the derived one wins,
      // so mark the base version as "hidden". This will be
      // removed from the available set.
      if (fn_signatures::SameSignature(*derived.Proto, *derived.FnScope, *base.Proto, *base.ReachedThrough)) {
        hidden.insert(base.Proto);
      }
    }
  }

  // Filter off, by ast, any function prototype asts that have
  // been overridden by more derived versions. Add the set into
  // method table for this key.
  found |= genex::actions::remove_if([&hidden](auto const &e) { return hidden.contains(e.Proto); });
  table.ByName[key] = found;
  return found;
}

auto spp::analyse::utils::overload_resolution::ClearCaches()
  -> void {
  _MethodTables().clear();
  _IndirectPrototypes().clear();
}

/// [CHECKED]
auto spp::analyse::utils::overload_resolution::GetAllFnScopes(
  IdentifierAst const &target_fn_name, Scope const *target_scope,
  ScopeManager &sm, meta::CompilerMetaData *meta) -> Vec<FnOverload> {
  // If the name is empty (non-symbolic call) then return
  // "no scopes". If the target scope is nullptr, then the
  // functions are being superimposed over a generic type.
  if (target_fn_name.Val.empty() or target_scope == nullptr) { return {}; }

  // Get the function-type name from the function: "func()"
  // => "$func". Start with an empty set of function
  // overloads.
  const auto mapped_name = target_fn_name.ToFnIdentifier();
  auto overload_scopes = Vec<FnOverload>();

  // A discovered sup-ext scope is valid if the ast is the
  // correct type, and its target type matches the mapped
  // name ("$func"). Todo: no interning?
  auto is_valid_ext_scope = [mapped_name=mapped_name.get()](auto const *scope) {
    const auto ext = AstAs<SupPrototypeExtensionAst>(scope->AstNode);
    if (ext == nullptr) { return false; }
    const auto ext_name = ext->Name->template To<TypeIdentifierAst>();
    return ext_name != nullptr and ext_name->Name == mapped_name->Val;
  };

  // Check for namespaced (module-level) functions (they
  // will have no inheritable generics as they are free
  // functions, not inside a "sup" block).
  if (target_scope->LinkedNamespaceSymbol != nullptr) {
    for (const auto ancestor_scope : target_scope->GetAncestors()) {
      for (const auto sup_scope : ancestor_scope->Children
           | genex::views::ptr
           | genex::views::filter(is_valid_ext_scope)) {
        overload_scopes.emplace_back(FnOverload{
          .FnScope = sup_scope,
          .Proto = fn_values::FnBlockOf(*sup_scope).second,
          .ReachedThrough = nullptr,
          .FwdType = nullptr
        });
      }
    }
  }

  // Functions belonging to a type will have inheritance
  // generics from "sup [...] Type { ... }": its methods,
  // once overrides are applied ("MethodsOf"). Get the
  // available methods from the target scope and add them
  // to the local overload set.
  else {
    for (auto const &entry : MethodsOf(*target_scope, target_fn_name)) {
      overload_scopes.emplace_back(FnOverload{
        .FnScope = entry.FnScope,
        .Proto = entry.Proto,
        .ReachedThrough = entry.ReachedThrough,
        .FwdType = nullptr
      });
    }
  }

  // Next, get scopes from "forwarding types" (ie FwdRef and
  // FwdMut return types). Forwarding is a fallback, so the
  // receiver's own methods shadow the forwarded-to ones;
  // without this, a type whose forwarded-to type also forwards
  // (e.g. "NonNull[Str]" -> "&Str" -> "&StrView") sees both
  // "fwd_ref" overloads and the forwarding call is ambiguous.
  if (target_scope->LinkedTypeSymbol != nullptr
    and meta->CurrentStage >= meta::CompilerStage::kAnalyseSemantics
    and overload_scopes.empty()) {
    // Get the scopes on the scope for the forwarded type,
    // and append the scope list to the previously discovered
    // ones.
    const auto [fwd_ref_sup, fwd_mut_sup] = marker_sups::FindFwdSups(
      TypeRef::Of(*target_scope->LinkedTypeSymbol, *sm.CurrentScope), *sm.CurrentScope);
    const auto fwd_ref = marker_sups::FwdTargetOf(fwd_ref_sup);
    const auto fwd_mut = marker_sups::FwdTargetOf(fwd_mut_sup);
    if (fwd_ref.Symbol != nullptr or fwd_mut.Symbol != nullptr) {
      const auto inner_sym = fwd_ref.Symbol != nullptr
        ? fwd_ref.Symbol
        : fwd_mut.Symbol;

      auto fwd_overloads = inner_sym != nullptr
        ? GetAllFnScopes(target_fn_name, inner_sym->LinkedScope, sm, meta)
        : Vec<FnOverload>{};

      for (auto &i : fwd_overloads) {
        i.FwdType = AstCloneShared(inner_sym->FqName());
      }
      overload_scopes.append_range(genex::views::move(fwd_overloads));
    }
  }

  // Return all the found function scopes.
  return overload_scopes;
}
