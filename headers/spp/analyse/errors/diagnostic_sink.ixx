module;
#include <spp/macros.hpp>

export module spp.analyse.errors.diagnostic_sink;
import spp.analyse.errors.semantic_error;
import spp.utils.types;
import std;

use(spp::asts, struct Ast);

/// A set of functions for storing and retrieving multiple found
/// errors that will be rendered into the IDEA plugin.
namespace spp::analyse::errors::diagnostic_sink {
  /// Collect errors rather than letting them out, for the member
  /// loops that know how to carry on.
  SPP_EXP_FUN auto Enable(bool enabled) -> void;

  /// Whether the member loops should catch. False everywhere but
  /// an editor's compile.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto IsEnabled() -> bool;

  /// Keep an error that a member loop caught, and mark the member
  /// it came from.
  SPP_EXP_FUN auto Report(SemanticError const &error, Ast const *member) -> void;

  /// Whether this member has already failed, and so holds half an
  /// analysis that the stages after it must not read.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto IsPoisoned(Ast const *member) -> bool;

  /// Every error collected so far, in the order they were raised.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto Collected() -> Vec<SemanticError> const&;

  /// Whether anything was collected - what decides that no artefact
  /// is produced.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto HasErrors() -> bool;

  /// Forget everything, for the next compile in this process.
  SPP_EXP_FUN auto Clear() -> void;
}
