module;
#include <spp/macros.hpp>

#if SPP_PLATFORM_UNIX
#include <dlfcn.h>
#include <link.h>
#endif

module spp.codegen.llvm_artefact;
import spp.codegen.llvm_target;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  namespace {
    /// Where a program's hardening record lives: an ELF note,
    /// which "strip" keeps, or a section of its own elsewhere.
    constexpr auto kHardeningNoteSection = llvm::StringLiteral(".note.spp.hardening");
    constexpr auto kHardeningMachOSection = llvm::StringLiteral("__spp_hardening");
    constexpr auto kHardeningCoffSection = llvm::StringLiteral(".spphard");

    /// The owner the hardening record's ELF note names, and the
    /// note's type under that owner.
    constexpr auto kHardeningNoteOwner = llvm::StringLiteral("SPP");
    constexpr auto kHardeningNoteType = 1U;

    /// What the libc this process runs against can fortify: "X"
    /// for every "__X_chk" it exports, which is the set of calls
    /// _FORTIFY_SOURCE can replace, for whichever version of the
    /// library it is. A program is only linked for the host, so
    /// this is the libc it links against too. Empty without one
    /// (not glibc, or not Linux), and then nothing is checked.
    auto FortifiableByLibc() -> llvm::StringSet<> const& {
      static const auto names = [] {
        auto out = llvm::StringSet<>();
#if SPP_PLATFORM_UNIX
        const auto handle = dlopen("libc.so.6", RTLD_LAZY | RTLD_NOLOAD);
        if (handle == nullptr) { return out; }
        auto map = static_cast<link_map*>(nullptr);
        const auto found = dlinfo(handle, RTLD_DI_LINKMAP, &map) == 0 and map != nullptr;
        const auto path = found ? Str(map->l_name) : Str();
        dlclose(handle);
        if (path.empty()) { return out; }

        auto object = llvm::object::ObjectFile::createObjectFile(path);
        if (not object) {
          llvm::consumeError(object.takeError());
          return out;
        }
        const auto elf = llvm::dyn_cast<llvm::object::ELFObjectFileBase>(object->getBinary());
        if (elf == nullptr) { return out; }
        for (auto const &sym : elf->getDynamicSymbolIterators()) {
          auto flags = sym.getFlags();
          auto name = sym.getName();
          if (not flags or not name) {
            if (not flags) { llvm::consumeError(flags.takeError()); }
            if (not name) { llvm::consumeError(name.takeError()); }
            continue;
          }
          if ((*flags & llvm::object::SymbolRef::SF_Undefined) != 0) { continue; }
          if (name->starts_with("__") and name->ends_with("_chk")) { out.insert(name->drop_front(2).drop_back(4)); }
        }
#endif
        return out;
      }();
      return names;
    }

    /// "n" rounded up to the four bytes every part of an ELF
    /// note is padded to.
    constexpr auto NotePadded(const std::size_t n) -> std::size_t {
      return (n + 3) & ~std::size_t{3};
    }
  }
}

auto spp::codegen::EmitHardeningRecord(
  void *llvm_module,
  char const *record)
  -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto &ctx = llvm_mod.getContext();
  const auto &triple = TargetTriple();
  const auto text = llvm::StringRef(record);

  // On ELF, a note: its owner's name and the record each padded
  // to four bytes, behind a header of their sizes and the note's
  // type, in the target's byte order.
  auto bytes = Str();
  auto section = llvm::StringRef();
  if (triple.isOSBinFormatELF()) {
    const auto little = llvm_mod.getDataLayout().isLittleEndian();
    const auto put32 = [&](const std::uint32_t v) {
      for (auto i = 0U; i < 4U; ++i) { bytes.push_back(static_cast<char>(v >> (8U * (little ? i : 3U - i)))); }
    };
    const auto owner_size = kHardeningNoteOwner.size() + 1;
    put32(static_cast<std::uint32_t>(owner_size));
    put32(static_cast<std::uint32_t>(text.size()));
    put32(kHardeningNoteType);
    bytes += kHardeningNoteOwner;
    bytes.resize(12 + NotePadded(owner_size), '\0');
    bytes += text;
    bytes.resize(12 + NotePadded(owner_size) + NotePadded(text.size()), '\0');
    section = kHardeningNoteSection;
  }
  else {
    bytes = text.str();
    section = triple.isOSBinFormatMachO() ? llvm::StringRef("__TEXT,__spp_hardening") : llvm::StringRef(kHardeningCoffSection);
  }

  // Nothing refers to it, so it is kept by being "used".
  const auto init = llvm::ConstantDataArray::getString(ctx, bytes, /*AddNull=*/false);
  const auto global = new llvm::GlobalVariable(
    llvm_mod, init->getType(), true, llvm::GlobalValue::PrivateLinkage, init, "spp.hardening");
  global->setSection(section);
  global->setAlignment(llvm::Align(4));
  llvm::appendToUsed(llvm_mod, {global});
}

auto spp::codegen::ReadHardeningRecord(
  char const *path,
  char const **error)
  -> char const* {
  static auto record = Str();
  static auto failure = Str();
  record.clear();

  auto object = llvm::object::ObjectFile::createObjectFile(path);
  if (not object) {
    failure = llvm::toString(object.takeError());
    *error = failure.c_str();
    return nullptr;
  }

  auto const &file = *object->getBinary();
  for (auto const &sec : file.sections()) {
    auto name = sec.getName();
    if (not name) {
      llvm::consumeError(name.takeError());
      continue;
    }
    if (*name != kHardeningNoteSection and *name != kHardeningMachOSection and *name != kHardeningCoffSection) {
      continue;
    }
    auto contents = sec.getContents();
    if (not contents) {
      failure = llvm::toString(contents.takeError());
      *error = failure.c_str();
      return nullptr;
    }

    // An ELF note's record follows its header and owner's name.
    auto data = *contents;
    if (file.isELF()) {
      const auto get32 = [&](const std::size_t at) {
        auto v = std::uint32_t{0};
        for (auto i = 0U; i < 4U; ++i) {
          const auto byte = static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + i]));
          v |= byte << (8U * (file.isLittleEndian() ? i : 3U - i));
        }
        return v;
      };
      if (data.size() < 12) { break; }
      const auto desc_at = 12 + NotePadded(get32(0));
      const auto desc_size = get32(4);
      if (desc_at + desc_size > data.size()) { break; }
      data = data.substr(desc_at, desc_size);
    }
    record = data.str();
    break;
  }
  return record.c_str();
}

auto spp::codegen::UnfortifiedImport(
  char const *path)
  -> char const* {
  static auto found = Str();
  auto object = llvm::object::ObjectFile::createObjectFile(path);
  if (not object) {
    llvm::consumeError(object.takeError());
    return nullptr;
  }
  const auto elf = llvm::dyn_cast<llvm::object::ELFObjectFileBase>(object->getBinary());
  if (elf == nullptr) { return nullptr; }

  // A library built with fortification imports a checked
  // function wherever it could tell a buffer's size, so one
  // that imports any is taken as fortified; the plain calls left
  // are those it could not check. One importing only plain ones
  // that libc could have checked was built without.
  auto const &fortifiable = FortifiableByLibc();
  auto plain = Str();
  for (auto const &sym : elf->getDynamicSymbolIterators()) {
    auto flags = sym.getFlags();
    auto name = sym.getName();
    if (not flags or not name) {
      if (not flags) { llvm::consumeError(flags.takeError()); }
      if (not name) { llvm::consumeError(name.takeError()); }
      continue;
    }
    if ((*flags & llvm::object::SymbolRef::SF_Undefined) == 0) { continue; }
    if (name->starts_with("__") and name->ends_with("_chk")) { return nullptr; }
    if (plain.empty() and fortifiable.contains(*name)) { plain = name->str(); }
  }
  if (plain.empty()) { return nullptr; }
  found = std::move(plain);
  return found.c_str();
}
