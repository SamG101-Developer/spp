module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
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
  /// generator superimposition. Retrieve its symbol along with the
  /// Yield type in the generator's generics, and if its Gen or
  /// GenOnce. Fallible with >1 generator candidates. The errors
  /// are placed on "expr" and print the type "spell" gives, which
  /// is only asked for when one is raised.
  SPP_EXP_FUN auto GetGenAndYieldTypes(
    TypeRef const &ref,
    Scope const &scope,
    ExpressionAst const &expr,
    std::function<Shared<TypeAst>()> const &spell,
    StrView what,
    bool raise = true)
    -> Tup<TypeSymbol*, Shared<TypeAst>, bool>;

  /// Check the type and search the supertypes to identifier a
  /// try-type superimposition. Retrieve its symbol, whose "Value"
  /// and "Residual" arguments are what an early return reads. The
  /// errors print the type "spell" gives, as above.
  SPP_EXP_FUN auto GetTryType(
    TypeRef const &ref,
    ExpressionAst const &expr,
    std::function<Shared<TypeAst>()> const &spell,
    ScopeManager const &sm,
    StrView what,
    bool raise = true)
    -> TypeSymbol*;

  /// Check the type and search the supertypes to identify a
  /// forwarding superimposition (pair). Retrieve the ref/mut
  /// forwarding super classes ("FwdRef[T=StrView]" for "Str"),
  /// whose "T" is what is forwarded to.
  SPP_EXP_FUN auto GetFwdTypes(
    TypeSymbol const &sym,
    Scope const &scope)
    -> Pair<TypeSymbol*, TypeSymbol*>;

  /// Manually build the hidden forwarding call that is
  /// abstracted over for things like member access, assignment,
  /// returning etc. This is needed to the llvm codegen can
  /// access the forwarded object.
  SPP_EXP_FUN auto BuildFwdCall(
    ExpressionAst const &receiver,
    TypeRef const &receiver_ref,
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
  /// Do an analysis afterwards if a substitution actually happened,
  /// which "substituted" reports.
  SPP_EXP_FUN auto SubstituteSelfTypeAndAnalyse(
    TypeAst const &type,
    Scope const &scope,
    ScopeManager &sm,
    meta::CompilerMetaData &meta,
    bool *substituted = nullptr)
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

  /// Whether "ResolveWrittenType" replaces "Self" from the scope, or
  /// leaves it for a caller that decides per use what it stands for
  /// (function parameter and return types).
  SPP_EXP_CLS enum class SelfPolicy { kSubstitute, kKeep };

  /// Resolve a type as written in source: substitute "Self", analyse,
  /// qualify through its symbol, then restore the written convention
  /// and source span. A type that had "Self" replaced is analysed with
  /// abstract types allowed, as "Self" may name an abstract class.
  SPP_EXP_FUN auto ResolveWrittenType(
    TypeAst const &written,
    ScopeManager &sm,
    meta::CompilerMetaData &meta,
    SelfPolicy self = SelfPolicy::kSubstitute)
    -> Shared<TypeAst>;

  /// Stamp every name in a type whose meaning is fixed with
  /// what it means in the scope the type is written in: a
  /// generic parameter, a closed class, and the template at
  /// the head of a name written with arguments. A copy read
  /// again from any other scope then resolves through the
  /// stamps ("Scope::Canon"), not by its spelling.
  SPP_EXP_FUN auto StampWrittenParts(
    TypeAst const &type,
    Scope const &scope)
    -> void;
}
