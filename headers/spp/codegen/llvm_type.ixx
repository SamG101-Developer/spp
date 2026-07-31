module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_type;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS struct TypeSymbol;
  SPP_EXP_CLS class Scope;
}

namespace spp::asts {
  SPP_EXP_CLS struct ClassPrototypeAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::codegen {
  SPP_EXP_FUN auto RegisterLlvmTypeInfo(
    asts::ClassPrototypeAst const *cls_proto,
    LLvmCtx const *ctx)
    -> void;

  SPP_EXP_FUN auto GetLlvmType(
    analyse::scopes::TypeSymbol const &type_sym,
    LLvmCtx const *ctx)
    -> llvm::Type*;

  /**
   * The "Fun*"/"Gen*" family of compiler-known types always lower to the same { fn_ptr, env_ptr } fat pointer,
   * whether @p type IS one of them, or a class superimposes one of them as an interface (eg "Iterator[T]" over
   * "Gen[T]"). This is the single source of truth for that shape, so "RegisterLlvmTypeInfo" (lowering the type
   * itself) and "ClassPrototypeAst::_FillLlvmLayout" (prepending the shape onto a superimposing class) can't drift
   * apart.
   * @param type The type to test.
   * @param scope The scope to resolve @p type against.
   * @param ctx The LLVM context containing all codegen info.
   * @return The fat pointer's fields, or nothing if @p type is not one of the fat-pointer family.
   */
  SPP_EXP_FUN auto GetFatPointerFields(
    asts::TypeAst const &type,
    analyse::scopes::Scope const &scope,
    LLvmCtx const *ctx)
    -> std::optional<Vec<llvm::Type*>>;

  /**
   * Get the type of the tag on variant lowered struct: the integer type (64-bit).
   * @param ctx The LLVM context containing all codegen info.
   * @return The integer type used for every variant's discriminant.
   */
  SPP_EXP_FUN auto GetVariantTagType(
    LLvmCtx const *ctx)
    -> llvm::IntegerType*;

  /**
   * Get the tag from the variant type. This extracts the @c tag field from the @code { tag, ptr }@endcode struct.
   * @param variant_type The variant type being tagged into.
   * @param member_type The member type to find the discriminant of.
   * @param scope The scope to resolve both types against.
   * @return The member's discriminant, or nothing if the type is not a member of the variant.
   */
  SPP_EXP_FUN auto GetVariantIndexOfMember(
    asts::TypeAst const &variant_type,
    asts::TypeAst const &member_type,
    analyse::scopes::Scope const &scope)
    -> std::optional<std::uint64_t>;

  /**
   * Get the pointer from the variant type. This extracts the @c ptr field from the @code { tag, ptr }@endcode struct.
   * @param variant_ptr The address of the variant value.
   * @param variant_llvm_type The lowered { tag, payload } type of the variant.
   * @param name The name to give the generated address in the ir.
   * @param ctx The LLVM context containing all codegen info.
   * @return The address of the variant's payload buffer.
   */
  SPP_EXP_FUN auto GetVariantPayloadPtr(
    llvm::Value *variant_ptr,
    llvm::Type *variant_llvm_type,
    Str const &name,
    LLvmCtx *ctx)
    -> llvm::Value*;

  /**
   * Load the discriminant out of a variant, to test which member it currently holds.
   * @param variant_ptr The address of the variant value.
   * @param variant_llvm_type The lowered { tag, payload } type of the variant.
   * @param name The name to give the loaded discriminant in the ir.
   * @param ctx The LLVM context containing all codegen info.
   * @return The variant's discriminant, comparable against @c GetVariantTag.
   */
  SPP_EXP_FUN auto LoadVariantTag(
    llvm::Value *variant_ptr,
    llvm::Type *variant_llvm_type,
    Str const &name,
    LLvmCtx *ctx)
    -> llvm::Value*;

  /**
   * A variant such as @code A or B@endcode becomes a { tag, payload } pair. The tag marks which member the payload
   * currently holds and the payload is a buffer sized for the largest member. The width matches the what @c SizeOf
   * calculates, keeping uniformity. Wrap one of a variant's members into the variant itself, by tagging it and copying
   * it into the payload buffer. This is the only way a variant value comes into existence: S++ doesn't allow variants
   * to be initialized directly; a variant is always built from a value of one of its member types.
   * @param member_val The member's value, or nullptr for a member that lowers to nothing.
   * @param variant_llvm_type The lowered { tag, payload } type of the variant.
   * @param tag The member's discriminant, from @c GetVariantTag.
   * @param name The name to give the generated value in the ir.
   * @param ctx The LLVM context containing all codegen info.
   * @return The built variant value.
   */
  SPP_EXP_FUN auto BuildVariant(
    llvm::Value *member_val,
    llvm::Type *variant_llvm_type,
    std::uint64_t tag,
    Str const &name,
    LLvmCtx *ctx)
    -> llvm::Value*;

  /**
   * Coerce a value into the variant it is being assigned to, which is the single entry point every site storing into
   * a variant slot should go through (a @c ret, a case branch, a @c let, an argument). It covers the two ways
   * @c TypeEq admits a value into a variant: wrapping one of its members, and widening a narrower variant whose
   * members are a subset of this one's. Widening is not a copy, because the two variants number their shared members
   * independently, so the discriminant is remapped as well as the payload moved.
   * @param llvm_val The value being assigned, lowered as its own type.
   * @param target_type The variant type being assigned into.
   * @param source_type The type of @p llvm_val.
   * @param scope The scope to resolve both types against.
   * @param name The name to give the generated value in the ir.
   * @param ctx The LLVM context containing all codegen info.
   * @return The coerced value, or @p llvm_val unchanged when the target is not a variant needing one.
   */
  SPP_EXP_FUN auto CoerceToVariant(
    llvm::Value *llvm_val,
    asts::TypeAst const &target_type,
    asts::TypeAst const &source_type,
    analyse::scopes::Scope const &scope,
    Str const &name,
    LLvmCtx *ctx)
    -> llvm::Value*;
}
