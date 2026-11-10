module;
#include <spp/macros.hpp>

module spp.codegen.llvm_abi;
import spp.analyse.scopes.symbols;
import spp.asts.function_prototype_ast;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_target;
import spp.utils.types;
import genex;
import std;

namespace spp::codegen {
  namespace {
    /// Whether @p type is a C struct, as a by-value parameter: a
    /// struct of two or more fields. One of a single field is the
    /// scalar inside it, however deeply wrapped.
    auto IsCStruct(llvm::Type const *type) -> bool {
      while (const auto st = llvm::dyn_cast<llvm::StructType>(type)) {
        if (st->getNumElements() != 1) { return st->getNumElements() > 1; }
        type = st->getElementType(0);
      }
      return false;
    }

    /// The C scalar a struct of one field stands for (the field,
    /// however deeply wrapped), or null for anything else.
    auto CScalarOf(llvm::Type *type) -> llvm::Type* {
      if (not llvm::isa<llvm::StructType>(type)) { return nullptr; }
      while (const auto st = llvm::dyn_cast<llvm::StructType>(type)) {
        if (st->getNumElements() != 1) { return nullptr; }
        type = st->getElementType(0);
      }
      return type;
    }

    /// The indices from a one-field struct chain of @p type down to
    /// its scalar: one 0 per level.
    auto CScalarPath(llvm::Type const *type) -> Vec<unsigned> {
      auto path = Vec<unsigned>();
      while (const auto st = llvm::dyn_cast<llvm::StructType>(type)) {
        path.emplace_back(0u);
        type = st->getElementType(0);
      }
      return path;
    }

    /// Whether the target's C convention extends an integer of
    /// @p type (signed if @p is_signed) to the register it is
    /// passed or returned in, and how; "None" for no integer or no
    /// need. Every target extends "char", "short" and "bool"; 64-bit
    /// PowerPC, s390x and SPARC extend "int" by its signedness, and
    /// RISC-V, LoongArch and MIPS sign-extend it either way. A
    /// caller that leaves the upper half as it was hands a callee
    /// that reads the whole register a wrong value.
    auto CExtendOf(llvm::Type const *type, const bool is_signed) -> llvm::Attribute::AttrKind {
      // As clang marks a C prototype's integers for each target.
      const auto integer = llvm::dyn_cast<llvm::IntegerType>(type);
      if (integer == nullptr) { return llvm::Attribute::None; }
      const auto by_sign = is_signed ? llvm::Attribute::SExt : llvm::Attribute::ZExt;
      const auto width = integer->getBitWidth();
      if (width == 1) { return llvm::Attribute::ZExt; }
      if (width < 32) { return by_sign; }
      if (width > 32) { return llvm::Attribute::None; }

      switch (TargetTriple().getArch()) {
        case llvm::Triple::ppc64:
        case llvm::Triple::ppc64le:
        case llvm::Triple::systemz:
        case llvm::Triple::sparcv9:
          return by_sign;
        case llvm::Triple::riscv64:
        case llvm::Triple::loongarch64:
        case llvm::Triple::mips:
        case llvm::Triple::mipsel:
        case llvm::Triple::mips64:
        case llvm::Triple::mips64el:
          return llvm::Attribute::SExt;
        default:
          return llvm::Attribute::None;
      }
    }

    /// Lay "fields" out in declaration order, natural padding
    /// between them ("packed" would drop it): the C order. An
    /// empty field map means the declaration order was kept
    /// outright, which it only is when nothing was dropped.
    auto LayOutInOrder(
      llvm::StructType &type, Vec<llvm::Type*> const &fields,
      LlvmTypeSymbolInfo &info, const bool packed) -> void {
      const auto kept = DropValuelessFields(fields);
      auto kept_types = Vec<llvm::Type*>();
      auto kept_map = Map<std::size_t, std::size_t>();
      for (auto new_idx = 0uz; new_idx < kept.size(); ++new_idx) {
        kept_types.emplace_back(fields[kept[new_idx]]);
        kept_map[kept[new_idx]] = new_idx;
      }
      if (type.isOpaque()) { type.setBody(kept_types, packed); }
      if (kept.size() != fields.size()) { info.FieldIndexMap = std::move(kept_map); }
      else { info.FieldIndexMap.clear(); }
    }

    /// The S++ convention: fields re-ordered to minimise padding,
    /// and parameters and returns passed as their LLVM values are.
    class SppAbiImpl final : public Abi {
    public:
      auto Name() const -> StrView override { return "spp"; }
      auto IsForeign() const -> bool override { return false; }

      auto LayOutStruct(
        llvm::StructType &type, Vec<llvm::Type*> const &fields,
        LlvmTypeSymbolInfo &info, LlvmCtx const &ctx) const -> void override {
        // Record where each declared attribute ended up, so that
        // codegen can map a declaration index to its physical one.
        auto [sorted_types, index_map] = SortMembersForSppLayout(fields, &ctx);
        if (type.isOpaque()) { type.setBody(sorted_types, false); }
        info.FieldIndexMap = std::move(index_map);
      }

      auto ParamsOf(
        Vec<llvm::Type*> const &value_types, Vec<bool> const &,
        llvm::Module const &) const -> Vec<AbiParam> override {
        return value_types
          | genex::views::transform([](auto *type) { return AbiParam{.Value = type, .Declared = type}; })
          | genex::to<Vec>();
      }

      auto ReturnOf(llvm::Type *type) const -> llvm::Type* override { return type; }
      auto AddAttrs(llvm::Function &, Vec<AbiParam> const &, bool) const -> void override {}
      auto AdaptArgs(llvm::Function const &, Vec<llvm::Value*> &, LlvmCtx *) const -> void override {}
      auto AdaptReturn(llvm::Value *result, llvm::Type *, LlvmCtx *) const -> llvm::Value* override { return result; }
    };

    /// The target's C convention, as clang lowers a C prototype for
    /// it (checked against "clang --target=<t> -S -emit-llvm"):
    /// fields in declaration order; a struct parameter "byval", as
    /// an integer or through a copy where the target says; a
    /// one-field struct as its scalar, in and out; and a small
    /// integer extended to its register. Todo: a returned C struct
    /// ("sret") is not lowered; sppc returns none.
    class CAbiImpl final : public Abi {
    public:
      auto Name() const -> StrView override { return "C"; }
      auto IsForeign() const -> bool override { return true; }

      auto LayOutStruct(
        llvm::StructType &type, Vec<llvm::Type*> const &fields,
        LlvmTypeSymbolInfo &info, LlvmCtx const &) const -> void override {
        LayOutInOrder(type, fields, info, false);
      }

      auto ParamsOf(
        Vec<llvm::Type*> const &value_types, Vec<bool> const &is_signed,
        llvm::Module const &module) const -> Vec<AbiParam> override {
        const auto &triple = TargetTriple();
        const auto arch = triple.getArch();
        const auto by_val = arch == llvm::Triple::ppc or arch == llvm::Triple::ppcle or arch == llvm::Triple::m68k;
        // s390x and Windows x64 share a rule: a struct of 1, 2, 4 or 8
        // bytes travels as an integer of that size, any other as a
        // pointer to the caller's copy. (Windows arm64 passes up to 16
        // bytes in registers, as other AArch64 targets do: direct.)
        const auto s390x = arch == llvm::Triple::systemz;
        const auto win64 = arch == llvm::Triple::x86_64 and triple.isOSWindows();
        const auto int_or_ptr = s390x or win64;
        const auto &layout = module.getDataLayout();

        auto params = Vec<AbiParam>();
        params.reserve(value_types.size());
        for (auto i = 0uz; i < value_types.size(); ++i) {
          const auto type = value_types[i];
          const auto sign = i < is_signed.size() and is_signed[i];
          auto param = AbiParam{.Value = type, .Declared = type, .Extend = CExtendOf(type, sign)};
          if (const auto scalar = CScalarOf(type); scalar != nullptr) {
            param.How = AbiParam::Kind::Unwrap;
            param.Declared = scalar;
            param.Extend = CExtendOf(scalar, sign);
          }
          else if (IsCStruct(type) and (by_val or int_or_ptr)) {
            const auto size = layout.getTypeAllocSize(type).getFixedValue();
            if (int_or_ptr and (size == 1 or size == 2 or size == 4 or size == 8)) {
              param.How = AbiParam::Kind::Coerce;
              param.Declared = llvm::IntegerType::get(type->getContext(), static_cast<unsigned>(size * 8));
            }
            else {
              param.How = by_val ? AbiParam::Kind::ByVal : AbiParam::Kind::Indirect;
              param.Declared = llvm::PointerType::get(type->getContext(), 0);
            }
          }
          params.emplace_back(param);
        }
        return params;
      }

      auto ReturnOf(llvm::Type *type) const -> llvm::Type* override {
        const auto scalar = CScalarOf(type);
        return scalar != nullptr ? scalar : type;
      }

      auto AddAttrs(
        llvm::Function &fn, Vec<AbiParam> const &params, const bool ret_signed) const -> void override {
        const auto &layout = fn.getParent()->getDataLayout();
        for (auto i = 0u; i < params.size(); ++i) {
          if (params[i].Extend != llvm::Attribute::None) { fn.addParamAttr(i, params[i].Extend); }
          if (params[i].How != AbiParam::Kind::ByVal) { continue; }
          fn.addParamAttr(i, llvm::Attribute::getWithByValType(fn.getContext(), params[i].Value));
          fn.addParamAttr(i, llvm::Attribute::getWithAlignment(fn.getContext(), layout.getABITypeAlign(params[i].Value)));
        }
        if (const auto ext = CExtendOf(fn.getReturnType(), ret_signed); ext != llvm::Attribute::None) {
          fn.addRetAttr(ext);
        }
      }

      auto AdaptArgs(
        llvm::Function const &callee, Vec<llvm::Value*> &args, LlvmCtx *ctx) const -> void override {
        const auto fn_type = callee.getFunctionType();
        for (auto i = 0u; i < args.size() and i < fn_type->getNumParams(); ++i) {
          const auto declared = fn_type->getParamType(i);
          const auto value = args[i];
          if (value->getType() == declared) { continue; }

          // A one-field struct passes as its scalar.
          if (CScalarOf(value->getType()) == declared) {
            args[i] = ctx->Builder.CreateExtractValue(value, CScalarPath(value->getType()), "cabi.arg.scalar");
            continue;
          }
          if (not IsCStruct(value->getType())) { continue; }

          // The copy is the caller's, made for this call: "byval" lets
          // the callee write it, and s390x's and Windows x64's pointer is
          // to a copy the callee owns.
          const auto slot = LlvmEntryAlloca(value->getType(), "cabi.arg", ctx);
          ctx->Builder.CreateStore(value, slot);
          args[i] = declared->isPointerTy() ? static_cast<llvm::Value*>(slot)
            : ctx->Builder.CreateLoad(declared, slot, "cabi.arg.int");
        }
      }

      auto AdaptReturn(
        llvm::Value *result, llvm::Type *expected, LlvmCtx *ctx) const -> llvm::Value* override {
        // Its scalar, wrapped back up into the one-field struct the
        // S++ type is.
        if (expected == nullptr or result->getType() == expected) { return result; }
        if (CScalarOf(expected) != result->getType()) { return result; }
        return ctx->Builder.CreateInsertValue(llvm::PoisonValue::get(expected), result, CScalarPath(expected), "cabi.ret");
      }
    };

    const auto kSppAbi = SppAbiImpl();
    const auto kCAbi = CAbiImpl();

    /// Every convention "!conv" can name.
    constexpr auto kAbis = std::to_array<Abi const*>({&kSppAbi, &kCAbi});
  }
}

auto spp::codegen::FindAbi(
  const StrView name)
  -> Abi const* {
  const auto it = genex::find_if(kAbis, [&](auto const *abi) { return abi->Name() == name; });
  return it != kAbis.end() ? *it : nullptr;
}

auto spp::codegen::AbiNames(
  const Opt<bool> foreign)
  -> Vec<Str> {
  return kAbis
    | genex::views::filter([&](auto const *abi) { return not foreign.has_value() or abi->IsForeign() == *foreign; })
    | genex::views::transform([](auto const *abi) { return Str(abi->Name()); })
    | genex::to<Vec>();
}

auto spp::codegen::GetSppAbi()
  -> Abi const& {
  return kSppAbi;
}

auto spp::codegen::GetCAbi()
  -> Abi const& {
  return kCAbi;
}

auto spp::codegen::AbiOf(
  analyse::scopes::TypeSymbol const &cls)
  -> Abi const& {
  return cls.DeclaredAbi != nullptr ? *cls.DeclaredAbi : GetSppAbi();
}

auto spp::codegen::AbiOf(
  asts::FunctionPrototypeAst const &fn)
  -> Abi const& {
  return fn.DeclaredAbi != nullptr ? *fn.DeclaredAbi : GetSppAbi();
}

auto spp::codegen::IsCScalar(
  llvm::Type const *type)
  -> bool {
  if (type->isIntegerTy() or type->isFloatingPointTy() or type->isPointerTy()) { return true; }
  if (const auto arr = llvm::dyn_cast<llvm::ArrayType>(type)) { return IsCScalar(arr->getElementType()); }
  const auto st = llvm::dyn_cast<llvm::StructType>(type);
  return st != nullptr and st->getNumElements() == 1 and IsCScalar(st->getElementType(0));
}
