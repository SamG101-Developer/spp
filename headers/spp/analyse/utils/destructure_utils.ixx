module;
#include <spp/macros.hpp>

export module spp.analyse.utils.destructure_utils;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct LlvmCtx);

namespace spp::analyse::utils::destructure_utils {
  constexpr auto kUnmatchableTag = "_UNMATCHABLE";

  /// Extract the names of the bindings at any depth, using
  /// a transform mapping to the "ExtractNames" function.
  SPP_EXP_FUN auto GetNestedBindingIdentifiers(
    Vec<Unique<LocalVariableAst>> const &elems)
    -> Vec<Shared<IdentifierAst>>;

  /// Create an unmatchable identifier at the given position.
  /// This just wraps the "kUnmatchableTag" into a shared
  /// pointer.
  SPP_EXP_FUN auto UnmatchableSingleIdentifier(
    std::size_t pos)
    -> Shared<IdentifierAst>;

  /// Whether the expression holds "destructure-able" storage
  /// or not. Typically, if not, then a materialization occurs.
  SPP_EXP_FUN auto IsDestructurePlaceExpression(
    ExpressionAst const &expr)
    -> bool;

  /// When the value being destructured doesn't name any storage,
  /// then materialize it and take parts of the materialization,
  /// otherwise we end up cloning the temporary and breaking
  /// lots of analysis.
  SPP_EXP_FUN auto BindDestructureTemporary(
    Ast const &owner,
    ExpressionAst *val,
    Shared<TypeAst> const &val_type,
    ScopeManager &sm)
    -> Shared<IdentifierAst>;

  /// When we have a pattern like "let Self(fd) = self", we
  /// mark "self" as moved, because all non-copyable fields
  /// have to be moved (for linear system drop rules), and
  /// this is how in the "drop" methods, we finish consuming
  /// "self".
  SPP_EXP_FUN auto ConsumeDestructureSource(
    Ast const &owner,
    bool from_case_pattern,
    bool any_binding_is_moving,
    ScopeManager &sm,
    CompilerMetaData const *meta)
    -> void;

  /// When we have a temporary materialization, mark it as
  /// consumed by getting the symbol and setting the memory
  /// fields on it to mark as "moved".
  SPP_EXP_FUN auto ConsumeDestructureTemp(
    IdentifierAst const &tmp_name,
    ScopeManager const &sm)
    -> void;

  /// Run uniform stage 8 memory analysis on the destructure
  /// temporary materialization, should it exist (this won't
  /// be called if not).
  SPP_EXP_FUN auto DestructureTempStage8(
    Ast const &owner,
    IdentifierAst const &tmp_name,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> void;

  /// Run uniform stage 9 comptime resolution on the
  /// destructure temporary materialization, should it exist
  /// (this won't be called if not).
  SPP_EXP_FUN auto DestructureTempStage9(
    Shared<IdentifierAst> const &tmp_name,
    ScopeManager const &sm,
    CompilerMetaData const *meta)
    -> void;

  /// Run uniform stage 11 code generation on the destructure
  /// temporary materialization, should it exist (this won't
  /// be called if not). The value bound to the destructure's
  /// hidden temporary is generated into the stack before the
  /// expanded "let" statements use it.
  SPP_EXP_FUN auto DestructureTempStage11(
    Shared<IdentifierAst> const &tmp_name,
    llvm::Value *llvm_subject,
    ScopeManager &sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;
}
