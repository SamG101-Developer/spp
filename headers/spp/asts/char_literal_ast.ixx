module;
#include <spp/macros.hpp>

export module spp.asts.char_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CharLiteralAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TokenAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::CharLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(CharLiteralAst);

  /// The optional "b" prefix, converting the char into a U8
  /// byte type.
  Unique<TokenAst> BytePrefix;

  /// The actual char represented by the literal.
  Unique<TokenAst> Val;

  explicit CharLiteralAst(
    decltype(BytePrefix) &&byte_prefix,
    decltype(Val) &&val);

  ~CharLiteralAst() override;

  SPP_ATTR_NODISCARD auto EqualsCharLiteral(CharLiteralAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::CharLiteralAst);
