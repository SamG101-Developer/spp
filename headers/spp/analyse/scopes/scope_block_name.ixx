module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_block_name;
import spp.utils.types;
import std;

use(spp::analyse::scopes, struct ScopeBlockName);
use(spp::analyse::scopes, struct ScopeIdentifierName);
use(spp::analyse::scopes, struct ScopeTypeIdentifierName);
use(spp::asts, struct Ast);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

/// A scope block name wraps the a string into a struct, such
/// as "case" or "loop" for specific asts. This differentiates
/// them from function/module/type scopes.
SPP_EXP_CLS struct spp::analyse::scopes::ScopeBlockName {
  /// The internal string name of the scope being created.
  /// This will look like "loop#30" for a loop ast starting
  /// at token 30. The actual token number isn't important.
  Str Name;

  /// Take a "header" name, like "loop" or "case", a "parts"
  /// vector, for additional identifying metadata, and a token
  /// position. These are combined into one name, calling the
  /// private constructor. Often, the parts vector is empty.
  static auto FromParts(Str &&header, Vec<Ast*> const &parts, std::size_t pos) -> ScopeBlockName;

  ScopeBlockName(ScopeBlockName const &) = default;
  ScopeBlockName(ScopeBlockName &&) noexcept = default;

private:
  explicit ScopeBlockName(Str &&name);
};

/// The module and function scopes use their associated
/// identifier ast to name their scope.
SPP_EXP_CLS struct spp::analyse::scopes::ScopeIdentifierName {
  /// The internal identifier name, shared with the module
  /// or function prototype ast this scope represents.
  Shared<IdentifierAst> Name;

  explicit ScopeIdentifierName(Shared<IdentifierAst> const &name);
};

/// The class scopes using a type ast representing the
/// type's name to name their scope.
SPP_EXP_CLS struct spp::analyse::scopes::ScopeTypeIdentifierName {
  /// The internal identifier name, shared with the
  /// class prototype ast this scope represents.
  Shared<TypeIdentifierAst> Name;

  explicit ScopeTypeIdentifierName(Shared<TypeAst> const &name);
  explicit ScopeTypeIdentifierName(Shared<TypeIdentifierAst> const &name);
};
