module;
#include <spp/macros.hpp>

export module spp.analyse.utils.expr_utils;
import spp.utils.types;
import sys;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS struct ScopeManager;
  SPP_EXP_CLS struct NamespaceSymbol;
  SPP_EXP_CLS struct VariableSymbol;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct StatementAst;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::analyse::utils::expr_utils {
  SPP_EXP_CLS struct PrimaryExpressionOptions {
    bool AllowTypeAst = false;
    bool AllowTokenAst = false;
  };

  SPP_EXP_FUN auto IsPrimaryExprTypeValid(
    asts::ExpressionAst const &expr,
    scopes::ScopeManager const &sm,
    PrimaryExpressionOptions &&options = {})
    -> bool;

  SPP_EXP_FUN auto ValidateNoUnreachableCode(
    Vec<asts::StatementAst*> const &members,
    scopes::ScopeManager const &sm)
    -> void;

  /**
   * Reject a statement whose value nothing consumes. Ownership is linear, so a produced value has to go somewhere:
   * only @c Void and @c Never - which produce nothing and never arrive respectively - may be written as a statement.
   * @param member The statement in discard position.
   * @param scope The scope @p member was written in, which its type is inferred against.
   * @param sm The scope manager, used for its global scope and for error formatting.
   * @param meta Associated metadata.
   */
  SPP_EXP_FUN auto ValidateDiscardedValue(
    asts::Ast &member,
    scopes::Scope *scope,
    scopes::ScopeManager const &sm,
    asts::meta::CompilerMetaData *meta)
    -> void;

  /**
   * One scope that declares a name, and how far it sits from the type the lookup began at. This allows for
   * disambiguation between which field to use from layers inheritance if there are duplicate field names etc.
   */
  SPP_EXP_CLS struct DeclaringVarScope {
    sys::ssize_t Depth;
    scopes::Scope *Where;
    scopes::VariableSymbol *Symbol;
  };

  /**
   * One scope that declares a type, and how far it sits from the type the lookup began at. This allows for
   * disambiguation between which field to use from layers inheritance if there are duplicate field names etc.
   */
  SPP_EXP_CLS struct DeclaringTypeScope {
    sys::ssize_t Depth;
    scopes::Scope *Where;
    scopes::TypeSymbol *Symbol;
  };

  /**
   * Every scope that contains the variable requested. These are then inter-compared for depths, to determine if there
   * are ambiguous lookups or not.
   * @param type_scope The scope of the type the access was written against.
   * @param name The member being accessed.
   * @param sup_scope_search Whether each scope may answer with what its own super-scopes declare.
   * @return The scopes holding it, unordered; pass to @c ClosestScopes to keep only the nearest.
   */
  SPP_EXP_FUN auto ScopesDeclaringVar(
    scopes::Scope &type_scope,
    asts::IdentifierAst const &name,
    bool sup_scope_search)
    -> Vec<DeclaringVarScope>;

  /**
   * Every scope that contains the type requested. These are then inter-compared for depths, to determine if there are
   * ambiguous lookups or not.
   * @param type_scope The scope of the type the access was written against.
   * @param name The member being accessed.
   * @param sup_scope_search Whether each scope may answer with what its own super-scopes declare.
   * @return The scopes holding it, unordered; pass to @c ClosestScopes to keep only the nearest.
   */
  SPP_EXP_FUN auto ScopesDeclaringType(
    scopes::Scope &type_scope,
    asts::TypeIdentifierAst const &name,
    bool sup_scope_search)
    -> Vec<DeclaringTypeScope>;

  /**
   * The candidates sitting nearest to the type the lookup began at. More than one means two superimpositions at the
   * same level declare the name, and the access cannot choose between them.
   */
  SPP_EXP_FUN auto ClosestScopes(
    Vec<DeclaringVarScope> const &candidates)
    -> Vec<DeclaringVarScope>;

  /**
   * The candidates sitting nearest to the type the lookup began at. More than one means two superimpositions at the
   * same level declare the name, and the access cannot choose between them.
   */
  SPP_EXP_FUN auto ClosestScopes(
    Vec<DeclaringTypeScope> const &candidates)
    -> Vec<DeclaringTypeScope>;

  /**
   * Report if there is an ambiguity for variable lookup.
   * @param closest The nearest candidates, (less than 2 is fine, otherwise ambiguous).
   * @param access The member access being reported.
   * @param sm The scope manager, for error formatting.
   */
  SPP_EXP_FUN auto RaiseIfAmbiguous(
    Vec<DeclaringVarScope> const &closest,
    asts::Ast const &access,
    scopes::ScopeManager const &sm)
    -> void;

  /**
   * Report if there is an ambiguity for type lookup.
   * @param closest The nearest candidates, (less than 2 is fine, otherwise ambiguous).
   * @param access The member access being reported.
   * @param sm The scope manager, for error formatting.
   */
  SPP_EXP_FUN auto RaiseIfAmbiguous(
    Vec<DeclaringTypeScope> const &closest,
    asts::Ast const &access,
    scopes::ScopeManager const &sm)
    -> void;

  SPP_EXP_FUN SPP_ATTR_COLD SPP_ATTR_NORETURN auto RaiseMissingIdentifierAndClosestOptions(
    asts::IdentifierAst const &identifier,
    Vec<scopes::VariableSymbol*> const &var_symbols,
    Vec<scopes::NamespaceSymbol*> const &ns_symbols,
    scopes::ScopeManager const &sm)
    -> void;

  SPP_EXP_FUN SPP_ATTR_COLD SPP_ATTR_NORETURN auto RaiseMissingTypeIdentifierAndClosestOptions(
    asts::TypeIdentifierAst const &identifier,
    Vec<scopes::TypeSymbol*> const &symbols,
    scopes::ScopeManager const &sm)
    -> void;
}
