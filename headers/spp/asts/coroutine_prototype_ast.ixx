module;
#include <spp/macros.hpp>

export module spp.asts.coroutine_prototype_ast;
import spp.asts.ast_kind;
import spp.asts.function_prototype_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
}

namespace spp::asts {
  SPP_EXP_CLS struct CoroutinePrototypeAst;
  SPP_EXP_CLS struct SubroutinePrototypeAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::CoroutinePrototypeAst final : FunctionPrototypeAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KIND(CoroutinePrototypeAst)

  CoroutinePrototypeAst(
    decltype(Annotations) &&annotations,
    decltype(TokCmp) &&tok_cmp,
    decltype(TokFun) &&tok_fun,
    decltype(Name) name,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(FnParamGroup) &&param_group,
    decltype(TokArrow) &&tok_arrow,
    decltype(ReturnType) return_type,
    decltype(Impl) &&impl);

  ~CoroutinePrototypeAst() override;

  SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto IsCoroutine() const -> bool override;

  auto IsOnce() const -> bool;

  /**
   * The subroutine this coroutine was desugared into, or @c nullptr if it was not one that is (see @c _LowerGenOnce ).
   * A call to a @c GenOnce coroutine targets this, not the coroutine itself.
   */
  SPP_ATTR_NODISCARD auto GenOnceLowered() const -> SubroutinePrototypeAst*;

private:
  bool _IsOnce;
  Shared<TypeAst> _YieldType;
  Shared<TypeAst> _SendType;
  Unique<SubroutinePrototypeAst> _GenOnceLowered;

  /**
   * Desugar a @c GenOnce coroutine into a subroutine returning the yielded value: nothing ever resumes it, so there is
   * no frame to build and no generator to hand back, and the one @c gen it runs is a @c ret . Idempotent, and a no-op
   * on a coroutine that yields more than once.
   *
   * @n
   * Moving the body out is only safe once Stage8 has read it as a coroutine's, which is why this is not done during
   * analysis.
   */
  auto _LowerGenOnce() -> void;

  /**
   * Mark @p lowered @c alwaysinline when this coroutine yields a borrow, so that the storage the borrow points at
   * ends up in the caller's frame - where the yield's lifetime says it belongs, and where it can be promoted away.
   */
  auto _ForceInlineBorrowedYield(SubroutinePrototypeAst const &lowered) const -> void;

  /**
   * Give the lowering's allocas a lifetime of their own; see the definition. Needs the body, so runs after it.
   */
  auto _DeclareBorrowedYieldStorage(SubroutinePrototypeAst const &lowered) const -> void;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::CoroutinePrototypeAst)
