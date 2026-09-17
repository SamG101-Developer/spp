module;
#include <spp/macros.hpp>

export module spp.asts.boolean_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(BooleanLiteralAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

/// The boolean literal is either "true" or "false" expressed
/// in S++ code.
SPP_EXP_CLS struct spp::asts::BooleanLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(BooleanLiteralAst);

  /// The true/false token for the bool literal.
  Unique<TokenAst> TokBool;

  /// Static named constructor to build from a C++ literal.
  static auto FromCppVal(bool val) -> Unique<BooleanLiteralAst>;

  explicit BooleanLiteralAst(
    decltype(TokBool) &&tok_bool);

  ~BooleanLiteralAst() override;

  /// Check the internal bool is equal with the corresponding
  /// internal bool of the other array.
  SPP_ATTR_NODISCARD auto EqualsBooleanLiteral(BooleanLiteralAst const &other) const -> Ordering override;

  /// Reverse hook to activate the boolean equality check
  /// from the other ast.
  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  /// Static named constructor to create a "true" boolean
  /// literal at the given position.
  static auto True(std::size_t pos) -> Unique<BooleanLiteralAst>;

  /// Static named constructor to create a "false" boolean
  /// literal at the given position.
  static auto False(std::size_t pos) -> Unique<BooleanLiteralAst>;

  /// Check if the internally stored token represents the
  /// "true" literal.
  SPP_ATTR_NODISCARD auto IsTrue() const -> bool;

  /// Extract the internally stored token into the C++ bool
  /// value it represents.
  SPP_ATTR_NODISCARD auto CppVal() const -> bool;

  /// Resolve the boolean literal at compile time, producing a
  /// compile time value of either "true" or "false".
  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Generate an LLVM constant integer value of 1 for "true"
  /// and 0 for "false".
  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// The boolean literal's type is always "std::boolean::Bool",
  /// the compiler known boolean type.
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::BooleanLiteralAst);
