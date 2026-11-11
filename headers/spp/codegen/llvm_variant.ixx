module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_variant;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeRef);

namespace spp::codegen {
  /// Get the type of tag on a variant-lowered type. This is always a 64-bit
  /// integer type.
  SPP_EXP_FUN auto GetVariantTagType(LlvmCtx const *ctx) -> llvm::IntegerType*;

  /// Get the index of a type within its variant, such as "None" within
  /// "Opt[S32]" ("Some[S32] or None"). Members are numbered in their type
  /// identity order, not the order they are written in.
  SPP_EXP_FUN auto GetVariantIndexOfMember(
    TypeRef const &variant, TypeRef const &member,
    Scope const &scope) -> Opt<std::uint64_t>;

  /// Get the LLVM pointer of the variant's payload, the variant structured as
  /// "{ tag, payload }". A simple GEP into the field indexed at 1.
  SPP_EXP_FUN auto GetVariantPayloadPtr(
    llvm::Value *llvm_variant_ptr, llvm::Type *llvm_variant_type,
    Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// Get the LLVM pointer of the variant's tag, the variant structured as
  /// "{ tag, payload }". A simple GEP into the field indexed at 0.
  SPP_EXP_FUN auto GetVariantTagPtr(
    llvm::Value *llvm_variant_ptr, llvm::Type *llvm_variant_type,
    Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// Get the LLVM tag of the variant, loaded out of the tag value, used to test
  /// the current variant being held.
  SPP_EXP_FUN auto LoadVariantTag(
    llvm::Value *llvm_variant_ptr, llvm::Type *llvm_variant_type,
    Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// Build a variant from a type such as "A or B", lowered into "{ tag,
  /// payload }". The tag marks the member being held; the payload is as wide
  /// as the widest member. Given a variant can only be initialised by one of
  /// its member types, not "Variant()", load the payload with the provided
  /// value.
  SPP_EXP_FUN auto BuildVariant(
    llvm::Value *llvm_member_val, llvm::Type *llvm_variant_type,
    std::uint64_t index, Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// Store a built variant into a pre-created slot, rather than the default
  /// "BuildVariant" alloca which then gets loaded from into another slot. For
  /// a variant built on several edges and joined: each edge writes the one
  /// slot, and the join loads it once. A phi of whole variants instead hides a
  /// pointer payload from the optimiser as an integer (the payload cells are
  /// "i64"), which stops a generator loop vectorising.
  SPP_EXP_FUN auto StoreVariant(
    llvm::Value *llvm_slot, llvm::Value *llvm_member_val,
    llvm::Type *llvm_variant_type, std::uint64_t index, Str const &name,
    LlvmCtx *ctx) -> void;

  /// Widen the variant at "source_ptr" (of type "source") into type "target", a
  /// variant with all of its members and maybe more (such as "Str or Bool" into
  /// "Str or Bool or S32"). The members may be numbered differently, so the tag
  /// is translated, then the payload moved over. Gives the target's slot; null
  /// if "target" is not such a widening of "source" or either is not a variant.
  SPP_EXP_FUN auto WidenVariant(
    llvm::Value *source_ptr, TypeRef const &source, TypeRef const &target,
    Scope const &scope, Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// Narrow the variant at "source_ptr" (of type "source") into type "target",
  /// a variant containing a subset of source's member types (such as "Some[S32]
  /// or None or GenDone" into "Opt[S32]"). Gives the target's slot, valid only
  /// while the source holds one of the target's members, which the caller
  /// tests (a case pattern's tag check); null if "target" is not such a
  /// sub-variant or either is not a variant.
  SPP_EXP_FUN auto NarrowVariant(
    llvm::Value *source_ptr, TypeRef const &source, TypeRef const &target,
    Scope const &scope, Str const &name, LlvmCtx *ctx) -> llvm::Value*;
}
