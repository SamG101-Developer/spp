module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_debug;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_sym_info;
import spp.utils.features;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, struct TypeRef);

/// The debug info a module carries (DWARF; "[binary.debug] info"): a compile
/// unit per s++ module, a subprogram per function, and a location on every
/// instruction, so a debugger and a crash report can name the source line. The
/// language is "DW_LANG_Rust" until S++ has a registered code: a vendor code
/// left LLDB showing no variables at all, and Rust's rules suit S++ best in
/// gdb (numbers as numbers, no byte pointer read as a C string). Only how a
/// debugger shows values and reads expressions follows from it.
namespace spp::codegen {
  /// Setup the LLVM context's debug info fields, and add any additional flags
  /// specific to debugging. The "level" indicates "full" for debugging in "dev"
  /// mode, and "lines" otherwise.
  SPP_EXP_FUN auto StartDebugInfo(
    LlvmCtx &ctx, StrView path, Path const &root,
    utils::features::DebugInfo level) -> void;

  /// Run the "finalisation" function on the debug information builder, which
  /// invokes the teardown logic, resolving any pending debug info, allowing
  /// verification and linking. After codegen, before IR is read.
  SPP_EXP_FUN auto FinishDebugInfo(
    LlvmCtx const &ctx) -> void;

  /// The file entry for the source at "path", made once per module. Re-query
  /// returns the existing pointer for the path (create and store if new).
  SPP_EXP_FUN auto DebugFileOf(
    LlvmCtx &ctx, StrView path) -> llvm::DIFile*;

  /// Give "fn" its subprogram, at the line of "position" (an ast's start), by
  /// its S++ name (no linkage name with line tables only). A position with no
  /// line (generated code) makes an artificial one at line 0, which a debugger
  /// steps over.
  SPP_EXP_FUN auto AttachSubprogram(
    LlvmCtx &ctx, llvm::Function &fn, std::size_t position) -> void;

  /// Update the current debug location so that any debug information emitted
  /// lands in the line/column "position" of the target block, in the innermost
  /// lexical block being generated in the builder's function.
  SPP_EXP_FUN auto SetDebugLocation(
    LlvmCtx &ctx, std::size_t position) -> void;

  /// Open a lexical block at "position" (a block's start), inside the innermost
  /// one of the builder's function, or its subprogram. This is the scope the
  /// variables and locations of a block's members are placed in, so a debugger
  /// shows a variable only inside its block, and a shadowing one in place of
  /// the one it shadows.
  SPP_EXP_FUN auto EnterDebugBlock(
    LlvmCtx &ctx, std::size_t position) -> void;

  /// Close the lexical block the matching "EnterDebugBlock" opened, and go back
  /// to the location current when it opened. This will be the statement the
  /// block is part of, which the code after it belongs to.
  SPP_EXP_FUN auto LeaveDebugBlock(
    LlvmCtx &ctx) -> void;

  /// The debug type of an S++ type, made once per module: a basic type for a
  /// number or "Bool"; a pointer for a borrow ("&T", "&mut T"), "NonNull[T]"
  /// and "Ptr[T]"; an array for "Arr[T, n]"; a structure of its fields, at
  /// their offsets in the type's own layout, for a class or a tuple; and for a
  /// variant, its tag, an enumeration naming each member, beside a union of
  /// the members. A type it cannot describe (one with no layout) is a structure
  /// of its size with no fields. Nothing for "Void".
  SPP_EXP_FUN auto DebugTypeOf(
    LlvmCtx &ctx, TypeRef const &ref) -> llvm::DIType*;

  /// Describe the variable "name", held in "info"'s stack slot, of type "ref",
  /// declared at "position", for a debugger to show. A parameter is numbered
  /// ("arg_no", from 1); 0 for any other variable. A binding that can be moved
  /// (its type is not "Copy", nor a borrow) is not described for its whole
  /// block, but from where it is given a value ("ResumeVariable") to where it
  /// is moved ("EndVariable"); "info" keeps its variable for those.
  SPP_EXP_FUN auto DeclareVariable(
    LlvmCtx &ctx, LlvmVarSymbolInfo &info, StrView name, TypeRef const &ref,
    std::size_t position, unsigned arg_no) -> void;

  /// A binding moved here: a debugger shows it as not available from here on,
  /// rather than the value it no longer holds. Nothing for a binding that
  /// cannot be moved.
  SPP_EXP_FUN auto EndVariable(
    LlvmCtx &ctx, LlvmVarSymbolInfo const &info) -> void;

  /// A binding given a value here (an assignment): a debugger shows it again,
  /// after a move ended it. Nothing for a binding that cannot be moved.
  SPP_EXP_FUN auto ResumeVariable(
    LlvmCtx &ctx, LlvmVarSymbolInfo const &info) -> void;
}
