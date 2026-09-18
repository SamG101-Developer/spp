module;
#include <spp/macros.hpp>

module spp.asts.boolean_literal_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;

SPP_MOD_BEGIN
auto BooleanLiteralAst::FromCppVal(
  const bool val) -> Unique<BooleanLiteralAst> {
  return val ? True(0) : False(0);
}

BooleanLiteralAst::BooleanLiteralAst(
  decltype(TokBool) &&tok_bool) :
  TokBool(std::move(tok_bool)) {
}

BooleanLiteralAst::~BooleanLiteralAst() = default;

auto BooleanLiteralAst::EqualsBooleanLiteral(
  BooleanLiteralAst const &other) const -> Ordering {
  // Equality is based off the bool value.
  return *TokBool == *other.TokBool
    ? Ordering::equal
    : Ordering::less;
}

auto BooleanLiteralAst::Equals(
  ExpressionAst const &other) const -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsBooleanLiteral(*this);
}

auto BooleanLiteralAst::PosStart() const -> std::size_t {
  // Use the bool token.
  return TokBool->PosStart();
}

auto BooleanLiteralAst::PosEnd() const -> std::size_t {
  // Use the bool token.
  return TokBool->PosEnd();
}

auto BooleanLiteralAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<BooleanLiteralAst>(
    AstClone(TokBool));
}

auto BooleanLiteralAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokBool);
  SPP_STRING_END;
}

auto BooleanLiteralAst::True(
  const std::size_t pos) -> Unique<BooleanLiteralAst> {
  // Create a boolean literal AST representing the "true" value.
  auto tok = MakeUnique<TokenAst>(
    pos, lex::SppTokenType::KW_TRUE, "true");
  return MakeUnique<BooleanLiteralAst>(std::move(tok));
}

auto BooleanLiteralAst::False(
  const std::size_t pos) -> Unique<BooleanLiteralAst> {
  // Create a boolean literal AST representing the "false" value.
  auto tok = MakeUnique<TokenAst>(
    pos, lex::SppTokenType::KW_FALSE, "false");
  return MakeUnique<BooleanLiteralAst>(std::move(tok));
}

auto BooleanLiteralAst::IsTrue() const -> bool {
  // Check if the boolean literal represents a true value.
  return TokBool->TokenType == lex::SppTokenType::KW_TRUE;
}

auto BooleanLiteralAst::CppVal() const -> bool {
  // Return the C++ boolean value of the boolean literal.
  return IsTrue();
}

auto BooleanLiteralAst::Stage9_CompTimeResolve(
  ScopeManager *, CompilerMetaData *meta) -> void {
  // Clone and return the boolean literal as is for compile-time
  // resolution.
  meta->CmpResult = AstClone(this);
}

auto BooleanLiteralAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  using lex::SppTokenType;
  SPP_ASSERT(
    TokBool->TokenType == SppTokenType::KW_TRUE or
    TokBool->TokenType == SppTokenType::KW_FALSE);

  // Instead of hardcoding the i1 type here, we resolve the
  // Bool type and then use the uniform LLVM type mapping
  // function to evaluate what the LLVM type is, should Bool
  // ever be changed from "i1".
  const auto type_sym = InferTypeRef(sm, meta).Sym;
  const auto llvm_type = codegen::GetLlvmType(*type_sym, ctx);

  // Create the "constant int" (this literal represents a known
  // bool value), and return that into whatever is using it.
  // This is an expression so much return the generated llvm
  // value.
  const auto value = TokBool->TokenType == SppTokenType::KW_TRUE
    ? 1ul
    : 0ul;
  return llvm::ConstantInt::get(llvm_type, value);
}

auto BooleanLiteralAst::InferType(
  ScopeManager *, CompilerMetaData *) -> Shared<TypeAst> {
  // The boolean ast is always inferred as "std::boolean::Bool".
  using generate::common_types::BooleanType;
  return BooleanType(PosStart());
}

auto BooleanLiteralAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  return TypeRef::Of(
    *generate::common_types_precompiled::BOOL, *sm->CurrentScope);
}

SPP_MOD_END
