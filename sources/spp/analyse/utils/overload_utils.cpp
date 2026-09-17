module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.overload_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.cmp_utils;
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
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
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
import spp.asts.generic_parameter_type_inline_constraints_ast;
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

namespace spp::analyse::utils::overload_utils {
  namespace {
    struct OverloadCandidates {
      bool IsClosure;
      Unique<FunctionPrototypeAst> ClosureProto;
      Vec<func_utils::FunctionOverload> Overloads;
    };

    struct PropagatedMethodCall {
      PassedOverload Overload;
      bool IsClosure;
      Unique<PostfixExpressionAst> TransformedAst;
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
      FunctionPrototypeAst const &new_fn_proto,
      FunctionPrototypeAst const &fn_proto,
      Scope *new_fn_scope,
      GenericArgumentGroupAst const &combined_generics,
      ScopeManager &tm,
      meta::CompilerMetaData *meta)
      -> void {
      for (auto *p : new_fn_proto.FnParamGroup->GetNonSelfParams()) {
        const auto declared = p->Source.OriginalType;
        if (declared == nullptr) { continue; }

        const auto gn_param = genex::find_if(
          fn_proto.GnParamGroup->Params, [&](auto const &g) { return *g->Name == *declared; });
        if (gn_param == fn_proto.GnParamGroup->Params.end()) { continue; }

        auto const &constraints = *gn_param;
        if (constraints->Constraints == nullptr) { continue; }

        for (auto const &c : constraints->Constraints->Constraints) {
          if (not type_predicates::IsTypeFunc(TypeRef::OfHead(*c, *new_fn_scope), *new_fn_scope)) { continue; }
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
      FunctionPrototypeAst &new_fn_proto,
      Scope *new_fn_scope,
      GenericArgumentGroupAst const &combined_generics,
      Shared<TypeAst> const &variadic_pack_type,
      ScopeManager &tm,
      meta::CompilerMetaData *meta)
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
            self_sym->Type = substituted_self->WithConvention(AstClone(self_param->Conv));
          }
        }
      }
      new_fn_proto.VariadicPackType = AstClone(variadic_pack_type);

      // A variadic parameter declares one element but binds the whole tuple.
      if (variadic_pack_type != nullptr) {
        auto pack_type = AstClone(variadic_pack_type);
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
      GenericArgumentGroupAst const &combined_generics,
      FunctionPrototypeAst const &new_fn_proto,
      Scope const *new_fn_scope,
      ScopeManager &tm,
      ScopeManager const *sm,
      meta::CompilerMetaData *meta)
      -> bool {
      const auto type_is_concrete = [&](TypeAst const &type) {
        const auto resolved = type_utils::SubstituteSelfTypeAndAnalyse(type, *new_fn_scope, tm, *meta);
        return type_predicates::IsTypeFullyConcrete(*resolved, *new_fn_scope);
      };

      // The arguments are asked the same way a class instantiation asks them; a function goes on to check the
      // signature it ended up with, which a class has no equivalent of.
      return type_predicates::AreGenericArgsConcrete(combined_generics.Args, *sm->CurrentScope)
        and type_is_concrete(*new_fn_proto.ReturnType)
        and genex::all_of(new_fn_proto.FnParamGroup->GetAllParams(), [&](auto *p) {
          return type_is_concrete(*p->Type);
        });
    }

    auto GetFuncOwnerTypeAndFuncName(
      ExpressionAst const &lhs,
      ScopeManager &sm,
      meta::CompilerMetaData *meta)
      -> Tup<Shared<TypeAst>, Scope const*, Shared<IdentifierAst>> {
      //
      using expr_utils::RaiseMissingIdentifierAndClosestOptions;

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
      if (lhs_as_ident and sm.CurrentScope->GetVarSymbol(lhs_as_ident) == nullptr) {
        RaiseMissingIdentifierAndClosestOptions(*lhs_as_ident, sm.CurrentScope->AllVarSymbols(), {}, sm);
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
        fn_owner_scope = sm.CurrentScope->GetTypeSymbol(fn_owner_type.get())->LinkedScope;
      }

      // Static access into a type: "Type::method()" or
      // "ns::Type::method()".
      else if (static_field != nullptr and postfix_lhs_as_type != nullptr) {
        fn_owner_type = AstCloneShared(postfix_lhs_as_type);
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
        fn_name = AstCloneShared(lhs_as_ident);

        // A name declared inside the function (a function-type
        // variable) is a value being called, not a module function
        // spelled the same.
        const auto mod_scope = sm.CurrentScope->ParentModule();
        const auto sym = sm.CurrentScope->GetVarSymbol(lhs_as_ident);
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
        if (auto [value_fn_name, value_fn_scope] = func_utils::GetFunctionValueName(lhs_ref);
          value_fn_name != nullptr) {
          fn_name = std::move(value_fn_name);
          fn_owner_scope = value_fn_scope;
        }
      }

      return {fn_owner_type, fn_owner_scope, fn_name};
    }

    auto ConvertMethodToFuncForm(
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
      // expression type.
      auto ret_ty = expr_ty.LastTypePart()->GnArgGroup->At("Out")->TypeVal;
      auto p_tys = expr_ty.LastTypePart()->GnArgGroup->At("Args")->TypeVal->LastTypePart()->GnArgGroup->GetTypeArgs()
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

    auto NamedArgsOnly(
      Vec<Unique<GenericArgumentAst>> &&args)
      -> Vec<Unique<GenericArgumentAst>> {
      //
      using namespace spp::asts;
      auto out = Vec<Unique<GenericArgumentAst>>();
      for (auto &&arg : args) {
        if (arg->Name != nullptr) { out.EmplaceBack(std::move(arg)); }
      }
      return out;
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
      const auto self_param = fn_proto.FnParamGroup->GetSelfParam();
      if (self_param != nullptr and self_param->Conv == nullptr) { return true; }

      return type_predicates::NamesSelfType(*fn_proto.ReturnType)
        or genex::any_of(
          fn_proto.FnParamGroup->GetNonSelfParams(),
          [&](auto const *p) { return type_predicates::NamesSelfType(*p->Type); });
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
      meta::CompilerMetaData const *meta)
      -> TypeAst* {
      if (meta->PostfixExpressionLhs == nullptr) { return nullptr; }
      const auto postfix = meta->PostfixExpressionLhs->To<PostfixExpressionAst>();
      return postfix != nullptr ? postfix->Lhs->To<TypeAst>() : nullptr;
    }

    auto RetrieveOwnerGenericArgs(
      Shared<TypeAst> const &fwd_type,
      meta::CompilerMetaData const *meta)
      -> Vec<Unique<GenericArgumentAst>> {
      // A forwarding type stands in for the owner, so its
      // generics are the ones that count.
      if (fwd_type != nullptr) {
        return NamedArgsOnly(std::move(fwd_type->LastTypePart()->GnArgGroup->Args));
      }

      // Otherwise take them from the type the call was made
      // on, if it was made on one at all - a module-level
      // function has no owner to inherit from.
      const auto receiver = ReceiverTypeAtCallSite(meta);
      const auto is_type = receiver != nullptr ? AstCloneShared(receiver) : nullptr;
      if (is_type != nullptr) {
        return NamedArgsOnly(std::move(is_type->LastTypePart()->GnArgGroup->Args));
      }

      return {};
    }

    auto RetrieveAllOverloads(
      IdentifierAst const *fn_name,
      Scope const *fn_owner_scope,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
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
          .SupGenerics = GenericArgumentGroupAst::NewEmpty(),
          .FwdType = nullptr
        });
        return OverloadCandidates{true, std::move(closure_fn_proto), std::move(all_overloads)};
      }

      // Otherwise, there are no scopes (handled in caller).
      return OverloadCandidates{false, nullptr, {}};
    }

    auto PropagateMethodToFunction(
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
      auto [transformed_lhs, transformed_fn_call] = ConvertMethodToFuncForm(
        fn_owner_type, fn_name, cast_lhs, fn_call, *sm, meta);

      // Determine the overload based off the function
      // (uniform system).
      auto [overload, is_closure] = [&] {
        const auto _meta_guard = meta::MetaGuard(meta);
        meta->PostfixExpressionLhs = transformed_lhs.get();
        return DetermineOverload(*transformed_fn_call, sm, meta);
      }();

      // Get the argument group with the "self" injection,
      // and bind it to the function call.
      fn_call.FnArgGroup = AstClone(transformed_fn_call->FnArgGroup);

      // Create a mock postfix based on the transformation.
      transformed_lhs->Stage7_AnalyseSemantics(sm, meta);
      auto pf = MakeUnique<PostfixExpressionAst>(
        std::move(transformed_lhs), std::move(transformed_fn_call));
      return PropagatedMethodCall{std::move(overload), is_closure, std::move(pf)};
    }

    /**
     * Bind the two kinds of "sup" generic that attaching the block to the receiver leaves unbound: a trailing pack
     * ("sup [First, ..Rest] Tup[First, Rest]", matched but never bound) and a blanket block's own type ("sup [T] T",
     * attached unsubstituted). Both are read straight off the receiver, and offered last, so a binding the call or the
     * attachment made still wins. Left unbound, the method reaches code generation still generic, and has no
     * declaration to call.
     * @param fn_scope The scope the candidate was declared in; the "sup" block it was written in is found above it.
     * @param fn_owner_type The type the call was made on, or @c nullptr for a free function.
     * @param gn_args The generic arguments for this candidate, merged into in place.
     * @todo: See if this can be removed - works but bloaty
     */
    auto BindBlanketAndPackGenericsFromReceiver(
      Scope const *fn_scope,
      TypeAst const *fn_owner_type,
      GenericArgumentGroupAst &gn_args)
      -> void {
      if (fn_owner_type == nullptr or fn_scope == nullptr) { return; }

      // A method is lowered into its own "sup $F ext FunXxx" block,
      // inside the one it was written in; the generics are on that.
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
      if (pattern == nullptr) { return; }
      const auto receiver = fn_owner_type->WithConvention(nullptr);
      auto bound = Vec<Unique<GenericArgumentAst>>();

      // A blanket block's type is its own generic, standing for
      // the whole receiver.
      const auto pattern_sym = sup_scope->GetTypeSymbol(pattern->WithoutGenerics().get());
      if (pattern_sym != nullptr and pattern_sym->IsTypeGeneric()) {
        if (auto name = dynamic_shared_cast<TypeIdentifierAst>(pattern->WithoutGenerics()); name != nullptr) {
          bound.EmplaceBack(GenericArgumentAst::NewType(std::move(name), receiver));
        }
      }

      // A trailing pack stands for the receiver's arguments from
      // its position on, as the tuple a variadic generic is bound
      // to everywhere else.
      else {
        auto const &pattern_args = pattern->LastTypePart()->GnArgGroup->Args;
        const auto receiver_args = receiver->LastTypePart()->GnArgGroup->GetTypeArgs();
        const auto last = not pattern_args.IsEmpty() and pattern_args.Back()->TypeVal != nullptr
          ? pattern_args.Back().get()
          : nullptr;
        const auto last_sym = last != nullptr ? sup_scope->GetTypeSymbol(last->TypeVal->WithoutGenerics().get()) : nullptr;
        if (last_sym != nullptr and last_sym->IsTypeGeneric() and last_sym->IsVariadic
          and receiver_args.Len() + 1 >= pattern_args.Len()) {
          auto elems = Vec<Shared<TypeAst>>();
          for (auto i = pattern_args.Len() - 1; i < receiver_args.Len(); ++i) { elems.EmplaceBack(receiver_args[i]->TypeVal); }
          if (auto name = dynamic_shared_cast<TypeIdentifierAst>(last->TypeVal); name != nullptr) {
            bound.EmplaceBack(GenericArgumentAst::NewType(
              std::move(name), generate::common_types::TupleType(0, std::move(elems))));
          }
        }
      }
      gn_args.MergeGenerics(std::move(bound));
    }

    /**
     * Pin "Self" to the receiver for this candidate, if the receiver is what "Self" stands for here. A method declared
     * on a class and called on an implementer of it needs "Self" bound to the implementer, not left as the declaring
     * class - that is what lets "Writer::write_all" call "self.write()" and reach the implementer's "write" rather
     * than the abstract one. The pin is merged into @p gn_args and also returned, because inference rebuilds the
     * argument list from the prototype's own generic parameters and drops a name that is not one of them.
     * @param fn_proto The candidate prototype.
     * @param fn_scope The scope the candidate was declared in.
     * @param fn_owner_type The type the call was made on, or @c nullptr when the callee is a plain function name and
     * there is no receiver - a free function has nothing for "Self" to be pinned to.
     * @param gn_args The generic arguments for this candidate, merged into in place.
     * @param sm The scope manager, positioned at the call site.
     * @param meta Associated metadata.
     * @return The pinned receiver type to re-apply after inference, or @c nullptr when nothing was pinned.
     */
    auto PinSelfToReceiver(
      FunctionPrototypeAst const &fn_proto,
      Scope const *fn_scope,
      TypeAst const *fn_owner_type,
      GenericArgumentGroupAst &gn_args,
      ScopeManager const *sm,
      meta::CompilerMetaData const *meta)
      -> Shared<TypeAst> {
      using type_compare::TypeEq;

      const auto declared_self = fn_scope->GetEnclosingSelfType(*meta);
      const auto declared_self_sym = declared_self != nullptr
        ? fn_scope->GetTypeSymbol(declared_self.get())
        : nullptr;
      const auto declared_on_abstract = declared_self_sym != nullptr and declared_self_sym->LinkedScope != nullptr
        and not type_members::GetUnimplementedAbstractMethods(*declared_self_sym->LinkedScope).IsEmpty();

      auto self_pin = Shared<TypeAst>(nullptr);
      if (fn_owner_type != nullptr and declared_self != nullptr
        and (SignatureNamesSelf(fn_proto) or declared_on_abstract)) {
        auto receiver = fn_owner_type->WithConvention(nullptr);

        // "Self" only stands for the receiver when the receiver really is an implementer of the class the method was
        // declared on - this is what stops a forwarding type triggering it.
        const auto receiver_sym = sm->CurrentScope->GetTypeSymbol(receiver->WithoutGenerics().get());
        const auto receiver_implements_declarer = receiver_sym != nullptr and receiver_sym->LinkedScope != nullptr
          and genex::any_of(receiver_sym->LinkedScope->SupTypes(), [&](auto const &sup) {
            return TypeEq(*declared_self, *sup, *fn_scope, *receiver_sym->LinkedScope);
          });

        if (receiver_implements_declarer and not TypeEq(*declared_self, *receiver, *fn_scope, *sm->CurrentScope)) {
          if (not receiver->IsSelfType() and not receiver_sym->IsTypeGeneric()) {
            self_pin = receiver;
          }
          auto self_arg = Vec<Unique<GenericArgumentAst>>();
          self_arg.EmplaceBack(GenericArgumentAst::NewType(
            generate::common_types::SelfType(0), std::move(receiver)));
          gn_args.MergeGenerics(std::move(self_arg));
        }
      }
      return self_pin;
    }

    auto InferAllGenerics(
      FunctionPrototypeAst const &fn_proto,
      FunctionParameterGroupAst const &fn_params,
      FunctionCallArgumentGroupAst &fn_args,
      GenericArgumentGroupAst &gn_args,
      const bool is_variadic_fn,
      Scope const *fn_scope,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      //
      using generic_bindings::EnforceGenericConstraintsAllArgs;
      using generic_bindings::InferGnArgs;
      using func_utils::NameFnArgs;

      // Name the positional function arguments. The generic arguments
      // were named by the caller, which has to do it before it merges
      // the owner's and the "sup" block's arguments in.
      NameFnArgs(
        fn_args, fn_params, *sm, meta, gn_args.GetAllArgs(),
        const_cast<Scope*>(fn_scope));

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
      FunctionPrototypeAst const &fn_proto,
      Scope const *fn_scope)
      -> void {
      auto &params = fn_proto.FnParamGroup->Params;
      for (auto i = params.Len(); i > 0uz; --i) {
        if (type_predicates::IsTypeVoid(TypeRef::OfHead(*params[i - 1uz]->Type, *fn_scope), *fn_scope)) {
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
      FunctionParameterGroupAst const &func_params,
      Vec<Shared<IdentifierAst>> const &func_param_names,
      Vec<Shared<IdentifierAst>> const &func_param_names_req,
      Vec<IdentifierAst*> const &func_arg_names,
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      ScopeManager const *sm)
      -> void {
      using errors::SppArgumentNameInvalidError;
      using errors::SppArgumentMissingError;

      const auto invalid_args = func_arg_names
        | genex::views::not_in(func_param_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

      const auto param_ctx = func_params.Params.IsEmpty()
        ? static_cast<Ast const*>(&func_params)
        : static_cast<Ast const*>(func_params.Params[0].get());
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
      PostfixExpressionOperatorFunctionCallAst const &fn_call,
      FunctionPrototypeAst const &fn_proto,
      Scope const *fn_scope,
      FunctionCallArgumentGroupAst const &func_args,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      //
      using errors::SppArgumentNameInvalidError;
      using errors::SppArgumentMissingError;
      using errors::SppTypeMismatchError;
      using type_compare::TypeEq;
      using type_compare::RelaxedTypeEq;

      StripVoidParams(fn_proto, fn_scope);
      const auto arg_is_void = [&](FunctionCallArgumentKeywordAst const *a) {
        const auto kind_sym = a->Val->InferTypeRef(sm, meta).KindSym();
        return kind_sym != nullptr and type_predicates::IsTypeVoid(*kind_sym, *sm->CurrentScope);
      };

      // Recreate the lists of function parameters, and their
      // names ("Void" removed, generics etc).
      const auto func_params = fn_proto.FnParamGroup.get();
      const auto func_param_names = fn_proto.FnParamGroup->Params
        | genex::views::transform([](auto &&x) { return x->ExtractName(); })
        | genex::to<Vec>();
      auto func_param_names_req = fn_proto.FnParamGroup->GetRequiredParams()
        | genex::views::transform([](auto &&x) { return x->ExtractName(); })
        | genex::to<Vec>();

      // "self" is required too: a runtime call injects it, but
      // "Type::method()" has to pass it, or there is no receiver.
      if (const auto self_param = fn_proto.FnParamGroup->GetSelfParam(); self_param != nullptr) {
        func_param_names_req.Insert(func_param_names_req.begin(), self_param->ExtractName());
      }
      auto func_arg_names = Vec<IdentifierAst*>();
      for (auto const &x : func_args.GetKeywordArgs()) {
        if (not arg_is_void(x)) { func_arg_names.EmplaceBack(x->Name.get()); }
      }

      CheckArgNamesAgainstParams(
        *func_params, func_param_names, func_param_names_req, func_arg_names, fn_call, sm);

      // Type check the arguments against the parameters. Sort
      // the arguments into parameter order first.
      auto sorted_func_arguments = Vec<FunctionCallArgumentKeywordAst*>();
      for (auto const &x : func_args.GetKeywordArgs()) {
        if (not arg_is_void(x)) { sorted_func_arguments.EmplaceBack(x); }
      }
      genex::actions::sort(
        sorted_func_arguments,
        {}, [&](FunctionCallArgumentKeywordAst *arg) {
          return genex::position(func_param_names, [&arg](auto const &param) { return *arg->Name == *param; });
        });

      for (auto [arg, param] : genex::views::zip(sorted_func_arguments, func_params->GetAllParams())) {
        // A "self" parameter carries no type check of its own:
        // the receiver is what chose this overload to begin with,
        // so there is nothing left to compare it against. It only
        // needs the convention the prototype declares.
        if (const auto self_param = param->To<FunctionParameterSelfAst>(); self_param != nullptr) {
          // The prototype's convention is cloned, so its tokens
          // sit in the prototype's file; they are placed on the
          // receiver the call was made through instead.
          arg->Conv = AstClone(self_param->Conv);
          if (auto *const m = arg->Conv != nullptr ? arg->Conv->To<ConventionMutAst>() : nullptr) {
            m->TokBorrow->PatchPos(arg->Val->PosStart());
            m->TokMut->PatchPos(arg->Val->PosStart());
          }
          else if (auto *const r = arg->Conv != nullptr ? arg->Conv->To<ConventionRefAst>() : nullptr) {
            r->TokBorrow->PatchPos(arg->Val->PosStart());
          }
          continue;
        }

        auto p_type = fn_scope->GetTypeSymbol(param->Type.get())->FqName()->WithConvention(
          AstClone(param->Type->GetConvention()));
        if (type_predicates::NamesSelfType(*p_type)) {
          // "Self" is the type the function belongs to. Taking it from the call-site receiver is right when the
          // receiver is that type, and wrong when the method was reached by forwarding: "&Str" calling "StrView::eq"
          // bound "Self" in "that: &Self" to "Str", and since a bare "Self" is relaxed-matched against anything, the
          // argument was accepted un-forwarded and the callee read a "Str" through "StrView"'s "{ptr, length}" shape.
          // That is why "Str == Str" was false for equal strings.
          //
          // So the owning type wins whenever the receiver only reaches it by forwarding, and the receiver wins
          // otherwise - "Self" on an abstract sup block is meant to be the concrete receiver, not the block's class.
          // Guarded throughout: a call whose left-hand side is not a postfix-with-type has no receiver at all, and
          // used to crash here rather than failing the candidate.
          const auto receiver = ReceiverTypeAtCallSite(meta);
          const auto conv = p_type->GetConvention();
          const auto owner = fn_scope->GetEnclosingSelfType(*meta);

          const auto owner_known = owner != nullptr and not owner->IsSelfType();
          const auto reached_by_forwarding = owner_known and receiver != nullptr and conv != nullptr
            and not TypeEq(*owner, *receiver, *fn_scope, *sm->CurrentScope)
            and type_compare::TypeFwdEq(
              *receiver->WithConvention(AstClone(conv)),
              *owner->WithConvention(AstClone(conv)),
              *sm->CurrentScope, *fn_scope);

          if (reached_by_forwarding or (owner_known and receiver == nullptr)) {
            p_type = type_utils::SubstituteSelfTypeAndAnalyse(*p_type->WithoutConvention(), *fn_scope, *sm, *meta)
              ->WithConvention(AstClone(conv));
          }
          else if (receiver != nullptr) {
            p_type = type_utils::SubstituteSelfTypeWith(*p_type->WithoutConvention(), *receiver)
              ->WithConvention(AstClone(conv));
          }
        }

        auto a_type = arg->InferType(sm, meta);

        if (const auto variadic_param = param->To<FunctionParameterVariadicAst>(); variadic_param != nullptr) {
          const auto variadic_gn_param = fn_proto.GetNonGenericImpl()->GnParamGroup->GetVariadicParams();
          const auto orig_name = dynamic_shared_cast<TypeIdentifierAst>(variadic_param->Source.OriginalType);
          const auto is_variadic_generic_type = variadic_gn_param != nullptr
            and orig_name != nullptr
            and *orig_name == *dynamic_shared_cast<TypeIdentifierAst>(variadic_gn_param->Name);

          if (not is_variadic_generic_type) {
            auto ts = Vec(a_type->LastTypePart()->GnArgGroup->Args.Len(), p_type);
            p_type = generate::common_types::TupleType(param->PosStart(), std::move(ts));
            p_type->Stage7_AnalyseSemantics(sm, meta);
          }
        }

        // An argument satisfies its parameter either outright,
        // or by binding a generic the call is free to choose.
        if (not type_compare::ConventionEq(*p_type, *a_type)
          or not TypeEq(*p_type, *a_type, *fn_scope, *sm->CurrentScope)) {
          // Operands go argument-first here, which is the order
          // "RelaxedTypeEq" infers the parameter's generics from
          // the argument in rather than the other way round; its
          // internal convention check is inverted to match.
          // A parameter whose generic is already fixed by a scope
          // enclosing the caller is not free to choose, so it
          // has to match exactly; and a relaxed match that only
          // held by binding such a generic is not a match either.
          auto inferred = type_compare::GenericInferenceMap();
          const auto relaxed_matched = type_compare::ConventionEq(*p_type, *a_type)
            and RelaxedTypeEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope, inferred);

          // Forwarding is the last resort, tried only once the
          // argument has failed to match any other way - so it
          // never displaces a relaxed match that would have bound
          // the parameter's generics correctly.
          if (not relaxed_matched and type_compare::TypeFwdEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope)) {
            const auto a_ref = TypeRef::Of(*a_type, *sm->CurrentScope);
            if (auto fwd_call = type_utils::BuildFwdCall(*arg->Val, a_ref, sm, meta); fwd_call != nullptr) {
              arg->Val = std::move(fwd_call);
              continue;
            }
          }

          RaiseIf<SppTypeMismatchError>(
            not relaxed_matched, {fn_scope, sm->CurrentScope}, ERR_ARGS(*param, *p_type, *arg, *a_type));
          continue;
        }

        // The types matched, and may still have matched by forwarding ("&Vec[T]" satisfying a "&View[T]" parameter),
        // in which case what the callee is handed is the forwarded-to value, so the argument becomes that call. This
        // is the argument-position counterpart of a method being called on the value its receiver forwards to. An
        // argument the relaxed match accepted never reaches here; "Self" picking the owner when the receiver only
        // forwards to it (above) is what keeps "Str == Str" from reading a "&Str" as a "StrView". Trying the forward
        // before the relaxed match instead segfaults a third of the suite.
        if (type_compare::TypeFwdEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope)) {
          const auto a_ref = TypeRef::Of(*a_type, *sm->CurrentScope);
          if (auto fwd_call = type_utils::BuildFwdCall(*arg->Val, a_ref, sm, meta); fwd_call != nullptr) {
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
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> void {
      using type_compare::TypeEq;
      using type_utils::SubstituteSelfTypeAndAnalyse;

      if (meta->ReturnTypeOverloadResolverType == nullptr) { return; }

      auto return_matches = Vec<PassedOverload>();
      for (auto &&matched : pass_overloads) {
        auto ret = AstCloneShared(matched.Proto->ReturnType);
        auto tm = ScopeManager(sm->GlobalScope, const_cast<Scope*>(matched.FnScope));
        ret = SubstituteSelfTypeAndAnalyse(*ret, *matched.FnScope, tm, *meta);

        const auto ret_ref = TypeRef::Of(*ret, *matched.FnScope);
        if (TypeEq(ret_ref, *meta->ReturnTypeOverloadResolverType, *matched.FnScope, *sm->CurrentScope)) {
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
     * The scope a prototype's generic parameters are looked up from: its own function scope, which declares its own
     * and reaches the ones inherited from its "sup" block through its ancestors.
     * @param fn_proto The prototype.
     * @param fallback The scope to use when the prototype has no scope of its own.
     * @return The scope to look the parameters up from.
     */
    auto CalleeScope(
      FunctionPrototypeAst const &fn_proto,
      Scope const &fallback)
      -> Scope const& {
      const auto *own = fn_proto.GetAstScope();
      return own != nullptr ? *own : fallback;
    }

    /**
     * Rewrite each generic argument to what it actually names at the call site, then drop the ones that only restate
     * their own parameter. A type argument naming a bound generic becomes the type it is bound to; a comp argument
     * naming a bound comp generic becomes the value read back off its symbol. An unbound parameter names nothing yet
     * and is left alone, which is what keeps a template's signature written in its own terms.
     * @param combined_generics The argument group to normalise, rewritten in place.
     * @param callee_scope The scope the callee's generic parameters are looked up from.
     * @param sm The scope manager, positioned at the call site.
     */
    auto NormaliseGenericArgs(
      GenericArgumentGroupAst &combined_generics,
      Scope const &callee_scope,
      ScopeManager const *sm)
      -> void {
      // Bound generics are written as what they are bound to, as a class instantiation's arguments are.
      monomorphization_utils::CanonicaliseGenericArgs(combined_generics, *sm->CurrentScope);

      // Drop the arguments that name the very parameter they bind - a call made inside the generic context declaring
      // it, as when one method of a generic "sup" block calls another. What is left is what this instantiation actually
      // pins, and if that is nothing then there is no instantiation to make. Only the same declaration counts, which a
      // parameter's "ParamId" identifies (a binding records the one it binds): another context's
      // parameter of the same name is another type, and gets an instantiation of its own.
      const auto names_own_param = [&](auto const &a) {
        if (a->Name != nullptr and a->TypeVal != nullptr) {
          const auto param_sym = callee_scope.GetTypeSymbol(a->Name.get());
          const auto val_sym = sm->CurrentScope->GetTypeSymbol(a->TypeVal.get());
          return param_sym != nullptr and val_sym != nullptr and param_sym->ParamId != 0
            and (val_sym->ParamId == param_sym->ParamId or val_sym->BindsParamId == param_sym->ParamId);
        }
        if (a->Name != nullptr and a->CompVal != nullptr) {
          const auto val_ident = a->CompVal->template To<IdentifierAst>();
          if (val_ident == nullptr) { return false; }
          const auto param_sym = callee_scope.GetVarSymbol(IdentifierAst::FromType(*a->Name).get());
          return param_sym != nullptr and param_sym == sm->CurrentScope->GetVarSymbol(val_ident);
        }
        return false;
      };
      combined_generics.Args |= genex::actions::remove_if(names_own_param);
    }

    auto PotentiallyGenerateGenericSubstitutedPrototype(
      FunctionPrototypeAst *fn_proto,
      Scope const *fn_scope,
      GenericArgumentGroupAst &combined_generics,
      Shared<TypeAst> const &variadic_pack_type,
      ScopeManager *sm,
      meta::CompilerMetaData *meta)
      -> Tup<FunctionPrototypeAst*, Scope const*> {
      //
      using errors::SppSecondClassBorrowViolationError;
      using monomorphization_utils::CreateGenericFunScope;
      using type_predicates::IsTypeBorrowed;

      // Inference has already produced a binding for every one of this prototype's generic parameters, including the
      // ones it inherited from the enclosing "sup" block.
      NormaliseGenericArgs(combined_generics, CalleeScope(*fn_proto, *fn_scope), sm);

      // Stamp comp arguments naming a comp parameter with that parameter, where they are written. The instantiation
      // below binds the callee's own generics in the same table, and a caller's "w" read there by name would be the
      // callee's inherited "w" - "U32::from(SizedIntegerUnsigned[w]::from(..))" inside BigUInt's "sup [cmp w: U32]".
      for (auto const *arg : combined_generics.GetCompArgs()) {
        if (arg->CompVal != nullptr) { cmp_utils::StampCompGenerics(*arg->CompVal, *sm->CurrentScope); }
      }

      // Separate variadic instantiation by the types going into the
      // variadic function parameter.
      if (variadic_pack_type != nullptr) {
        auto pack_name = MakeUnique<TypeIdentifierAst>(
          variadic_pack_type->PosStart(),
          "VariadicPackOf" + fn_proto->FnParamGroup->GetVariadicParams()->ExtractName()->Val,
          nullptr);
        combined_generics.Args.EmplaceBack(GenericArgumentAst::NewType(
          std::move(pack_name), AstClone(variadic_pack_type)));
      }

      // Consider if we need to create a generic substituted
      // function prototype.
      if (not combined_generics.Args.IsEmpty()) {
        // Reuse the instantiation for arguments of this identity if one already exists - what they resolve to from the
        // call site, not how they are spelled.
        const auto identity = sm->CurrentScope->InstanceIdentityKey(combined_generics.GetAllArgs());
        if (auto [existing_scope, existing_proto] = fn_proto->FindGenericSubstitution(identity);
          existing_proto != nullptr) {
          return {existing_proto, existing_scope};
        }

        auto new_fn_proto = AstClone(fn_proto);
        new_fn_proto->SetNonGenericImpl(fn_proto);
        new_fn_proto->DetachLlvmFuncSlot();

        // Create the new function scope for the generic implementation.
        const auto new_fn_scope = CreateGenericFunScope(
          *fn_scope, GenericArgumentGroupAst(nullptr, AstCloneVec(combined_generics.Args), nullptr), sm, meta);
        auto tm = ScopeManager(sm->GlobalScope, new_fn_scope);

        // Drop only the parameters this substitution actually bound.
        new_fn_proto->GnParamGroup->Params |= genex::actions::remove_if([&](auto const &param) {
          return genex::any_of(combined_generics.Args, [&](auto const &arg) {
            return arg->ViewName() == param->Name->ToString();
          });
        });

        auto &generic_sub_slot = AstBody(
          fn_scope->AstNode)[0]->To<FunctionPrototypeAst>()->RegisteredGenericSubstitutions().back();
        generic_sub_slot.IdentityKey = identity;

        // Substitute and analyse the function parameters and return
        // type.
        for (auto *p : new_fn_proto->FnParamGroup->GetNonSelfParams()) {
          p->Type = p->Type->SubstituteGenerics(combined_generics.GetAllArgs());
          p->Type->Stage7_AnalyseSemantics(&tm, meta);

          // The default is checked against the substituted type, so it is rebuilt from what was written, in the same
          // terms. The template's copy carries the template's analysis: "Wrap[T]::new()" would still return "Wrap[T=T]".
          if (auto *const opt = p->To<FunctionParameterOptionalAst>(); opt != nullptr and opt->Source.OriginalDefaultVal != nullptr) {
            opt->DefaultVal = AstClone(opt->Source.OriginalDefaultVal->SubstituteGenericsExpr(combined_generics.GetAllArgs()));
          }
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
  PostfixExpressionOperatorFunctionCallAst &fn_call,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Pair<PassedOverload, bool> {
  //
  using scopes::ScopeManager;
  using errors::SppFunctionCallTooManyArgumentsError;
  using type_compare::TypeEq;
  using type_utils::SubstituteSelfTypeAndAnalyse;

  auto lhs = meta->PostfixExpressionLhs;

  // Todo: Workaround for aliased variable symbols being used
  //  as function targets, due to scope lookup.
  auto temp = Shared<ExpressionAst>(nullptr);
  if (const auto id = lhs->To<IdentifierAst>()) {
    // A name declared inside the function (a function-type
    // variable) is a value being called, not a module function
    // spelled the same.
    const auto local = sm->CurrentScope->GetVarSymbol(id);
    if (local == nullptr or local->IsCompTime()) {
      const auto mod_scope = sm->CurrentScope->ParentModule();
      const auto x = mod_scope != nullptr ? mod_scope->GetVarSymbol(id) : local;
      if (x and x->IsCompTime()) {
        temp = x->FqName();
        lhs = temp.get();
      }
    }
  }

  // Extract metadata about the target function's overloads
  // such as the function's owner and scope.
  const auto [fn_owner_type, fn_owner_scope, fn_name] = GetFuncOwnerTypeAndFuncName(
    *lhs, *sm, meta);

  const auto is_postfix = meta->PostfixExpressionLhs->To<PostfixExpressionAst>();
  const auto is_runtime = is_postfix
    ? is_postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()
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
    auto fn_args = AstClone(fn_call.FnArgGroup);
    auto gn_args = AstClone(fn_call.GnArgGroup);
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
      BindBlanketAndPackGenericsFromReceiver(fn_scope, fn_owner_type.get(), *gn_args);
      gn_args->MergeGenerics(std::move(candidate.SupGenerics->Args));

      const auto self_pin = PinSelfToReceiver(
        *fn_proto, fn_scope, fn_owner_type.get(), *gn_args, sm, meta);

      InferAllGenerics(
        *fn_proto, *fn_params, *fn_args, *gn_args, is_variadic_fn, fn_scope, sm, meta);

      // Inference rebuilds the argument list from the prototype's
      // generic parameters, so the pin above - whose name is not
      // one of them - is dropped on the way out. Put it back.
      if (self_pin != nullptr) {
        auto self_arg = Vec<Unique<GenericArgumentAst>>();
        self_arg.EmplaceBack(GenericArgumentAst::NewType(
          generate::common_types::SelfType(0), std::move(self_pin)));
        gn_args->MergeGenerics(std::move(self_arg));
      }

      // "InferAllGenerics" has run "NameFnArgs", so the trailing
      // arguments of a variadic call are already collapsed into one
      // tuple-valued argument. Its type is what the callee's variadic
      // parameter actually receives.
      auto variadic_pack_type = Shared<TypeAst>(nullptr);
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

auto spp::analyse::utils::overload_utils::InstantiateOverload(
  FunctionPrototypeAst *fn_proto,
  Scope const *fn_scope,
  GenericArgumentGroupAst &generic_args,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> FunctionPrototypeAst* {
  // The arguments arrive already named - they are read off a "sup" block that has bound them - so there is nothing
  // here for inference to do, and the substitution itself is the whole of what a call site would reach.
  return std::get<0>(PotentiallyGenerateGenericSubstitutedPrototype(
    fn_proto, fn_scope, generic_args, nullptr, sm, meta));
}

auto spp::analyse::utils::overload_utils::FindInstantiatedOverload(
  FunctionPrototypeAst *fn_proto,
  GenericArgumentGroupAst &generic_args,
  ScopeManager const *sm)
  -> FunctionPrototypeAst* {
  // Nothing to substitute means the template is the only prototype there is, exactly as
  // "PotentiallyGenerateGenericSubstitutedPrototype" decides it.
  NormaliseGenericArgs(generic_args, CalleeScope(*fn_proto, *sm->CurrentScope), sm);
  if (generic_args.Args.IsEmpty()) { return fn_proto; }
  return fn_proto->FindGenericSubstitution(sm->CurrentScope->InstanceIdentityKey(generic_args.GetAllArgs())).second;
}
