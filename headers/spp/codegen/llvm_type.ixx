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
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
  SPP_EXP_CLS struct ClassPrototypeAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::codegen {
  /**
   * Lower the type from the class prototype ast into the @c llvm::Type* inside the llvm system.
   */
  SPP_EXP_FUN auto RegisterLlvmTypeInfo(
    asts::ClassPrototypeAst const *cls_proto,
    analyse::scopes::ScopeManager const &sm,
    LlvmCtx const *ctx)
    -> void;

  /**
   * Lower the type owned by @p scope , filling @c scope->TySym 's llvm type. Taking the scope rather than a class
   * prototype matters for a generic instantiation: its symbol's @c Type still names the *template* it instantiates
   * (see @c CreateGenericClsScope ), so deriving the target from the prototype fills the generic symbol and leaves the
   * instantiation's empty. Callers that hold the instantiation's scope should come through here.
   */
  SPP_EXP_FUN auto RegisterLlvmTypeInfo(
    analyse::scopes::Scope const *scope,
    analyse::scopes::ScopeManager const &sm,
    LlvmCtx const *ctx)
    -> void;

  SPP_EXP_FUN auto GetLlvmType(
    analyse::scopes::TypeSymbol const &type_sym,
    LlvmCtx const *ctx)
    -> llvm::Type*;

  /**
   * Bring @p type_sym all the way down to a type whose size the DataLayout can answer for: registered if it has not
   * been lowered at all, and given its struct body if it is still the opaque placeholder the Stage10 walk has yet to
   * reach. Laying a type out needs the sizes of whatever it contains, and the walk visits types in module order
   * rather than in dependency order, so anything that needs a size reaches for it through here instead of waiting.
   * @param[in] type_sym The symbol to complete.
   * @param[in] sm The scope manager.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EnsureLlvmTypeComplete(
    analyse::scopes::TypeSymbol const &type_sym,
    analyse::scopes::ScopeManager const &sm,
    LlvmCtx const *ctx)
    -> void;

  /**
   * Whether a value of this lowered type has any runtime representation at all.
   *
   * @n
   * @c Void is the case that matters: llvm has no value of type void, so there is nothing to store, nothing to load
   * and nothing to hand to a phi. Code generation represents such a value as a null @c llvm::Value* - the same thing
   * a statement evaluates to - so a producer returns null for one and a consumer skips the store, load or incoming
   * edge it would otherwise make. A generic instantiated at @c Void reaches all of these without anything unusual
   * being written: the @c "val" bound by destructuring a @c "Pass[T=Void]" is exactly such a value.
   *
   * @param[in] type The lowered type, which may be null when the type did not lower at all.
   */
  SPP_EXP_FUN auto IsValuelessType(
    llvm::Type const *type)
    -> bool;

  SPP_EXP_FUN auto GetLlvmTypeOf(
    asts::TypeAst const &type,
    analyse::scopes::Scope const &scope,
    LlvmCtx const *ctx)
    -> llvm::Type*;

  /**
   * The "Fun*"/"Gen*" family of compiler-known types always lower to the same { fn_ptr, env_ptr } fat pointer,
   * whether @p type IS one of them, or a class superimposes one of them as an interface (eg "Iterator[T]" over
   * "Gen[T]"). This is the single source of truth for that shape, so "RegisterLlvmTypeInfo" (lowering the type
   * itself) and "ClassPrototypeAst::FillLlvmLayout" (prepending the shape onto a superimposing class) can't drift
   * apart.
   * @param type The type to test.
   * @param scope The scope to resolve @p type against.
   * @param ctx The LLVM context containing all codegen info.
   * @return The fat pointer's fields, or nothing if @p type is not one of the fat-pointer family.
   */
  SPP_EXP_FUN auto GetFatPointerFields(
    asts::TypeAst const &type,
    analyse::scopes::Scope const &scope,
    LlvmCtx const *ctx)
    -> std::optional<Vec<llvm::Type*>>;
}
