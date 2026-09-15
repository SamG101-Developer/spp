module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ArrayLiteralExplicitElementsAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// The explicit array ast represents an array literal with
/// a variable number of elements. This maps to the Arr[T, n]
/// type.
SPP_EXP_CLS struct spp::asts::ArrayLiteralExplicitElementsAst final : ArrayLiteralAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(ArrayLiteralExplicitElementsAst);

  /// The opening "[" token.
  Unique<TokenAst> TokL;

  /// The list of all the elements in the array.
  Vec<Unique<ExpressionAst>> Elems;

  /// The closing "]" token.
  Unique<TokenAst> TokR;

  ArrayLiteralExplicitElementsAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elements,
    decltype(TokR) &&tok_r);

  ~ArrayLiteralExplicitElementsAst() override;

  /// Check each element is equal with the corresponding
  /// element of the other array, after a length check.
  SPP_ATTR_NODISCARD auto EqualsArrayLiteralExplicitElements(
    ArrayLiteralExplicitElementsAst const &other) const
    -> Ordering override;

  /// Reverse hook to activate the array equality check
  /// from the other ast.
  SPP_ATTR_NODISCARD auto Equals(
    ExpressionAst const &other) const
    -> Ordering override;

  /// Analyse each element, check they are valid expression
  /// asts, check they are all the same type, not borrowed,
  /// and activate an analysis on the inferred type of this
  /// array, to instantiate the generic.
  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Check the memory status of every symbol going into
  /// the array, to check that they're initialised etc,
  /// and able to me "moved" or copied.
  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Resolve the array literal at compile-time. This can
  /// only be done if each of the elements within the array
  /// is compile-time evaluatable themselves.
  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Use the internal LLVM array type, which is what the
  /// standard library's "Arr" type lowers to anyway, and
  /// create the array of values. Comptime and runtime
  /// paths (create with values vs GEP).
  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// Create the "Arr[T, n]" type based off the elements'
  /// consistent types, the number of elements, and then
  /// analyse the type to trigger a generic instantiation.
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  /// Move through the elements to substitute generics in
  /// as they might contain postfix ops that need to be
  /// checked.
  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  /// Arrays can be used ina runtime default context, only
  /// if all of the elements are allowed to be used in a
  /// runtime default context.
  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::ArrayLiteralExplicitElementsAst)
