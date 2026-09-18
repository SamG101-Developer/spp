module;
#include <spp/macros.hpp>

export module spp.analyse.utils.func_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionCallArgumentAst);
use(spp::asts, struct FunctionCallArgumentKeywordAst);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionParameterAst);
use(spp::asts, struct FunctionParameterGroupAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct PostfixExpressionOperatorFunctionCallAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, class ScopeManager);

namespace spp::analyse::utils::func_utils {
  /// A function overload struct contains information about
  /// the scope/proto of the function, generics being inherited
  /// into it, and a potential type-forwarding  type too.
  SPP_EXP_CLS struct FunctionOverload {
    Scope const *FnScope;
    FunctionPrototypeAst *Proto;
    Unique<GenericArgumentGroupAst> SupGenerics;
    Shared<TypeAst> FwdType;
  };

  /// A function value match struct contains information about
  /// the function prototype, the scope it is in, and the
  /// generic arguments being used.
  SPP_EXP_CLS struct FunctionValueMatch {
    FunctionPrototypeAst *Proto;
    Scope const *FnScope;
    Unique<GenericArgumentGroupAst> GenericArgs;
  };

  /// Given a function name and a scope, find all the function
  /// overloads that can be reached. This includes searching
  /// through scopes, handling generics, etc. All overloads
  /// are then processed for eligibility when calling.
  SPP_EXP_FUN auto GetAllFunctionScopes(
    IdentifierAst const &target_fn_name,
    Scope const *target_scope,
    ScopeManager &sm,
    meta::CompilerMetaData *meta)
    -> Vec<FunctionOverload>;

  /// Get the actual name of a function hidden by a $Type. For
  /// regular functions and methods, this is simply converting
  /// "$Type" back to "type". For closures, there is no name,
  /// as a closure is an unnamed function by definition, so
  /// "nullptr". The scope returned is the parent of the overload.
  SPP_EXP_FUN auto GetFunctionValueName(
    TypeRef const &type)
    -> Pair<Shared<IdentifierAst>, Scope const*>;

  /// Given a mock function type like $Type, and a genuine
  /// functional type like FunMov[(), Void], extract the match
  /// information based on what $Type superimposes. Prefer
  /// non-generic overloads, and handle generic functions by
  /// inferring of the "func_type".
  SPP_EXP_FUN auto MatchFunctionValue(
    TypeRef const &mock,
    TypeRef const &func,
    Scope const &func_scope)
    -> std::optional<FunctionValueMatch>;

  /// When we pass a function value into a functional type,
  /// ie "$Type" into "FunMov[(), Void]" (arg->param, "let",
  /// "ret", assignment) - "codegen::CoerceToFunctionValue"
  /// handles this at the codegen level. But if the overload
  /// it stands for is generic, it must be instantiated in
  /// the analysis engine earlier, so the overload is ready
  /// by codegen-time.
  SPP_EXP_FUN auto InstantiateFunctionValue(
    TypeRef const &value,
    TypeRef const &target,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void;

  /// Lookup for a generic instantiation of a function based
  /// on the function's "$Type" and the function "FunMov"
  /// type.
  SPP_EXP_FUN auto FindFunctionValue(
    TypeRef const &value,
    TypeRef const &target,
    ScopeManager const &sm)
    -> FunctionPrototypeAst*;

  /// Check whether the new function prototype conflicts with
  /// functions of the same name but different signatures with
  /// the same owner. Two "fun f(&self) -> Void" is ambiguous.
  /// Several semantic checks in place to detect ambiguities.
  SPP_EXP_FUN auto CheckForConflictingOverload(
    Scope const &this_scope,
    Scope const *target_scope,
    FunctionPrototypeAst const &new_fn,
    ScopeManager &sm,
    meta::CompilerMetaData *meta)
    -> FunctionPrototypeAst*;

  /// The core of the override checker, checking whether two
  /// functions semantically share a signature. Publicly
  /// exposed because the type_members module needs it for
  /// checking for any unimplemented abstract methods.
  SPP_EXP_FUN auto SameSignature(
    FunctionPrototypeAst const &fn_a,
    Scope const &scope_a,
    FunctionPrototypeAst const &fn_b,
    Scope const &scope_b)
    -> bool;

  /// Check whether the new function prototype is a genuine
  /// override of a method on a super type. This is used to
  /// ensure function overriding is a genuine match. It is
  /// also used to remove overridden functions from overload
  /// selection, so we get subclass's prototype override.
  SPP_EXP_FUN auto CheckForConflictingOverride(
    Scope const &this_scope,
    Scope const *target_scope,
    FunctionPrototypeAst const &new_fn,
    ScopeManager &sm,
    meta::CompilerMetaData *meta,
    Scope const *exclude_scope = nullptr)
    -> FunctionPrototypeAst*;

  /// Take a function argument group, and name the arguments
  /// based on the parameters available. Custom logic for
  /// optional and variadic parameters.
  SPP_EXP_FUN auto NameFnArgs(
    FunctionCallArgumentGroupAst &a_group,
    FunctionParameterGroupAst const &p_group,
    ScopeManager &sm,
    meta::CompilerMetaData *meta,
    Vec<GenericArgumentAst*> const &generic_args = {},
    Scope *callee_scope = nullptr)
    -> void;

  /// Check whether an expression is callable or not, by
  /// checking if the type is functional, or a symbol flag
  /// has been set via generic constraints.
  SPP_EXP_FUN auto IsTargetCallable(
    ExpressionAst &expr,
    ScopeManager &sm,
    meta::CompilerMetaData *meta)
    -> Shared<const TypeAst>;
}
