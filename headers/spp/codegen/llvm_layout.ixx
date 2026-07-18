module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_layout;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import ankerl;
import llvm;
import std;

namespace spp::codegen {
    /**
     * Different available layouts that S++ will support, using the `!layout(conv="C")` syntax. Once enums are
     * supported, the accepted convention type will be enum based. This enum will be consistent with the S++ available
     * layouts too. The FFI structs always use the "C" or "packed" layouts.
     */
    SPP_EXP_CLS enum class StructLayout { Spp, C, Packed };

    /**
     * Apply the S++ layout convention to LLVM struct field types. It works in the same way as the Rust convention,
     * where it re-orders the members to minimize padding and therefore overall object size.
     * @param field_types The LLVM types belonging to each field of a struct.
     * @param ctx The LLVM context containing all codegen info.
     * @return The sorted types and a index mapper for the class prototype to use.
     */
    SPP_EXP_FUN auto SortMembersForSppLayout(
        Vec<llvm::Type*> const &field_types,
        spp::codegen::LLvmCtx const *ctx)
        -> Pair<Vec<llvm::Type*>, ankerl::unordered_dense::map<std::size_t, std::size_t>>;
}
