module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_variant;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::asts, struct TypeAst);

namespace spp::codegen {
  /// Get the type of a tag on a variant-lowered type. This is
  /// always a 64-bit integer type.
  SPP_EXP_FUN auto GetVariantTagType(LlvmCtx const *ctx) -> llvm::IntegerType*;

  /// Get the index of a type within its variant. For example,
  /// the definition "type Opt[S32] = Some[S32] or None" means
  /// that "None" is at index 1 for the "Opt" variant type.
  SPP_EXP_FUN auto GetVariantIndexOfMember(
    TypeAst const &variant_type, TypeAst const &member_type, Scope const &scope) -> std::optional<std::uint64_t>;

  /// Get the LLVM pointer of the variant, which is structured as
  /// "{ tag, ptr }". A simple GEP into the field indexed at 1.
  SPP_EXP_FUN auto GetVariantPayloadPtr(
    llvm::Value *variant_ptr, llvm::Type *variant_llvm_type, Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// Get the LLVM tag of the variant, loaded out of the tag
  /// value, used to test the current variant being held.
  SPP_EXP_FUN auto LoadVariantTag(
    llvm::Value *variant_ptr, llvm::Type *variant_llvm_type, Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// Build a variant from a type such as "A or B", lowered into
  /// "{ tag, ptr }" shape. The tag marks the current variant
  /// being held. They pointer's value is the width of the widest
  /// type, calculated by the "SizeOf" function. Given a variant
  /// can only be initialized by one of the member types, not
  /// "Variant()", load the payload with the provided value.
  SPP_EXP_FUN auto BuildVariant(
    llvm::Value *member_val,
    llvm::Type *variant_llvm_type,
    std::uint64_t tag,
    Str const &name,
    LlvmCtx *ctx)
    -> llvm::Value*;

  /// Coerce a value into the variant it is being assigned to,
  /// which is the single entry point every site storing into
  /// a variant slot should go though - like "ret", "let", etc.
  /// This covers the two ways that a variant can be supplied
  /// a value: either placing a "S32" into a "S32 or Str"; or
  /// placing a "S32 or Str" into a "S32 or Str or Bool" (a
  /// guaranteed subset).
  SPP_EXP_FUN auto CoerceToVariant(
    llvm::Value *llvm_val,
    TypeAst const &target_type,
    TypeAst const &source_type,
    Scope const &scope,
    Str const &name,
    LlvmCtx *ctx)
    -> llvm::Value*;
}
