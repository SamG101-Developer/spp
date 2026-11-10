module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_abi;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_sym_info;
import spp.utils.types;
import llvm;
import std;

use(spp::codegen, class Abi);
use(spp::codegen, struct AbiParam);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct FunctionPrototypeAst);

/// How one by-value parameter crosses a convention ("How"), what it is
/// declared as ("Declared"), and whether the caller extends it to its register
/// ("Extend"). A C struct here is one of two or more fields; a struct of one
/// field stands for that field's scalar ("NonNull" is a pointer).
/// - Direct: passed as its LLVM value is. Every S++ parameter, and a C one
///   most targets take as it is (a small C struct passed as its fields).
/// - Unwrap: a one-field struct, declared and passed as the scalar inside it,
///   on every target: the C form, and what lets LLVM read a pointer as a
///   pointer.
/// - ByVal: a C struct passed through a pointer to a copy the callee may write
///   ("byval"), as 32-bit PowerPC and m68k take one.
/// - Indirect: a C struct passed through a pointer to a copy the caller makes,
///   as s390x and Windows x64 take one not of 1, 2, 4 or 8 bytes.
/// - Coerce: a C struct of 1, 2, 4 or 8 bytes passed as an integer of that
///   size, as s390x and Windows x64 take one.
SPP_EXP_CLS struct spp::codegen::AbiParam {
  enum class Kind { Direct, Unwrap, ByVal, Indirect, Coerce };

  /// How the parameter crosses, as above.
  Kind How = Kind::Direct;

  /// The parameter's LLVM value type, as S++ lays it out.
  llvm::Type *Value = nullptr;

  /// Its type in the declaration: "Value", a pointer, or an integer of
  /// "Value"'s size.
  llvm::Type *Declared = nullptr;

  /// "SExt" or "ZExt" for a small integer the caller extends to its register,
  /// as the target's C convention says, else "None".
  llvm::Attribute::AttrKind Extend = llvm::Attribute::None;
};

/// A convention, named by "!conv" ("!ffi" implies "C"): how a class it is on
/// lays out its fields, and how a function it is on takes its parameters and
/// returns its value. Every convention is one entry in a fixed table
/// ("FindAbi"); the default for a class or function with no "!conv" is "spp",
/// and tuples are always "C". Called "Abi" in the compiler, as a "convention"
/// there is a borrow's ("&", "&mut").
///
/// "packed" (declaration order, no padding) is not an entry yet: a borrow of a
/// packed class's field would still be given its type's "align", which it does
/// not have (see the borrow attributes in "GenerateLlvmDeclaration").
SPP_EXP_CLS class spp::codegen::Abi {
public:
  virtual ~Abi() = default;

  /// The name "!conv" takes.
  SPP_ATTR_NODISCARD virtual auto Name() const -> StrView = 0;

  /// Whether a function declared with it is implemented outside S++, so
  /// only "!ffi" functions may take it, and they may take nothing else.
  SPP_ATTR_NODISCARD virtual auto IsForeign() const -> bool = 0;

  /// Give "type" its body from "fields", the lowered types of a class's
  /// attributes (or a tuple's elements) in declaration order, recording in
  /// "info" where each declared field ended up. A field that carries no
  /// value is not laid out. A body already set is left alone.
  virtual auto LayOutStruct(
    llvm::StructType &type, Vec<llvm::Type*> const &fields,
    LlvmTypeSymbolInfo &info, LlvmCtx const &ctx) const -> void = 0;

  /// The declared form of each parameter of a function whose LLVM value
  /// types are "value_types", "is_signed" saying which are signed integers.
  SPP_ATTR_NODISCARD virtual auto ParamsOf(
    Vec<llvm::Type*> const &value_types, Vec<bool> const &is_signed,
    llvm::Module const &module) const -> Vec<AbiParam> = 0;

  /// The type a function declares it returns, for a return of LLVM value
  /// type "type".
  SPP_ATTR_NODISCARD virtual auto ReturnOf(
    llvm::Type *type) const -> llvm::Type* = 0;

  /// The attributes "ParamsOf" and "ReturnOf" call for on "fn", declared with
  /// each parameter's "Declared" type and the return "ReturnOf" gave, which is
  /// a signed integer if "ret_signed".
  virtual auto AddAttrs(
    llvm::Function &fn, Vec<AbiParam> const &params,
    bool ret_signed) const -> void = 0;

  /// Pass "args" to "callee" as its declaration takes them.
  virtual auto AdaptArgs(
    llvm::Function const &callee, Vec<llvm::Value*> &args,
    LlvmCtx *ctx) const -> void = 0;

  /// The value of a call's "result", as the S++ type it returns (LLVM type
  /// "expected").
  SPP_ATTR_NODISCARD virtual auto AdaptReturn(
    llvm::Value *result, llvm::Type *expected,
    LlvmCtx *ctx) const -> llvm::Value* = 0;
};

namespace spp::codegen {
  /// The convention "!conv" names "name", or null for none.
  SPP_EXP_FUN auto FindAbi(StrView name) -> Abi const*;

  /// Every convention's name, or only the foreign ones' (or only the others')
  /// if "foreign" says: for the closest match to a misspelt one.
  SPP_EXP_FUN auto AbiNames(Opt<bool> foreign = Nullopt) -> Vec<Str>;

  /// The default convention, for anything with no "!conv".
  SPP_EXP_FUN auto GetSppAbi() -> Abi const&;

  /// The target's C convention: "!ffi", and every tuple.
  SPP_EXP_FUN auto GetCAbi() -> Abi const&;

  /// The convention a class is laid out by: its "!conv", "C" for an "!ffi" one,
  /// else the default.
  SPP_EXP_FUN auto AbiOf(TypeSymbol const &cls) -> Abi const&;

  /// The convention a function is called with: its "!conv", "C" for an "!ffi"
  /// one, else the default.
  SPP_EXP_FUN auto AbiOf(FunctionPrototypeAst const &fn) -> Abi const&;

  /// Whether a value of LLVM type "type" crosses to C as it is: an integer, a
  /// float, a pointer, an array of those, or a struct of one field that stands
  /// for one (however deeply wrapped). A struct of two or more fields only
  /// crosses if its own convention is C, which only the caller knows.
  SPP_EXP_FUN auto IsCScalar(llvm::Type const *type) -> bool;
}
