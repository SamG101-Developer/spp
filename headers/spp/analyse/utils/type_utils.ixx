module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts, struct TypeStatementAst);

namespace spp::analyse::utils::type_utils {
  /// Check the type and search the supertypes to identify a
  /// functional superimposition. Retrieve it. There is a hack
  /// here where generic constraints are considered beforehand,
  /// because if we have a FunRef that must be a FunMov by
  /// constraint, for behaviour to be consistent, it must be
  /// treated like the FunMov would be.
  SPP_EXP_FUN auto GetFunctionalType(
    TypeAst const &type,
    Scope const &scope)
    -> Shared<const TypeAst>;

  /// Check the type and search the supertypes to identifier a
  /// generator superimposition. Retrieve it along with the Yield
  /// type in the generator's generics, and if its Gen or GenOnce.
  /// Fallible with >1 generator candidates.
  SPP_EXP_FUN auto GetGenAndYieldTypes(
    TypeAst const &type,
    Scope const &scope,
    ExpressionAst const &expr,
    StrView what,
    bool raise = true)
    -> Tup<Shared<const TypeAst>, Shared<TypeAst>, bool>;

  /// Check the type and search the supertypes to identifier a
  /// try-type superimposition. Retrieve it.
  SPP_EXP_FUN auto GetTryType(
    TypeAst const &type,
    ExpressionAst const &expr,
    ScopeManager const &sm,
    StrView what,
    bool raise = true)
    -> Shared<const TypeAst>;

  /// Check the type and search the supertypes to identify a
  /// forwarding superimposition (pair). Retrieve the ref/mut
  /// forwarding target types. "Str" -> "&StrView" etc.
  SPP_EXP_FUN auto GetFwdTypes(
    TypeAst const &type,
    ScopeManager const &sm)
    -> Pair<Shared<TypeAst>, Shared<TypeAst>>;

  /// Manually build the hidden forwarding call that is
  /// abstracted over for things like member access, assignment,
  /// returning etc. This is needed to the llvm codegen can
  /// access the forwarded object.
  SPP_EXP_FUN auto BuildFwdCall(
    ExpressionAst const &receiver,
    TypeAst const &receiver_type,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Unique<PostfixExpressionAst>;

  /// Get the type symbol specified by "type_part" in the type
  /// scope "scope". If it cannot be found, raise an error.
  /// Standard type analysis.
  SPP_EXP_FUN auto GetTypeSymOrError(
    Scope const &scope,
    TypeIdentifierAst const &type_part,
    ScopeManager const &sm)
    -> TypeSymbol*;

  /// Get the scope specified by "ns" in the scope "scope". If
  /// it cannot be found, raise an error. Standard type analysis.
  SPP_EXP_FUN auto GetNsScopeOrError(
    Scope const &scope,
    IdentifierAst const &ns,
    ScopeManager const &sm)
    -> Scope*;

  /// The core alias resolver, taking a type statement ast and
  /// determining its genuine original mapped type, untangling
  /// multi-stage aliasing, generics, etc.
  SPP_EXP_FUN auto RecursiveAliasSearch(
    TypeStatementAst const &alias_stmt,
    bool from_use_stmt,
    Scope *tracking_scope,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Tup<Shared<TypeAst>, Shared<GenericParameterGroupAst>, Scope*>;

  /// Resolve the "Self" type for the scope, and do a substitution
  /// on the type to translate all the generics into the true type.
  SPP_EXP_FUN auto SubstituteSelfType(
    TypeAst const &type,
    Scope const &scope,
    meta::CompilerMetaData const &meta,
    bool *substituted = nullptr)
    -> Shared<TypeAst>;

  /// Resolve the "Self" type for the scope, and do a substitution
  /// on the type to translate all the generics into the true type.
  /// Do an analysis afterwards if a substitution actually happened.
  /// Reuses the standard self type substitution function.
  SPP_EXP_FUN auto SubstituteSelfTypeAndAnalyse(
    TypeAst const &type,
    Scope const &scope,
    ScopeManager &sm,
    meta::CompilerMetaData &meta)
    -> Shared<TypeAst>;

  /// Replace every "Self" part of a written type with a type given
  /// outright, for the callers that decide what "Self" stands for
  /// themselves rather than reading it off the scope - overload
  /// resolution picks between the type owning the function and the
  /// type at the call site's receiver.
  SPP_EXP_FUN auto SubstituteSelfTypeWith(
    TypeAst const &type,
    TypeAst const &replacement)
    -> Shared<TypeAst>;
}
