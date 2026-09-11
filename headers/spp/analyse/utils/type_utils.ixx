module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct CaseExpressionBranchAst;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct GenericParameterGroupAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct PostfixExpressionAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
  SPP_EXP_CLS struct TypeStatementAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::analyse::utils::type_utils {
  // Type utilities: aliases, variants, forwarding, "Self" resolution, and lookup-or-error helpers.

  SPP_EXP_FUN auto GetFunctionalType(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Shared<const asts::TypeAst>;

  SPP_EXP_FUN auto GetGenAndYieldTypes(
    asts::TypeAst const &type,
    scopes::Scope const &scope,
    asts::ExpressionAst const &expr,
    StrView what,
    bool raise = true)
    -> Tup<Shared<const asts::TypeAst>, Shared<asts::TypeAst>, bool>;

  SPP_EXP_FUN auto GetTryType(
    asts::TypeAst const &type,
    asts::ExpressionAst const &expr,
    scopes::ScopeManager const &sm,
    StrView what,
    bool raise = true)
    -> Shared<const asts::TypeAst>;

  SPP_EXP_FUN auto GetFwdTypes(
    asts::TypeAst const &type,
    scopes::ScopeManager const &sm)
    -> Pair<Shared<asts::TypeAst>, Shared<asts::TypeAst>>;

  /**
   * Build the call that forwards a receiver to the type it forwards to, that is @code x.fwd_ref()@endcode for a type
   * superimposing @c FwdRef (or @code x.fwd_mut()@endcode for @c FwdMut). Because the forwarding coroutines return a
   * @c GenOnce, the call resumes automatically and evaluates to the borrow of the forwarded-to value, which is the
   * receiver every forwarded member access and method call actually operates on. The returned expression is fully
   * analysed, so it can be inferred from and generated like any other expression.
   * @param[in] receiver The expression that forwards, which is cloned into the built call.
   * @param[in] receiver_type The type of the receiver, whose superimpositions are searched for the forwarding marker.
   * @param[in,out] sm The scope manager to analyse the built call with.
   * @param[in,out] meta Associated metadata.
   * @return The forwarding call, or @c nullptr if the receiver's type does not forward.
   */
  SPP_EXP_FUN auto BuildFwdCall(
    asts::ExpressionAst const &receiver,
    asts::TypeAst const &receiver_type,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> Unique<asts::PostfixExpressionAst>;

  SPP_EXP_FUN auto GetTypeSymOrError(
    scopes::Scope const &scope,
    asts::TypeIdentifierAst const &type_part,
    scopes::ScopeManager const &sm)
    -> scopes::TypeSymbol*;

  SPP_EXP_FUN auto GetNsScopeOrError(
    scopes::Scope const &scope,
    asts::IdentifierAst const &ns,
    scopes::ScopeManager const &sm)
    -> scopes::Scope*;

  SPP_EXP_FUN auto RecursiveAliasSearch(
    asts::TypeStatementAst const &alias_stmt,
    bool from_use_stmt,
    scopes::Scope *tracking_scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> Tup<Shared<asts::TypeAst>, Shared<asts::GenericParameterGroupAst>, scopes::Scope*>;

  SPP_EXP_FUN auto ResolveAndSubstituteSelfType(
    asts::TypeAst const &type,
    scopes::Scope const &scope,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> Shared<asts::TypeAst>;

  /**
   * Replace every "Self" part of a written type with the type the enclosing block belongs to, without analysing the
   * result. This is what @c ResolveAndSubstituteSelfType does before it analyses, split out for the callers that run
   * before the stage a type can be analysed in - resolving an alias's target, for one, happens in stage 3.
   * @param type The written type to substitute into.
   * @param scope The scope the type was written in, which decides what "Self" names.
   * @param meta The compiler metadata, for escaping a closure scope to the scope it stands in for.
   * @param substituted Set to @c true when a "Self" was actually replaced, and left alone otherwise.
   * @return The substituted type, or a plain clone when there is no "Self" to replace or nothing to replace it with.
   */
  SPP_EXP_FUN auto SubstituteSelfType(
    asts::TypeAst const &type,
    scopes::Scope const &scope,
    asts::meta::CompilerMetaData const &meta,
    bool *substituted = nullptr)
    -> Shared<asts::TypeAst>;

  /**
   * Replace every "Self" part of a written type with a type given outright, for the callers that decide what "Self"
   * stands for themselves rather than reading it off the scope - overload resolution picks between the type owning
   * the function and the type at the call site's receiver.
   * @param type The written type to substitute into.
   * @param replacement The type every "Self" part stands for.
   * @return The substituted type, or a plain clone when there is no "Self" to replace.
   */
  SPP_EXP_FUN auto SubstituteSelfTypeWith(
    asts::TypeAst const &type,
    asts::TypeAst const &replacement)
    -> Shared<asts::TypeAst>;
}
