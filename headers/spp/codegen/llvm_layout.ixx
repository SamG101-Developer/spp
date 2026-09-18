module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_layout;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_sym_info;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  /// The layouts S++ supports, chosen with the
  /// "!layout(conv="C")" syntax. Once enums are supported, the
  /// accepted convention will be enum based, matching this
  /// enum. FFI structs always use the "C" or "packed" layouts.
  SPP_EXP_CLS enum class StructLayout { Spp, C, Packed };

  /// The declaration indices of the fields that are actually
  /// laid out, in declaration order, given the lowered types
  /// of the struct's fields in declaration order.
  SPP_EXP_FUN auto DropValuelessFields(
    Vec<llvm::Type*> const &field_types)
    -> Vec<std::size_t>;

  /// Apply the S++ layout convention to a struct's field
  /// types. Like Rust's, it re-orders the members to minimise
  /// padding and so the overall object size. Answers the
  /// sorted types, and an index map for the class prototype.
  SPP_EXP_FUN auto SortMembersForSppLayout(
    Vec<llvm::Type*> const &field_types,
    LlvmCtx const *ctx)
    -> Pair<Vec<llvm::Type*>, Map<std::size_t, std::size_t>>;

  /// Translate an attribute's declaration index (its position
  /// in "GetAllAttrs") into the physical field index of the
  /// generated llvm struct, using the owning type symbol's llvm
  /// info. The S++ layout re-orders fields to minimise padding,
  /// so the two differ; layouts that keep declaration order
  /// leave "FieldIndexMap" empty, which is the identity map.
  SPP_EXP_FUN auto GetPhysicalFieldIndex(
    LlvmTypeSymInfo const &sym_info,
    std::size_t decl_index)
    -> std::uint32_t;
}
