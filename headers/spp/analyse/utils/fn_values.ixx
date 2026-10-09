module;
#include <spp/macros.hpp>

export module spp.analyse.utils.fn_values;
import spp.analyse.scopes.substitution;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct SupPrototypeExtensionAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::fn_values {
  /// A function value match struct contains information about
  /// the function prototype, the scope it is in, and the
  /// generic arguments being used.
  SPP_EXP_CLS struct FnValueMatch {
    FunctionPrototypeAst *Proto;
    Scope const *FnScope;
    scopes::GenericSubst Bindings;
  };

  /// The "sup $F ext FunXXX { ... }" block an overload was
  /// lowered to, and the overload; or nullptrs.
  SPP_EXP_FUN auto FnBlockOf(
    Scope const &scope) -> Pair<SupPrototypeExtensionAst*, FunctionPrototypeAst*>;

  /// Get the actual name of a function hidden by a $Type. For
  /// regular functions and methods, this is simply converting
  /// "$Type" back to "type". For closures, there is no name,
  /// as a closure is an unnamed function by definition, so
  /// "nullptr". The scope returned is the parent of the overload.
  SPP_EXP_FUN auto GetFnValueName(
    TypeRef const &type) -> Pair<Shared<IdentifierAst>, Scope const*>;

  /// Given a mock function type like $Type, and a genuine
  /// functional type like FunMov[(), Void], extract the match
  /// information based on what $Type superimposes. Prefer
  /// non-generic overloads, and handle generic functions by
  /// inferring of the "func_type".
  SPP_EXP_FUN auto MatchFnValue(
    TypeRef const &mock, TypeRef const &func,
    Scope const &func_scope) -> std::optional<FnValueMatch>;

  /// When we pass a function value into a functional type,
  /// ie "$Type" into "FunMov[(), Void]" (arg->param, "let",
  /// "ret", assignment) - "codegen::CoerceToFnValue"
  /// handles this at the codegen level. But if the overload
  /// it stands for is generic, it must be instantiated in
  /// the analysis engine earlier, so the overload is ready
  /// by codegen-time.
  SPP_EXP_FUN auto InstantiateFnValue(
    TypeRef const &value, TypeRef const &target,
    ScopeManager *sm, meta::CompilerMetaData *meta) -> void;

  /// Lookup for a generic instantiation of a function based
  /// on the function's "$Type" and the function "FunMov"
  /// type.
  SPP_EXP_FUN auto FindFnValue(
    TypeRef const &value, TypeRef const &target,
    ScopeManager const &sm) -> FunctionPrototypeAst*;

  /// Check whether the new function prototype conflicts with
  /// functions of the same name but different signatures with
  /// the same owner. Two "fun f(&self) -> Void" is ambiguous.
  /// Several semantic checks in place to detect ambiguities.
  /// The conflicting prototype and the scope it is declared in
  /// (another module's, for a "sup" block written elsewhere);
  /// both null for no conflict.
  SPP_EXP_FUN auto CheckForConflictingOverload(
    Scope const &this_scope, Scope const *target_scope,
    FunctionPrototypeAst const &new_fn, ScopeManager &sm,
    meta::CompilerMetaData *meta) -> Pair<FunctionPrototypeAst*, Scope const*>;

  /// The core of the override checker, checking whether "fn_a"
  /// (the override or implementation) has "fn_b"'s signature: the
  /// same parameters, and a return type that may narrow "fn_b"'s.
  /// Publicly exposed because the type_members module needs it for
  /// checking for any unimplemented abstract methods.
  SPP_EXP_FUN auto SameSignature(
    FunctionPrototypeAst const &fn_a, Scope const &scope_a,
    FunctionPrototypeAst const &fn_b, Scope const &scope_b) -> bool;

  /// Check whether the new function prototype is a genuine
  /// override of a method on a super type. This is used to
  /// ensure function overriding is a genuine match. It is
  /// also used to remove overridden functions from overload
  /// selection, so we get subclass's prototype override.
  SPP_EXP_FUN auto CheckForConflictingOverride(
    Scope const &this_scope, Scope const *target_scope, FunctionPrototypeAst const &new_fn,
    ScopeManager &sm, meta::CompilerMetaData *meta, Scope const *exclude_scope = nullptr)
    -> FunctionPrototypeAst*;

  /// Check whether an expression is callable or not, by
  /// checking if the type is functional, or a symbol flag
  /// has been set via generic constraints.
  SPP_EXP_FUN auto IsTargetCallable(
    ExpressionAst &expr, ScopeManager &sm, meta::CompilerMetaData *meta) -> Shared<const TypeAst>;
}
