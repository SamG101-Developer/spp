module;
#include <spp/macros.hpp>

module spp.codegen.llvm_cfi;
import spp.analyse.scopes.symbols;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_target;
import spp.utils.features;
import spp.utils.ice;
import spp.utils.types;
import llvm;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, TypeRef);

namespace spp::codegen {
  namespace {
    /// The control-flow-integrity type a function is called as ("StampCfiFn").
    constexpr char kCfiMd[] = "spp.cfi";

    /// A coroutine's resume and destroy types, which the functions its lowering
    /// splits it into keep ("StampCfiCoroutine").
    constexpr char kCfiCoroMd[] = "spp.cfi.coro";

    /// A 32-bit FNV-1a hash: stable across builds and platforms,
    /// so a build stays reproducible.
    auto Fnv32(const StrView text) -> std::uint32_t {
      auto hash = 2166136261u;
      for (const auto c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 16777619u;
      }
      return hash;
    }

    /// Metadata holding "types", each as an "i32".
    auto CfiMd(llvm::LLVMContext &ctx, const std::initializer_list<std::uint32_t> types) -> llvm::MDNode* {
      auto ops = spp::Vec<llvm::Metadata*>();
      const auto i32 = llvm::Type::getInt32Ty(ctx);
      for (const auto t : types) {
        ops.emplace_back(llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(i32, t)));
      }
      return llvm::MDNode::get(ctx, ops);
    }

    /// The control-flow-integrity type of one of a generator's two
    /// functions ("FindGenSup").
    auto CfiGenType(
      TypeRef const &gen_sup, const CoroEdge edge) -> std::uint32_t {
      const auto which = edge == CoroEdge::Destroy ? "destroy " : "resume ";
      return Fnv32(which + mangle::MangleTypeName(*gen_sup.Symbol));
    }

    /// The control-flow-integrity type of a closure's capture
    /// destructor ("void(ptr env)"), which every closure shares.
    auto CfiClosureEnvDropType() -> std::uint32_t {
      return Fnv32("closure env drop");
    }

    /// The "kcfi" operand bundle checking for "type"; none when
    /// the feature is off.
    auto KcfiBundles(
      const std::uint32_t type, LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef> {
      if (not utils::features::IsEnabled(utils::features::ConfigKey::BinaryCodegenCfi)) { return {}; }
      auto bundles = Vec<llvm::OperandBundleDef>();
      bundles.emplace_back(
        "kcfi", Vec<llvm::Value*>{
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), type)
        });
      return bundles;
    }

    /// Whether the selected target can check indirect calls by
    /// type (KCFI): x86-64 and AArch64 can.
    auto TargetHasKcfi() -> bool {
      return TargetTriple().getArch() == llvm::Triple::x86_64 or TargetTriple().isAArch64();
    }

    /// The type operand "i" of a control-flow-integrity record
    /// holds.
    auto CfiTypeAt(llvm::MDNode const &md, const unsigned i) -> llvm::ConstantInt* {
      return llvm::mdconst::extract<llvm::ConstantInt>(md.getOperand(i));
    }

    /// Give every function in "llvm_mod" with a control-flow record
    /// its "kcfi_type": the type it is called as. The functions a
    /// coroutine is split into share its record, and are told apart
    /// by the suffix the split names them with: the frame's resume
    /// slot holds ".resume", and its destroy slot ".destroy", or
    /// ".cleanup" when the frame was not allocated. A ramp has no
    /// suffix and is left untyped: it is only ever called directly.
    auto TypeCfiFunctions(llvm::Module &llvm_mod) -> void {
      auto &ctx = llvm_mod.getContext();
      for (auto &fn : llvm_mod) {
        auto type = static_cast<llvm::ConstantInt*>(nullptr);
        if (const auto md = fn.getMetadata(kCfiMd)) { type = CfiTypeAt(*md, 0); }
        else if (const auto coro = fn.getMetadata(kCfiCoroMd)) {
          const auto name = fn.getName();
          if (name.ends_with(".resume")) { type = CfiTypeAt(*coro, 0); }
          else if (name.ends_with(".destroy") or name.ends_with(".cleanup")) { type = CfiTypeAt(*coro, 1); }
        }
        if (type != nullptr) {
          fn.setMetadata(llvm::LLVMContext::MD_kcfi_type, llvm::MDNode::get(ctx, llvm::ConstantAsMetadata::get(type)));
        }
      }
    }
  }
}

/// [CHECKED]
auto spp::codegen::CfiFnType(
  TypeRef const &fn_sup) -> std::uint32_t {
  // Get the "Args" tuple type, the "Out" return type,
  // mangled and hashed for CFI.
  const auto args = fn_sup.Symbol->TypeArgRef("Args");
  const auto out = fn_sup.Symbol->TypeArgRef("Out");
  return Fnv32(
    "fn " + mangle::MangleTypeName(*args.Symbol) +
    " -> " + mangle::MangleTypeName(*out.Symbol));
}

/// [CHECKED]
auto spp::codegen::StampCfiFn(
  llvm::Function &fn, TypeRef const &fn_sup) -> void {
  // Stamp the CFI hashed type ("CfiFnType") onto the function
  // in its metadata, for later retrieval.
  fn.setMetadata(kCfiMd, CfiMd(fn.getContext(), {CfiFnType(fn_sup)}));
}

/// [CHECKED]
auto spp::codegen::StampCfiCoroutine(
  llvm::Function &ramp, TypeRef const &gen_sup) -> void {
  // Stamp the CFI hashed type ("CfiGenType") onto the function
  // in its metadata, for later retrieval.
  const auto resume = CfiGenType(gen_sup, CoroEdge::Resume);
  const auto destroy = CfiGenType(gen_sup, CoroEdge::Destroy);
  ramp.setMetadata(kCfiCoroMd, CfiMd(ramp.getContext(), {resume, destroy}));
}

auto spp::codegen::CfiFnCallBundles(
  TypeRef const &fn_sup,
  LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef> {
  return KcfiBundles(CfiFnType(fn_sup), ctx);
}

auto spp::codegen::CfiGenCallBundles(
  TypeRef const &gen_sup, const CoroEdge edge,
  LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef> {
  return KcfiBundles(CfiGenType(gen_sup, edge), ctx);
}

auto spp::codegen::StampCfiClosureEnvDrop(
  llvm::Function &fn) -> void {
  // Every capture destructor has the one type, so one checks as another.
  fn.setMetadata(kCfiMd, CfiMd(fn.getContext(), {CfiClosureEnvDropType()}));
}

auto spp::codegen::CfiClosureEnvDropCallBundles(
  LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef> {
  return KcfiBundles(CfiClosureEnvDropType(), ctx);
}

auto spp::codegen::ApplyCfiTypes(
  void *llvm_module) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  if (not TargetHasKcfi()) {
    llvm::errs()
      << "This target cannot check indirect calls by type.\n"
      << "  To build without it, set 'cfi = false' under [binary.codegen] in spp.toml.\n";
    return false;
  }

  // The functions that exist before optimising: the coroutines
  // are not split yet, so their parts are typed by
  // "ApplyCfiCoroTypes" once they are.
  TypeCfiFunctions(llvm_mod);

  // The backend emits each function's type, and each call's
  // check, only for a module that asks for them.
  llvm_mod.addModuleFlag(llvm::Module::Override, "kcfi", 1);
  return true;
}

auto spp::codegen::ApplyCfiCoroTypes(
  void *llvm_module) -> void {
  // Retyping the functions typed before is harmless; the module
  // flag is not added again, as a second one fails verification.
  TypeCfiFunctions(*static_cast<llvm::Module*>(llvm_module));
}

auto spp::codegen::FinishCfi(
  void *llvm_module) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);

  // On Mach-O the type word before a function belongs to the atom
  // before it, which ld64 may move (a cold function, either side).
  // Prefix data gets a label of its own and turns the function into
  // an .alt_entry of it, so the word after it (the type) stays in the
  // function's atom. ELF keeps the word in the function's section.
  const auto anchor_types = TargetTriple().isOSBinFormatMachO();
  const auto anchor = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_mod.getContext()), 0);

  for (auto &fn : llvm_mod) {
    if (fn.isDeclaration()) { continue; }
    if (anchor_types and not fn.hasPrefixData() and fn.hasMetadata(llvm::LLVMContext::MD_kcfi_type)) {
      fn.setPrefixData(anchor);
    }

    // The calls carry their checks from where they were emitted
    // ("CfiFnCallBundles", "CfiGenCallBundles"). One that does not
    // would be the call an attacker could take anywhere, so it
    // stops the build.
    for (auto &inst : llvm::instructions(fn)) {
      const auto call = llvm::dyn_cast<llvm::CallBase>(&inst);
      if (call == nullptr or not call->isIndirectCall()) { continue; }
      if (not call->getOperandBundle(llvm::LLVMContext::OB_kcfi).has_value()) {
        spp::Ice("an indirect call in " + fn.getName().str() + " has no control-flow-integrity type");
      }
    }
  }
}
