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

// Width of a variant's discriminant. Matches the
// "sizeof(std::size_t)" discriminator that "SizeOf"
// accounts for.
constexpr auto kVariantTagBits = 64u;

auto spp::codegen::GetVariantTagType(
  LlvmCtx const *ctx)
  -> llvm::IntegerType* {
  // Every variant discriminates its members with the same
  // integer width (64 bits).
  return llvm::Type::getIntNTy(*ctx->Context, kVariantTagBits);
}

auto spp::codegen::GetVariantIndexOfMember(
  analyse::scopes::TypeRef const &variant,
  analyse::scopes::TypeRef const &member,
  analyse::scopes::Scope const &scope)
  -> Opt<std::uint64_t> {
  //
  using analyse::utils::type_compare::TypeEq;
  using analyse::utils::type_members::VariantMemberRefs;

  // Index the type in the list of member types of the variant.
  const auto members = VariantMemberRefs(variant, scope);
  for (auto i = 0uz; i < members.size(); ++i) {
    if (analyse::utils::type_compare::Assignable(members[i], member, scope, scope)) { return static_cast<std::uint64_t>(i); }
  }
  return Nullopt;
}

auto spp::codegen::GetVariantPayloadPtr(
  llvm::Value *variant_ptr,
  llvm::Type *variant_llvm_type,
  Str const &name,
  LlvmCtx *ctx)
  -> llvm::Value* {
  // The payload is the second field, behind the discriminant.
  return ctx->Builder.CreateStructGEP(variant_llvm_type, variant_ptr, 1, name);
}

auto spp::codegen::LoadVariantTag(
  llvm::Value *variant_ptr,
  llvm::Type *variant_llvm_type,
  Str const &name,
  LlvmCtx *ctx)
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
  LlvmCtx *ctx)
  -> llvm::Value* {
  // Build into a stack slot, because the payload is written
  // through a pointer rather than by value. The slot starts
  // zeroed, because the member rarely fills the whole payload,
  // and the whole struct is loaded back out at the end: the
  // bytes past the member would otherwise be stale stack data,
  // undef to the optimiser and a disclosure hazard the moment a
  // variant is ever copied out of the program. Everything the
  // "stores" below cover is dead-store-eliminated.
  const auto slot = LlvmEntryAlloca(variant_llvm_type, name + ".slot", ctx);
  ctx->Builder.CreateStore(
    llvm::Constant::getNullValue(variant_llvm_type), slot);

  const auto tag_ptr = ctx->Builder.CreateStructGEP(variant_llvm_type, slot, 0, name + ".tag.ptr");
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(GetVariantTagType(ctx), tag), tag_ptr);

  // A member that lowers to nothing (such as the stateless "None")
  // has no payload; the tag knows the type though.
  if (member_val != nullptr and not IsValuelessType(member_val->getType())) {
    ctx->Builder.CreateStore(member_val, GetVariantPayloadPtr(slot, variant_llvm_type, name + ".payload.ptr", ctx));
  }

  return ctx->Builder.CreateLoad(variant_llvm_type, slot, name);
}

auto spp::codegen::NarrowVariant(
  llvm::Value *source_ptr,
  TypeRef const &source,
  TypeRef const &target,
  Scope const &scope,
  Str const &name,
  LlvmCtx *ctx)
  -> Pair<llvm::Value*, llvm::Value*> {
  //
  using analyse::utils::type_members::VariantMemberRefs;
  using analyse::utils::type_predicates::IsTypeVariant;

  if (not IsTypeVariant(target, scope) or not IsTypeVariant(source, scope)) { return {nullptr, nullptr}; }
  const auto source_llvm_type = GetLlvmTypeOf(source.WithoutConvention(), ctx);
  const auto target_llvm_type = GetLlvmTypeOf(target.WithoutConvention(), ctx);
  if (source_llvm_type == nullptr or target_llvm_type == nullptr) { Ice("variant has no llvm type while narrowing"); }

  // Where each of the target's members sits in the source. Any
  // member the source lacks means this is not a sub-variant.
  auto source_tags = Vec<std::uint64_t>();
  for (auto const &member : VariantMemberRefs(target, scope)) {
    const auto source_tag = GetVariantIndexOfMember(source, member, scope);
    if (not source_tag.has_value()) { return {nullptr, nullptr}; }
    source_tags.emplace_back(*source_tag);
  }
  if (source_tags.empty()) { return {nullptr, nullptr}; }

  // The source holds the target when its tag is any of those.
  // The target's own tag is picked by the same comparisons, as a
  // chain of selects defaulting to its last member.
  const auto tag_type = GetVariantTagType(ctx);
  const auto source_tag = LoadVariantTag(source_ptr, source_llvm_type, name + ".from.tag", ctx);
  auto is_member = static_cast<llvm::Value*>(nullptr);
  auto target_tag = static_cast<llvm::Value*>(llvm::ConstantInt::get(tag_type, source_tags.size() - 1));
  for (auto i = source_tags.size(); i > 0; --i) {
    const auto matches = ctx->Builder.CreateICmpEQ(
      source_tag, llvm::ConstantInt::get(tag_type, source_tags[i - 1]), name + ".from.is");
    is_member = is_member == nullptr ? matches : ctx->Builder.CreateOr(is_member, matches, name + ".is.any");
    if (i < source_tags.size()) {
      target_tag = ctx->Builder.CreateSelect(
        matches, llvm::ConstantInt::get(tag_type, i - 1), target_tag, name + ".to.tag");
    }
  }

  // The target's members are a subset of the source's, so its
  // payload buffer is never the larger, and its size is the
  // amount worth copying.
  const auto target_slot = LlvmEntryAlloca(target_llvm_type, name + ".to.slot", ctx);
  ctx->Builder.CreateStore(llvm::Constant::getNullValue(target_llvm_type), target_slot);
  ctx->Builder.CreateStore(
    target_tag, ctx->Builder.CreateStructGEP(target_llvm_type, target_slot, 0, name + ".to.tag.ptr"));

  if (llvm::cast<llvm::StructType>(target_llvm_type)->getNumElements() < 2) { return {is_member, target_slot}; }
  auto const &dl = ctx->Module->getDataLayout();
  const auto target_payload_type = llvm::cast<llvm::StructType>(target_llvm_type)->getElementType(1);
  ctx->Builder.CreateMemCpy(
    GetVariantPayloadPtr(target_slot, target_llvm_type, name + ".to.payload.ptr", ctx),
    dl.getABITypeAlign(target_payload_type),
    GetVariantPayloadPtr(source_ptr, source_llvm_type, name + ".from.payload.ptr", ctx),
    dl.getABITypeAlign(llvm::cast<llvm::StructType>(source_llvm_type)->getElementType(1)),
    dl.getTypeAllocSize(target_payload_type).getFixedValue());

  return {is_member, target_slot};
}
