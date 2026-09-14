module;
#include <spp/macros.hpp>

export module spp.asts.string_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(StringLiteralAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::StringLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(StringLiteralAst);

  /// The optional "b" prefix, converting the string into a
  /// byte string.
  Unique<TokenAst> BytePrefix;

  /// The string value represented by the literal.
  Unique<TokenAst> Val;

  explicit StringLiteralAst(
    decltype(BytePrefix) &&byte_prefix,
    decltype(Val) &&val);

  ~StringLiteralAst() override;

  SPP_ATTR_NODISCARD auto EqualsStringLiteral(StringLiteralAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto CppVal() const -> Str;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::StringLiteralAst)
