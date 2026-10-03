module;
#include <spp/macros.hpp>

export module spp.analyse.utils.destructure_utils;
import spp.utils.ptr;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LetStatementInitializedAst);
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

  /// When the value being destructured doesn't name any storage,
  /// then materialize it and take parts of the materialization,
  /// otherwise we end up cloning the temporary and breaking
  /// lots of analysis.
  SPP_EXP_FUN auto BindDestructureTemporary(
    ExpressionAst *val,
    Shared<TypeAst> const &val_type,
    ScopeManager &sm)
    -> Shared<IdentifierAst>;

  /// What an array or tuple destructure ("let [a, b] = x", "let (a, ..r) = t") supplies to
  /// "DestructureSequenceStage7": the only parts the two kinds do differently.
  SPP_EXP_CLS struct SequenceShape {
    /// Check the value is of the kind (raising the kind's own error), and answer its element count.
    std::function<std::size_t(ExpressionAst const &val, Shared<TypeAst> const &val_type)> CheckAndCount;

    /// Raise the kind's size-mismatch error: "lhs" elements written against a value of "rhs".
    std::function<void(std::size_t lhs, ExpressionAst const &val, std::size_t rhs)> RaiseSizeMismatch;

    /// The literal a bound ".." collects its elements into.
    std::function<Unique<ExpressionAst>(Vec<Unique<ExpressionAst>> &&elems)> MakeRest;
  };

  /// Stage 7 of an array or tuple destructure: at most one "..", the value of the destructure's kind and size, bound
  /// to a hidden temporary unless it names a place ("tmp_name"), then one "let" per element over its index of it (a
  /// bound ".." taking the elements it skips, a skip taking none), analysed and kept in "new_asts".
  SPP_EXP_FUN auto DestructureSequenceStage7(
    LocalVariableAst const &self,
    Vec<Unique<LocalVariableAst>> const &elems,
    SequenceShape const &shape,
    Shared<IdentifierAst> &tmp_name,
    Vec<Unique<LetStatementInitializedAst>> &new_asts,
    bool from_case_pattern,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void;

  /// When we have a pattern like "let Self(fd) = self", we
  /// mark "self" as moved, because all non-copyable fields
  /// have to be moved (for linear system drop rules), and
  /// this is how in the "drop" methods, we finish consuming
  /// "self".
  auto ConsumeDestructureSource(
    Ast const &owner,
    bool from_case_pattern,
    bool any_binding_is_moving,
    ScopeManager &sm,
    CompilerMetaData const *meta)
    -> void;

  /// When we have a temporary materialization, mark it as
  /// consumed by getting the symbol and setting the memory
  /// fields on it to mark as "moved".
  auto ConsumeDestructureTemp(
    IdentifierAst const &tmp_name,
    ScopeManager const &sm)
    -> void;

  /// Run uniform stage 8 memory analysis on the destructure
  /// temporary materialization, should it exist (this won't
  /// be called if not).
  auto DestructureTempStage8(
    Ast const &owner,
    IdentifierAst const &tmp_name,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> void;

  /// Run uniform stage 9 comptime resolution on the
  /// destructure temporary materialization, should it exist
  /// (this won't be called if not).
  auto DestructureTempStage9(
    Shared<IdentifierAst> const &tmp_name,
    ScopeManager const &sm,
    CompilerMetaData const *meta)
    -> void;

  /// Stage 8 for a destructure: check the hidden temporary, the flow-typing "let" if there is one, and each expanded
  /// binding, then consume what was taken apart - the temporary if there is one, otherwise the source value.
  SPP_EXP_FUN auto DestructureStage8(
    LocalVariableAst const &destructure,
    Vec<Unique<LocalVariableAst>> const &elems,
    Vec<Unique<LetStatementInitializedAst>> const &new_asts,
    Shared<IdentifierAst> const &tmp_name,
    LetStatementInitializedAst *cond_let,
    bool from_case_pattern,
    ScopeManager &sm,
    CompilerMetaData *meta)
    -> void;

  /// Stage 9 for a destructure: resolve the hidden temporary, the flow-typing "let" if there is one, and each expanded
  /// binding.
  SPP_EXP_FUN auto DestructureStage9(
    Vec<Unique<LetStatementInitializedAst>> const &new_asts,
    Shared<IdentifierAst> const &tmp_name,
    LetStatementInitializedAst *cond_let,
    ScopeManager &sm,
    CompilerMetaData *meta)
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
