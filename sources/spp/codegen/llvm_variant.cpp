module;
#include <spp/macros.hpp>

module spp.codegen.llvm_variant;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.type_ast;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import spp.utils.ice;
import spp.utils.types;
import genex;
import llvm;
import std;

// Width of a variant's discriminant. Matches the "sizeof(std::size_t)"
// discriminator that "SizeOf" accounts for.
constexpr auto kVariantTagBits = 64u;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, TypeRef);

namespace spp::codegen {
  /// "source_tag" translated through "map" (each pair a source tag and the
  /// target tag for it), as a chain of selects, innermost first.
  static auto TranslateVariantTag(
    llvm::Value *source_tag, Vec<Pair<std::uint64_t, std::uint64_t>> const &map,
    Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// The variant at "source_ptr" re-tagged as "target" through "map", into a
  /// new zeroed slot: the tag stored, and the payload copied over, as much of
  /// it as the smaller of the two payloads holds. Widening and narrowing share
  /// this with inversely built maps.
  static auto RetagVariant(
    llvm::Value *source_ptr, TypeRef const &source, TypeRef const &target,
    Vec<Pair<std::uint64_t, std::uint64_t>> const &map, Str const &name,
    LlvmCtx *ctx) -> llvm::Value*;
}

/// [CHECKED]
auto spp::codegen::TranslateVariantTag(
  llvm::Value *source_tag, Vec<Pair<std::uint64_t, std::uint64_t>> const &map,
  Str const &name, LlvmCtx *ctx) -> llvm::Value* {
  // For an identity map, no translating needs to take place, so return the
  // current source tag.
  if (genex::all_of(map, [](auto const &m) { return m.first == m.second; })) {
    return source_tag;
  }

  // The default: the last entry's target tag, taken when no earlier entry
  // matches.
  const auto tag_type = GetVariantTagType(ctx);
  auto target_tag = static_cast<llvm::Value*>(
    llvm::ConstantInt::get(tag_type, map.back().second));

  // For each map entry, get the "from->to" mapping, and create a comparison
  // between the source tag value and the "from" type (by index). If they match,
  // select the "to" value, otherwise the current target (once found).
  for (auto i = map.size() - 1; i > 0; --i) {
    const auto from_tag = llvm::ConstantInt::get(tag_type, map[i - 1].first);
    const auto to_tag = llvm::ConstantInt::get(tag_type, map[i - 1].second);

    const auto matches = ctx->Builder.CreateICmpEQ(
      source_tag, from_tag, name + ".from.is");
    target_tag = ctx->Builder.CreateSelect(
      matches, to_tag, target_tag, name + ".to.tag");
  }
  return target_tag;
}

/// [CHECKED]
auto spp::codegen::RetagVariant(
  llvm::Value *source_ptr, TypeRef const &source, TypeRef const &target,
  Vec<Pair<std::uint64_t, std::uint64_t>> const &map, Str const &name,
  LlvmCtx *ctx) -> llvm::Value* {
  // Nothing to re-tag if the target has no members; and both types must have
  // an LLVM lowering.
  if (map.empty()) { return nullptr; }
  const auto source_llvm_type = GetLlvmTypeOf(source.WithoutConvention(), ctx);
  const auto target_llvm_type = GetLlvmTypeOf(target.WithoutConvention(), ctx);
  if (source_llvm_type == nullptr or target_llvm_type == nullptr) {
    Ice("variant has no llvm type while re-tagging");
  }

  // Load the variant tag from the source variant, using the source type. Then,
  // translate the source's tag, using the target's numbering.
  const auto source_tag = LoadVariantTag(
    source_ptr, source_llvm_type, name + ".from.tag", ctx);
  const auto target_tag = TranslateVariantTag(source_tag, map, name, ctx);

  // Zero the slot first: a wider target's payload tail is not
  // copied, and a target with no payload is all tag.
  const auto target_slot = LlvmEntryAlloca(target_llvm_type, name + ".to.slot", ctx);
  ctx->Builder.CreateStore(llvm::Constant::getNullValue(target_llvm_type), target_slot);
  ctx->Builder.CreateStore(
    target_tag, GetVariantTagPtr(target_slot, target_llvm_type, name + ".to.tag.ptr", ctx));

  // The payload: as much as the smaller of the two holds.
  const auto source_struct = llvm::cast<llvm::StructType>(source_llvm_type);
  const auto target_struct = llvm::cast<llvm::StructType>(target_llvm_type);
  if (source_struct->getNumElements() < 2 or target_struct->getNumElements() < 2) { return target_slot; }
  auto const &dl = ctx->Module->getDataLayout();
  const auto source_payload_type = source_struct->getElementType(1);
  const auto target_payload_type = target_struct->getElementType(1);
  const auto size = std::min(
    dl.getTypeAllocSize(source_payload_type).getFixedValue(),
    dl.getTypeAllocSize(target_payload_type).getFixedValue());
  ctx->Builder.CreateMemCpy(
    GetVariantPayloadPtr(target_slot, target_llvm_type, name + ".to.payload.ptr", ctx),
    dl.getABITypeAlign(target_payload_type),
    GetVariantPayloadPtr(source_ptr, source_llvm_type, name + ".from.payload.ptr", ctx),
    dl.getABITypeAlign(source_payload_type),
    size);
  return target_slot;
}

/// [CHECKED]
auto spp::codegen::GetVariantTagType(
  LlvmCtx const *ctx) -> llvm::IntegerType* {
  // Every variant discriminates its members with the same integer width (64
  // bits). This allows it to be updated from a single place in the future.
  return llvm::Type::getIntNTy(*ctx->Context, kVariantTagBits);
}

/// [CHECKED]
auto spp::codegen::GetVariantIndexOfMember(
  TypeRef const &variant, TypeRef const &member,
  Scope const &scope) -> Opt<std::uint64_t> {
  using namespace analyse::utils;

  // Index the type in the list of member types of the variant. If assignable
  // into the member type, return that index. Otherwise, nullopt (not matched).
  const auto members = type_members::VariantMemberRefs(variant, scope);
  for (auto i = 0uz; i < members.size(); ++i) {
    if (type_compare::Assignable(members[i], member, scope, scope)) {
      return static_cast<std::uint64_t>(i);
    }
  }
  return Nullopt;
}

/// [CHECKED]
auto spp::codegen::GetVariantPayloadPtr(
  llvm::Value *llvm_variant_ptr, llvm::Type *llvm_variant_type, Str const &name,
  LlvmCtx *ctx) -> llvm::Value* {
  // The payload is the second field, behind the discriminant. GEP it by field
  // index, and return the pointer, not the value under it.
  return ctx->Builder.CreateStructGEP(
    llvm_variant_type, llvm_variant_ptr, 1, name);
}

/// [CHECKED]
auto spp::codegen::GetVariantTagPtr(
  llvm::Value *llvm_variant_ptr, llvm::Type *llvm_variant_type, Str const &name,
  LlvmCtx *ctx) -> llvm::Value* {
  // The tag is the first field, before the payload. GEP it by field index, and
  // return the pointer, not the value under it.
  return ctx->Builder.CreateStructGEP(
    llvm_variant_type, llvm_variant_ptr, 0, name);
}

/// [CHECKED]
auto spp::codegen::LoadVariantTag(
  llvm::Value *llvm_variant_ptr, llvm::Type *llvm_variant_type, Str const &name,
  LlvmCtx *ctx) -> llvm::Value* {
  // The tag is the first field, before the payload: its pointer, then the
  // value underneath.
  const auto llvm_tag_ptr = GetVariantTagPtr(
    llvm_variant_ptr, llvm_variant_type, name + ".ptr", ctx);
  const auto llvm_tag_type = GetVariantTagType(ctx);
  return ctx->Builder.CreateLoad(llvm_tag_type, llvm_tag_ptr, name);
}

/// [CHECKED]
auto spp::codegen::BuildVariant(
  llvm::Value *llvm_member_val, llvm::Type *llvm_variant_type,
  const std::uint64_t index, Str const &name, LlvmCtx *ctx) -> llvm::Value* {
  // Build a stack slot using the common alloca function, then reuse the "store
  // variant" functionality to emplace the variant into the slot. Return the
  // value out (via a load instruction on the slot).
  const auto llvm_slot = LlvmEntryAlloca(llvm_variant_type, name + ".slot", ctx);
  StoreVariant(llvm_slot, llvm_member_val, llvm_variant_type, index, name, ctx);
  return ctx->Builder.CreateLoad(llvm_variant_type, llvm_slot, name);
}

/// [CHECKED]
auto spp::codegen::StoreVariant(
  llvm::Value *llvm_slot, llvm::Value *llvm_member_val,
  llvm::Type *llvm_variant_type, const std::uint64_t index, Str const &name,
  LlvmCtx *ctx) -> void {
  // Store a null version of the variant type into the pre-created "slot" (a
  // stack alloca).
  ctx->Builder.CreateStore(
    llvm::Constant::getNullValue(llvm_variant_type), llvm_slot);

  // Store the index for the type in the pointer of the tag (ie write into the
  // pointer, so get, not load).
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(GetVariantTagType(ctx), index),
    GetVariantTagPtr(llvm_slot, llvm_variant_type, name + ".tag.ptr", ctx));

  // If a type has a value, write it into the slot. Some members, such as the
  // stateless "None", lower to nothing, so have no payload.
  if (llvm_member_val == nullptr or IsValuelessType(llvm_member_val->getType())) { return; }
  ctx->Builder.CreateStore(
    llvm_member_val,
    GetVariantPayloadPtr(llvm_slot, llvm_variant_type, name + ".payload.ptr", ctx));
}

/// [CHECKED]
auto spp::codegen::WidenVariant(
  llvm::Value *source_ptr, TypeRef const &source, TypeRef const &target,
  Scope const &scope, Str const &name, LlvmCtx *ctx) -> llvm::Value* {
  using namespace analyse::utils;
  // If the source or the target is not a variant, then no widening can take
  // place.
  if (not type_predicates::IsTypeVariant(target, scope)
    or not type_predicates::IsTypeVariant(source, scope)) {
    return nullptr;
  }

  // Where each of the source's members sits in the target. Any member the
  // target lacks means this is not a widening.
  auto map = Vec<Pair<std::uint64_t, std::uint64_t>>();
  const auto source_members = type_members::VariantMemberRefs(source, scope);
  for (auto i = 0uz; i < source_members.size(); ++i) {
    const auto target_tag = GetVariantIndexOfMember(target, source_members[i], scope);
    if (not target_tag.has_value()) { return nullptr; }
    map.emplace_back(static_cast<std::uint64_t>(i), *target_tag);
  }
  return RetagVariant(source_ptr, source, target, map, name, ctx);
}

/// [CHECKED]
auto spp::codegen::NarrowVariant(
  llvm::Value *source_ptr, TypeRef const &source, TypeRef const &target,
  Scope const &scope, Str const &name, LlvmCtx *ctx) -> llvm::Value* {
  using namespace analyse::utils;
  // If the source or the target is not a variant, then no narrowing can take
  // place.
  if (not type_predicates::IsTypeVariant(target, scope)
    or not type_predicates::IsTypeVariant(source, scope)) {
    return nullptr;
  }

  // Where each of the target's members sits in the source. Any member the
  // source lacks means this is not a sub-variant.
  auto map = Vec<Pair<std::uint64_t, std::uint64_t>>();
  const auto target_members = type_members::VariantMemberRefs(target, scope);
  for (auto i = 0uz; i < target_members.size(); ++i) {
    const auto source_tag = GetVariantIndexOfMember(source, target_members[i], scope);
    if (not source_tag.has_value()) { return nullptr; }
    map.emplace_back(*source_tag, static_cast<std::uint64_t>(i));
  }
  return RetagVariant(source_ptr, source, target, map, name, ctx);
}
