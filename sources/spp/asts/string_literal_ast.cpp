module;
#include <spp/macros.hpp>

module spp.asts.string_literal_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.convention_ref_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_func;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.utils.strings;
import llvm;

SPP_MOD_BEGIN
spp::asts::StringLiteralAst::StringLiteralAst(
  decltype(BytePrefix) &&byte_prefix,
  decltype(Val) &&val) :
  BytePrefix(std::move(byte_prefix)),
  Val(std::move(val)) {
}

spp::asts::StringLiteralAst::~StringLiteralAst() = default;

auto spp::asts::StringLiteralAst::EqualsStringLiteral(
  StringLiteralAst const &other) const
  -> Ordering {
  // Equality is based on the internal string value.
  const auto matching_byte_prefix = static_cast<bool>(BytePrefix) == static_cast<bool>(other.BytePrefix);
  return matching_byte_prefix and Val->TokenData == other.Val->TokenData ? Ordering::equal : Ordering::less;
}

auto spp::asts::StringLiteralAst::Equals(
  ExpressionAst const &other) const
  -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsStringLiteral(*this);
}

auto spp::asts::StringLiteralAst::PosStart() const
  -> std::size_t {
  // Use the value.
  return Val->PosStart();
}

auto spp::asts::StringLiteralAst::PosEnd() const
  -> std::size_t {
  // Use the value.
  return Val->PosEnd();
}

auto spp::asts::StringLiteralAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<StringLiteralAst>(
    AstClone(BytePrefix),
    AstClone(Val));
}

auto spp::asts::StringLiteralAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(BytePrefix);
  SPP_STRING_APPEND(Val);
  SPP_STRING_END;
}

auto spp::asts::StringLiteralAst::Stage9_CompTimeResolve(
  ScopeManager *,
  CompilerMetaData *meta)
  -> void {
  // Clone and return the float literal as is for compile-time resolution.
  meta->CmpResult = AstClone(this);
}

auto spp::asts::StringLiteralAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  //
  using spp::utils::strings::DecodeStringLiteral;

  // Decode the token (which includes its surrounding double
  // quotes) into the raw bytes, resolving escape sequences,
  // and emit either a string or byte string for it.
  const auto bytes = DecodeStringLiteral(Val->TokenData);
  const auto str_alloc = ctx->Builder.CreateGlobalString(
    bytes, "string_literal", 0, ctx->Module.get(), false);
  return str_alloc;
  const auto emission_module = codegen::GetEmissionModule(*ctx);
  const auto llvm_bytes = ctx->Builder.CreateGlobalString(
    bytes, "string_literal", 0, emission_module, false);

  // A literal's type is "&StrView" (or "&View[U8]" behind the
  // "b" prefix), and both of those are a { ptr, length } pair
  // rather than a bare pointer. Everything in it is a compile
  // time constant, so the view is emitted as its own constant
  // global instead of being rebuilt on the stack at every use.
  const auto view_type = InferType(sm, meta)->WithoutConvention();
  const auto view_type_sym = sm->CurrentScope->GetTypeSymbol(view_type.get());
  const auto llvm_view_type = view_type_sym != nullptr
    ? llvm::dyn_cast_or_null<llvm::StructType>(codegen::GetLlvmType(*view_type_sym, ctx))
    : nullptr;

  // Build the view's fields, which are always a pointer and a
  // length.
  const auto ptr_idx = codegen::GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 0);
  const auto length_idx = codegen::GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 1);
  auto llvm_fields = Vec<llvm::Constant*>(llvm_view_type->getNumElements(), nullptr);
  llvm_fields[ptr_idx] = llvm_bytes;
  llvm_fields[length_idx] = llvm::ConstantInt::get(
    llvm_view_type->getElementType(length_idx), bytes.size());

  const auto llvm_view = llvm::ConstantStruct::get(
    llvm_view_type, llvm_fields.ToStdVector());
  return new llvm::GlobalVariable(
    *emission_module, llvm_view_type, true, llvm::GlobalValue::PrivateLinkage, llvm_view, "string_literal.view");
}

auto spp::asts::StringLiteralAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // A char literal is either a StrView or Vec[U8] type, depending on the "b" byte prefix.
  // Todo: static flag to check if the type's been analysed before? only has to be done once.
  using generate::common_types::StringViewType;
  using generate::common_types::ViewU8Type;
  auto type = BytePrefix != nullptr
    ? ViewU8Type(PosStart())->WithConvention(MakeUnique<ConventionRefAst>(nullptr))
    : StringViewType(PosStart())->WithConvention(MakeUnique<ConventionRefAst>(nullptr));
  type->Stage7_AnalyseSemantics(sm, meta); // Todo: single analysis somewhere?
  return type;
}

auto spp::asts::StringLiteralAst::CppVal() const -> Str {
  // Reuse the same decoding Stage11_CodeGen uses, so this matches the literal's actual (escape-resolved) value
  // instead of the raw source text (which would still contain unresolved escapes like "\n" as two characters).
  return spp::utils::strings::DecodeStringLiteral(Val->TokenData);
}

SPP_MOD_END
