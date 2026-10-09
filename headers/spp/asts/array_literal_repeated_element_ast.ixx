module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_repeated_element_ast;
import spp.asts.array_literal_ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ArrayLiteralRepeatedElementAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// The repeated array literal represents the [0; 3_uz] literal
/// which copies, by bits, the provided element (so it must
/// superimpose Copy), n amounts of times into the "Arr[T, n]"
/// type.
SPP_EXP_CLS struct spp::asts::ArrayLiteralRepeatedElementAst final : ArrayLiteralAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(ArrayLiteralRepeatedElementAst);

  /// The opening "[" token.
  Unique<TokenAst> TokL;

  /// The element that will be copied n times.
  Unique<ExpressionAst> Elem;

  /// The ";" separator token.
  Unique<TokenAst> TokSemicolon;

  /// The size of the array (comptime evaluatable).
  Unique<ExpressionAst> Size;

  /// The closing "]" token.
  Unique<TokenAst> TokR;

  ArrayLiteralRepeatedElementAst(
    decltype(TokL) &&tok_l,
    decltype(Elem) &&elem,
    decltype(TokSemicolon) &&tok_semicolon,
    decltype(Size) &&size,
    decltype(TokR) &&tok_r);

  ~ArrayLiteralRepeatedElementAst() override;

  /// Check the element is equal with the element of the
  /// other array, and that the lengths are equal too.
  SPP_ATTR_NODISCARD auto EqualsArrayLiteralRepeatedElement(
    ArrayLiteralRepeatedElementAst const &other) const -> Ordering override;

  /// Reverse hook to activate the array equality check
  /// from the other ast.
  SPP_ATTR_NODISCARD auto Equals(
    ExpressionAst const &other) const -> Ordering override;

  /// Check the element and size are valid (size must be
  /// compile-time evaluatable), the type must be copyable,
  /// and also check the element isn't borrowed.
  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Check the memory integrity of the element (the size
  /// is a number so always copyable). Todo: do we need to
  /// check it.
  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Resolve the array literal at compile-time. This can
  /// only be done if the element within the array is
  /// compile-time evaluatable itself.
  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Use the internal LLVM array type, which is what the
  /// standard library's "Arr" type lowers to anyway, and
  /// create the array of values. Comptime and runtime
  /// paths (create with values vs GEP).
  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// Create the "Arr[T, n]" type based off the element's
  /// type, and the provided size; and then analyse the
  /// type to trigger a generic instantiation.
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// Move through the element and size to substitute
  /// generics in as they might contain postfix ops that
  /// need to be checked.
  SPP_ATTR_NODISCARD auto ReadExpr(
    analyse::scopes::ExprSubst const &sub) const
    -> Shared<ExpressionAst> override;

  /// Arrays can be used ina runtime default context, only
  /// if all the element and size are allowed to be used in
  /// a runtime default context.
  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  /// The scope a size that was not already a literal was
  /// analysed in. "Size" itself is never analysed: it is either
  /// folded to a literal in stage 7, or, in a template, kept as
  /// written for each instantiation's clone to fold. So whatever
  /// scopes its analysis opened ("case", a block) are only here:
  /// stage 8 checks "_AnalysedSize" in this scope, and the later
  /// stages skip it as one. In a template, the array's type is
  /// analysed here too, as its size argument may open scopes of
  /// its own. Null when the size was a literal to begin with.
  Scope *_SizeScope = nullptr;

  /// The analysed copy of a size that was not a literal, kept so
  /// that stage 8 can memory check it like any other analysed
  /// expression.
  Unique<ExpressionAst> _AnalysedSize;

  /// The "Arr[T, n]" type, built and analysed once in stage 7.
  /// Rebuilding it for every "InferType" would analyse its size
  /// argument again each time, and a "case" there opens its scopes
  /// again each time.
  Shared<TypeAst> _ArrayType;

  /// Step the walk past "_SizeScope", after the element's own
  /// scopes (it was created after them).
  auto SkipSizeScope(ScopeManager *sm) const -> void;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::ArrayLiteralRepeatedElementAst)
