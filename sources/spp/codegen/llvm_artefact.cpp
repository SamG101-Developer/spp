module;
#include <spp/macros.hpp>

#if SPP_PLATFORM_UNIX
#include <dlfcn.h> // Todo: move to a "sys module
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
        // The directory it was loaded from, and the name it was
        // opened by; not its link map, as "<link.h>" brings in
        // "<elf.h>", whose macros take the names of llvm::ELF's.
        constexpr auto soname = "libc.so.6";
        const auto handle = dlopen(soname, RTLD_LAZY | RTLD_NOLOAD);
        if (handle == nullptr) { return out; }
        char origin[4096] = {};
        const auto found = dlinfo(handle, RTLD_DI_ORIGIN, origin) == 0;
        dlclose(handle);
        if (not found or origin[0] == '\0') { return out; }
        const auto path = Str(origin) + "/" + soname;

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

    /// What an ELF file lacks of full RELRO: a "PT_GNU_RELRO"
    /// segment, and binding every symbol at load ("BIND_NOW", in
    /// any of the three places a linker may say so). Empty when it
    /// has both, or its headers cannot be read.
    template <class ELFT>
    auto RelroGaps(llvm::object::ELFObjectFile<ELFT> const &file) -> Str {
      // Get the ELF file from the object file, and assume RELRO to
      // be false.
      auto const &elf = file.getELFFile();
      auto relro = false;

      // If any header in the ELF file is RELRO, then update the flag
      // (ie once true it doesn't become false). Todo: break on true?
      if (auto headers = elf.program_headers()) {
        for (auto const &ph : *headers) {
          relro = relro or ph.p_type == llvm::ELF::PT_GNU_RELRO;
        }
      }

      // Otherwise, consume the error and return an empty string, as
      // there is no handling to be done.
      else {
        llvm::consumeError(headers.takeError());
        return Str();
      }

      // For each entry in the ELF file's entries, get the tag and the
      // value, check the tag against known ELF values, and confirm
      // whether it is an "NOW" variation.
      auto now = false;
      if (auto entries = elf.dynamicEntries()) {
        for (auto const &dyn : *entries) {
          const auto tag = static_cast<std::uint64_t>(dyn.getTag());
          const auto val = static_cast<std::uint64_t>(dyn.getVal());
          now = now
            or tag == llvm::ELF::DT_BIND_NOW
            or (tag == llvm::ELF::DT_FLAGS and (val & llvm::ELF::DF_BIND_NOW) != 0)
            or (tag == llvm::ELF::DT_FLAGS_1 and (val & llvm::ELF::DF_1_NOW) != 0);
        }
      }

      // If there are no entries, consume the error and return an empty
      // string - no handling.
      else {
        llvm::consumeError(entries.takeError());
        return Str();
      }

      // Based on the RELRO and NOW flags, return the correct value
      // that provides "gaps" in the RELRO checks.
      if (not relro and not now) { return "PT_GNU_RELRO and BIND_NOW"; }
      if (not relro) { return "PT_GNU_RELRO"; }
      if (not now) { return "BIND_NOW"; }
      return Str();
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
    constexpr auto owner_size = kHardeningNoteOwner.size() + 1;
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
    section = triple.isOSBinFormatMachO()
      ? llvm::StringRef("__TEXT,__spp_hardening")
      : llvm::StringRef(kHardeningCoffSection);
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
  char const *path) -> Opt<Str> {
  auto object = llvm::object::ObjectFile::createObjectFile(path);
  if (not object) {
    llvm::consumeError(object.takeError());
    return Nullopt;
  }
  const auto elf = llvm::dyn_cast<llvm::object::ELFObjectFileBase>(object->getBinary());
  if (elf == nullptr) { return Nullopt; }

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
    if (name->starts_with("__") and name->ends_with("_chk")) { return Nullopt; }
    if (plain.empty() and fortifiable.contains(*name)) { plain = name->str(); }
  }
  if (plain.empty()) { return Nullopt; }
  return plain;
}

auto spp::codegen::UnhardenedLink(
  char const *path) -> Opt<Str> {
  // If llvm failed to make an object file then consume the error
  // and return nullopt.
  auto object = llvm::object::ObjectFile::createObjectFile(path);
  if (not object) {
    llvm::consumeError(object.takeError());
    return Nullopt;
  }

  // Try the 4 different class and byte orders for the headers. Any
  // different object type isn't ELF.
  auto const *const binary = object->getBinary();
  auto gaps = Str();
  if (const auto e = llvm::dyn_cast<llvm::object::ELFObjectFile<llvm::object::ELF64LE>>(binary)) {
    gaps = RelroGaps(*e);
  }
  else if (const auto e = llvm::dyn_cast<llvm::object::ELFObjectFile<llvm::object::ELF64BE>>(binary)) {
    gaps = RelroGaps(*e);
  }
  else if (const auto e = llvm::dyn_cast<llvm::object::ELFObjectFile<llvm::object::ELF32LE>>(binary)) {
    gaps = RelroGaps(*e);
  }
  else if (const auto e = llvm::dyn_cast<llvm::object::ELFObjectFile<llvm::object::ELF32BE>>(binary)) {
    gaps = RelroGaps(*e);
  }
  else { return Nullopt; }

  // If there are no gaps, there is nothing to report.
  if (gaps.empty()) { return Nullopt; }
  return gaps;
}
