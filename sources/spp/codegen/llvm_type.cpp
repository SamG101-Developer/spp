module;
#include <spp/macros.hpp>

module spp.codegen.llvm_type;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.analyse.scopes.symbols;
import spp.asts.boolean_literal_ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_mangle;
import spp.lex.tokens;
import spp.utils.types;
import genex;
import llvm;
import std;

const spp::Vec<spp::Str> kVoidParts = {"std", "void", "Void"};
const spp::Vec<spp::Str> kBoolParts = {"std", "boolean", "Bool"};
const spp::Vec<spp::Str> kStrViewParts = {"std", "string_view", "StrView"};
const spp::Vec<spp::Str> kSizedIntegerParts = {"std", "num", "sized_integer", "SizedInteger"};
const spp::Vec<spp::Str> kSizedFloatParts = {"std", "num", "sized_floating_point", "SizedFloatingPoint"};
const spp::Vec<spp::Str> kArrParts = {"std", "array", "Arr"};
const spp::Vec<spp::Str> kFunRefParts = {"std", "function", "FunRef"};
const spp::Vec<spp::Str> kFunMutParts = {"std", "function", "FunMut"};
const spp::Vec<spp::Str> kFunMovParts = {"std", "function", "FunMov"};
const spp::Vec<spp::Str> kGenParts = {"std", "generator", "Gen"};
const spp::Vec<spp::Str> kGenOnceParts = {"std", "generator", "GenOnce"};
const spp::Vec<spp::Str> kGeneratedParts = {"std", "generator", "Generated"};
const spp::Vec<spp::Str> kVarParts = {"std", "variant", "Var"};

// Width of a variant's discriminant. Matches the "sizeof(std::size_t)" discriminator that "SizeOf" accounts for.
constexpr auto kVariantTagBits = 64u;

// Largest alignment a variant payload buffer will be built out of. Anything needing more than a 16 byte alignment is
// vector/extended precision territory, which the layout code does not model either.
constexpr auto kMaxVariantPayloadAlign = 16uz;

static auto GetFloatIntrinsic(const std::size_t bit_width) -> llvm::fltSemantics const& {
  switch (bit_width) {
    case 8: { return llvm::APFloatBase::Float8E4M3(); }
    case 16: { return llvm::APFloatBase::IEEEhalf(); }
    case 32: { return llvm::APFloatBase::IEEEsingle(); }
    case 64: { return llvm::APFloatBase::IEEEdouble(); }
    case 128: { return llvm::APFloatBase::IEEEquad(); }
    default: std::unreachable();
  }
  std::unreachable();
}

auto spp::codegen::RegisterLlvmTypeInfo(
  asts::ClassPrototypeAst const *cls_proto,
  LLvmCtx const *ctx)
  -> void {
  // Note: because symbols have a convention attached to them, retrieval handles pointer logic for borrows.

  // $ types are function "mock" types (a $-type generated per function that superimposes n FunXXXs over itself). A
  // function used as a value is one of these mocks, so it lowers to the same { fn_ptr, env_ptr } pair as the function
  // type it extends, making it interchangeable with closures.
  if (cls_proto->Name->IsCompilerGeneratedType()) {
    const auto mock_sym = cls_proto->GetClsSym();
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    mock_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
    return;
  }

  // Get the class symbol from the current scope.
  const auto scope = cls_proto->GetAstScope();
  const auto cls_sym = scope->TySym;

  // For compiler known types, specialize the llvm type symbols.
  const auto parts = scope->Ancestors()
    | genex::views::drop_last(1)
    | genex::views::transform([](auto *x) { return x->NonGenericScope->NameAsString(); })
    | genex::views::reverse
    | genex::to<Vec>();

  // Lower S++ "Void" to the llvm "void" type.
  if (parts == kVoidParts) {
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getVoidTy(*ctx->Context);
    return;
  }

  // Lower S++ "Bool" to the llvm "i1" type.
  if (parts == kBoolParts) {
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getInt1Ty(*ctx->Context);
    return;
  }

  // Lower S++ "StrView" to the llvm "i8*" type.
  if (parts == kStrViewParts) {
    cls_sym->LlvmInfo->LlvmType = llvm::PointerType::get(*ctx->Context, 0);
    return;
  }

  // Lower S++ "S/U[8|16|32|64|128]" to the llvm "i[8|16|32|64|128]" type (llvm integers carry no signedness).
  if (parts == kSizedIntegerParts) {
    const auto bit_width_ast = scope->TySym->FqName()->LastTypePart()->GnArgGroup->CompAt("w")->Val->To<
      asts::IntegerLiteralAst>();
    if (bit_width_ast == nullptr) { return; }
    const auto w = static_cast<unsigned>(std::stoi(bit_width_ast->Val->TokenData));;
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getIntNTy(*ctx->Context, w);
    return;
  }

  // Lower S++ "F[8|16|32|64|128]" to the llvm "f[8|16|32|64|128]" type.
  if (parts == kSizedFloatParts) {
    const auto bit_width_ast = scope->TySym->FqName()->LastTypePart()->GnArgGroup->CompAt("w")->Val->To<
      asts::IntegerLiteralAst>();
    if (bit_width_ast == nullptr) { return; }
    const auto w = static_cast<unsigned>(std::stoi(bit_width_ast->Val->TokenData));;
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, GetFloatIntrinsic(w));
    return;
  }

  // Lower S++ Arr" to the llvm "[T * n]" type.
  if (parts == kArrParts) {
    const auto gn_arg_group = cls_sym->FqName()->LastTypePart()->GnArgGroup.get();
    const auto length_ast = gn_arg_group->CompAt("n")->Val->To<asts::IntegerLiteralAst>();
    const auto elem_sym = scope->GetTypeSymbol(gn_arg_group->TypeAt("T")->Val.get());
    if (length_ast != nullptr and elem_sym != nullptr) {
      if (elem_sym->LlvmInfo->LlvmType == nullptr and elem_sym->Type != nullptr) {
        RegisterLlvmTypeInfo(elem_sym->Type, ctx);
      }
      if (const auto elem_llvm_type = GetLlvmType(*elem_sym, ctx); elem_llvm_type != nullptr) {
        cls_sym->LlvmInfo->LlvmType = llvm::ArrayType::get(elem_llvm_type, std::stoull(length_ast->Val->TokenData));
      }
    }
    return;
  }

  // Lower the function types to a { fn_ptr, env_ptr } pair (a "fat pointer": the function code plus a pointer to its
  // captured environment; the env pointer is null for capture-less functions). All three share this layout, so plain
  // functions and closures are interchangeable.
  if (parts == kFunMovParts or parts == kFunMutParts or parts == kFunRefParts) {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
    return;
  }

  // Lower the generator types to a { resume_fn_ptr, env_ptr } pair (a "fat pointer": the coroutine resume function
  // plus a pointer to its frame/environment, which lives on the caller's stack - no heap allocation).
  if (parts == kGenParts or parts == kGenOnceParts or parts == kGeneratedParts) {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
    return;
  }

  // If the type already exists in LLVM, skip.
  if (const auto llvm_type = llvm::StructType::getTypeByName(*ctx->Context, mangle::mangle_type_name(*cls_sym));
    llvm_type != nullptr) {
    cls_sym->LlvmInfo->LlvmType = llvm_type;
    return;
  }

  // Lower S++ "Var" (the "A or B" variant type) to a { tag, payload } pair.
  if (parts == kVarParts) {
    const auto struct_type = llvm::StructType::create(*ctx->Context, mangle::mangle_type_name(*cls_sym));
    cls_sym->LlvmInfo->LlvmType = struct_type;

    auto const &dl = ctx->Module->getDataLayout();
    auto max_size = 0uz;
    auto max_align = 1uz;

    for (auto const &member : analyse::utils::type_utils::DedupVariableInnerTypes(*cls_sym->FqName(), *scope)) {
      const auto member_sym = scope->GetTypeSymbol(member.get());
      if (member_sym == nullptr) { continue; }

      // A variant can be registered before its members are, so lower any member still missing its llvm type.
      if (member_sym->LlvmInfo->LlvmType == nullptr and member_sym->Type != nullptr) {
        RegisterLlvmTypeInfo(member_sym->Type, ctx);
      }

      // Get the size and alignment, and upgrade the maximum if necessary.
      const auto member_llvm_type = GetLlvmType(*member_sym, ctx);
      if (member_llvm_type == nullptr or not member_llvm_type->isSized()) { continue; }
      max_size = std::max(max_size, dl.getTypeAllocSize(member_llvm_type).getFixedValue());
      max_align = std::max(max_align, dl.getABITypeAlign(member_llvm_type).value());
    }

    // Build the payload out of the widest integer any member needs to be aligned to, rather than out of bytes: a
    // "[n x i8]" buffer is only ever byte aligned, so storing a member into it would be under-aligned.
    const auto payload_elem_type = llvm::Type::getIntNTy(
      *ctx->Context, static_cast<unsigned>(std::min(max_align, kMaxVariantPayloadAlign) * 8));
    const auto payload_elem_size = dl.getTypeAllocSize(payload_elem_type).getFixedValue();
    const auto payload_type = llvm::ArrayType::get(
      payload_elem_type, (max_size + payload_elem_size - 1) / payload_elem_size);

    struct_type->setBody({GetVariantTagType(ctx), payload_type}, false);
    return;
  }

  // Empty struct, will fill in stage_10 when all attributes' types have been generated.
  cls_sym->LlvmInfo->LlvmType = llvm::StructType::create(*ctx->Context, mangle::mangle_type_name(*cls_sym));
}

auto spp::codegen::GetLlvmType(
  analyse::scopes::TypeSymbol const &type_sym,
  LLvmCtx const *ctx)
  -> llvm::Type* {
  // Either return the llvm type bound to the symbol, or a pointer for borrows.
  return type_sym.Convention != nullptr ? llvm::PointerType::get(*ctx->Context, 0) : type_sym.LlvmInfo->LlvmType;
}

auto spp::codegen::GetVariantTagType(
  LLvmCtx const *ctx)
  -> llvm::IntegerType* {
  // Every variant discriminates its members with the same integer width (64 bits).
  return llvm::Type::getIntNTy(*ctx->Context, kVariantTagBits);
}

auto spp::codegen::GetVariantTag(
  asts::TypeAst const &variant_type,
  asts::TypeAst const &member_type,
  analyse::scopes::Scope const &scope)
  -> std::optional<std::uint64_t> {
  //
  using analyse::utils::type_utils::DedupVariableInnerTypes;
  using analyse::utils::type_utils::TypeEq;

  // Index the type in the list of member types of the variant. Bind the list to a named local first, rather than
  // piping the returned temporary straight into a view over it.
  const auto members = DedupVariableInnerTypes(variant_type, scope);
  for (auto const &[i, member] : members | genex::views::enumerate) {
    if (TypeEq(*member, member_type, scope, scope, false)) {
      return static_cast<std::uint64_t>(i);
    }
  }
  return std::nullopt;
}

auto spp::codegen::GetVariantPayloadPtr(
  llvm::Value *variant_ptr,
  llvm::Type *variant_llvm_type,
  Str const &name,
  LLvmCtx *ctx)
  -> llvm::Value* {
  // The payload is the second field, behind the discriminant.
  return ctx->Builder.CreateStructGEP(variant_llvm_type, variant_ptr, 1, name);
}

auto spp::codegen::LoadVariantTag(
  llvm::Value *variant_ptr,
  llvm::Type *variant_llvm_type,
  Str const &name,
  LLvmCtx *ctx)
  -> llvm::Value* {
  // The discriminant is the first field, ahead of the payload.
  const auto tag_ptr = ctx->Builder.CreateStructGEP(variant_llvm_type, variant_ptr, 0, name + ".ptr");
  return ctx->Builder.CreateLoad(GetVariantTagType(ctx), tag_ptr, name);
}

auto spp::codegen::BuildVariant(
  llvm::Value *member_val,
  llvm::Type *variant_llvm_type,
  const std::uint64_t tag,
  Str const &name,
  LLvmCtx *ctx)
  -> llvm::Value* {
  // Build into a stack slot, because the payload is written through a pointer rather than by value. The slot starts
  // zeroed, because the member rarely fills the whole payload, and the whole struct is loaded back out at the end: the
  // bytes past the member would otherwise be stale stack data, undef to the optimiser and a disclosure hazard the
  // moment a variant is ever copied out of the program. Everything the "stores" below cover is dead-store-eliminated.
  const auto slot = llvm_entry_alloca(variant_llvm_type, name + ".slot", ctx);
  ctx->Builder.CreateStore(llvm::Constant::getNullValue(variant_llvm_type), slot);
  const auto tag_ptr = ctx->Builder.CreateStructGEP(variant_llvm_type, slot, 0, name + ".tag.ptr");
  ctx->Builder.CreateStore(llvm::ConstantInt::get(GetVariantTagType(ctx), tag), tag_ptr);

  // A member that lowers to nothing (such as the attribute-less "None") has no payload; the tag knows the type though.
  if (member_val != nullptr and not member_val->getType()->isVoidTy()) {
    ctx->Builder.CreateStore(member_val, GetVariantPayloadPtr(slot, variant_llvm_type, name + ".payload.ptr", ctx));
  }

  return ctx->Builder.CreateLoad(variant_llvm_type, slot, name);
}

auto spp::codegen::CoerceToVariant(
  llvm::Value *llvm_val,
  asts::TypeAst const &target_type,
  asts::TypeAst const &source_type,
  analyse::scopes::Scope const &scope,
  Str const &name,
  LLvmCtx *ctx)
  -> llvm::Value* {
  //
  using analyse::utils::type_utils::DedupVariableInnerTypes;
  using analyse::utils::type_utils::IsTypeVariant;
  using analyse::utils::type_utils::TypeEq;

  // Only a variant target ever needs a coercion, and a value already of the target type is one.
  if (llvm_val == nullptr or not IsTypeVariant(target_type, scope)) { return llvm_val; }
  if (TypeEq(target_type, source_type, scope, scope, false)) { return llvm_val; }

  const auto target_llvm_type = scope.GetTypeSymbol(&target_type)->LlvmInfo->LlvmType;
  SPP_ASSERT(target_llvm_type != nullptr);

  // A member value (source) is wrapped: tagged and copied into the payload.
  if (not IsTypeVariant(source_type, scope)) {
    const auto tag = GetVariantTag(target_type, source_type, scope);
    if (not tag.has_value()) { return llvm_val; }
    return BuildVariant(llvm_val, target_llvm_type, *tag, name, ctx);
  }

  // Otherwise, we need to widen one variant into another, like "Str or Bool" into "Str or Bool or S32". As the order is
  // not guaranteed to match, a mapping is needed.
  const auto source_llvm_type = scope.GetTypeSymbol(&source_type)->LlvmInfo->LlvmType;
  SPP_ASSERT(source_llvm_type != nullptr);

  auto tag_map = Vec<std::uint64_t>();
  auto is_identity_map = true;
  const auto source_members = DedupVariableInnerTypes(source_type, scope);
  for (auto const &[i, member] : source_members | genex::views::enumerate) {
    const auto target_tag = GetVariantTag(target_type, *member, scope);
    if (not target_tag.has_value()) { return llvm_val; }
    is_identity_map = is_identity_map and *target_tag == static_cast<std::uint64_t>(i);
    tag_map.EmplaceBack(*target_tag);
  }

  // Spill the source to memory, because the payload is copied through a pointer rather than by value.
  const auto source_slot = llvm_entry_alloca(source_llvm_type, name + ".from.slot", ctx);
  ctx->Builder.CreateStore(llvm_val, source_slot);
  const auto source_tag = LoadVariantTag(source_slot, source_llvm_type, name + ".from.tag", ctx);

  // Translate the discriminant with a chain of selects, innermost first. Variants have few members, so this stays
  // smaller than a lookup table, and it folds away entirely when the two numberings happen to agree.
  const auto tag_type = GetVariantTagType(ctx);
  auto target_tag = static_cast<llvm::Value*>(source_tag);
  if (not is_identity_map) {
    target_tag = llvm::ConstantInt::get(tag_type, tag_map.Back());
    for (auto i = tag_map.Len() - 1; i > 0; --i) {
      const auto matches = ctx->Builder.CreateICmpEQ(
        source_tag, llvm::ConstantInt::get(tag_type, i - 1), name + ".from.is");
      target_tag = ctx->Builder.CreateSelect(
        matches, llvm::ConstantInt::get(tag_type, tag_map[i - 1]), target_tag, name + ".to.tag");
    }
  }

  // Write the translated discriminant and move the payload over. The target's members are a superset of the source's,
  // so its payload buffer is always at least as large, and the source's size is the amount worth copying. That leaves
  // the target's wider tail uncopied, so zero the slot first.
  const auto target_slot = llvm_entry_alloca(target_llvm_type, name + ".to.slot", ctx);
  ctx->Builder.CreateStore(llvm::Constant::getNullValue(target_llvm_type), target_slot);
  ctx->Builder.CreateStore(
    target_tag, ctx->Builder.CreateStructGEP(target_llvm_type, target_slot, 0, name + ".to.tag.ptr"));

  auto const &dl = ctx->Module->getDataLayout();
  const auto source_payload_type = llvm::cast<llvm::StructType>(source_llvm_type)->getElementType(1);
  ctx->Builder.CreateMemCpy(
    GetVariantPayloadPtr(target_slot, target_llvm_type, name + ".to.payload.ptr", ctx),
    dl.getABITypeAlign(llvm::cast<llvm::StructType>(target_llvm_type)->getElementType(1)),
    GetVariantPayloadPtr(source_slot, source_llvm_type, name + ".from.payload.ptr", ctx),
    dl.getABITypeAlign(source_payload_type),
    dl.getTypeAllocSize(source_payload_type).getFixedValue());

  return ctx->Builder.CreateLoad(target_llvm_type, target_slot, name);
}
