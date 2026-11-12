module;
#include <spp/macros.hpp>

module spp.codegen.llvm_artefact;
import spp.codegen.llvm_target;
import spp.utils.types;
import llvm;
import std;
import sys;

namespace spp::codegen {
  /// Where a program's hardening record lives: an ELF note, which "strip"
  /// keeps, or a section of its own elsewhere.
  static constexpr auto kHardeningNoteSection = llvm::StringLiteral(
    ".note.spp.hardening");

  /// Where a program's hardening record lives for MachO binaries.
  static constexpr auto kHardeningMachOSection = llvm::StringLiteral("__spp_hardening");

  /// Where a program's hardening record lives for COFF binaries.
  static constexpr auto kHardeningCoffSection = llvm::StringLiteral(".spphard");

  /// The owner the hardening record's ELF note names, and the
  /// note's type under that owner.
  static constexpr auto kHardeningNoteOwner = llvm::StringLiteral("SPP");

  /// The hardening note type (version) embedded with the owner, allowing for
  /// different versions to expand in the future.
  static constexpr auto kHardeningNoteType = 1U;

  /// What the libc this process runs against can fortify: "X"
  /// for every "__X_chk" it exports, which is the set of calls
  /// _FORTIFY_SOURCE can replace, for whichever version of the
  /// library it is. A program is only linked for the host, so
  /// this is the libc it links against too. Empty without one
  /// (not glibc, or not Linux), and then nothing is checked.
  static auto FortifiableByLibc() -> llvm::StringSet<> const&;

  /// What an ELF file lacks of full RELRO: a "PT_GNU_RELRO"
  /// segment, and binding every symbol at load ("BIND_NOW", in
  /// any of the three places a linker may say so). Empty when it
  /// has both; an error when its headers cannot be read.
  template <class ELFT>
  static auto RelroGaps(llvm::object::ELFFile<ELFT> const &elf) -> Expected<Str, Str>;

  /// The plain libc call an ELF file imports where a checked one
  /// was available, as "UnfortifiedImport" describes; empty when
  /// it imports any checked ("__*_chk") call, or no plain one
  /// libc could have checked. A library built with fortification
  /// imports a checked call wherever it could tell a buffer's
  /// size, so one importing any is taken as fortified. A symbol
  /// whose name cannot be read is skipped: the answer turns on
  /// whether any checked import exists, which one entry does not
  /// decide. Its tables not reading is an error.
  template <class ELFT>
  static auto PlainFortifiableImport(llvm::object::ELFFile<ELFT> const &elf) -> Expected<Str, Str>;

  /// The first fault in an ELF file's loadable segments, as
  /// "UnhardenedSegments" describes; empty when there is none.
  template <class ELFT>
  static auto SegmentFaults(llvm::object::ELFFile<ELFT> const &elf) -> Expected<Str, Str>;

  /// "f" (a check answering a finding, or an error) applied to "data" read
  /// as an ELF file of class "ELFT"; an error when it cannot be read as one.
  template <class ELFT, class F>
  static auto VisitElfAs(const llvm::StringRef data, F &&f) -> Expected<Str, Str>;

  /// "f" applied to "object" as whichever of the four ELF classes and byte
  /// orders it is; none when it is not ELF, and an error when it is but
  /// cannot be read.
  template <class F>
  static auto VisitElf(
    llvm::object::ObjectFile const *object, F &&f) -> Opt<Expected<Str, Str>>;

  /// The ELF check "f" run over the file at "path", as the "Unhardened..."
  /// functions answer: none when it finds nothing or the file is not ELF, the
  /// finding otherwise, and an error when the file cannot be read (a
  /// hardening check fails closed).
  template <class F> // Todo: Constrain F?
  static auto ElfFinding(char const *path, F &&f) -> Expected<Opt<Str>, Str>;

  /// "n" rounded up to the four bytes every part of an ELF note is padded to.
  static constexpr auto NotePadded(const std::size_t n) -> std::size_t;

  /// Append "v" to "bytes" as four bytes, in the target's byte
  /// order ("little" endian or big): an ELF note's header words.
  static auto Put32(Str &bytes, const std::uint32_t v, const bool little) -> void;

  /// The inverse of "Put32": the four bytes of "data" at "at", in
  /// the given byte order, read back as one word.
  static auto Get32(const llvm::StringRef data, const std::size_t at, const bool little) -> std::uint32_t;

  /// What an ELF file lacks of BTI: its GNU property note's AArch64 feature
  /// word without the BTI bit, or no such note. The loader guards an object's
  /// code only when that bit is set, and the linker sets it only when every
  /// object linked in has it.
  template <class ELFT>
  static auto BranchTargetGaps(llvm::object::ELFFile<ELFT> const &elf) -> Expected<Str, Str>;
}

auto spp::codegen::FortifiableByLibc() -> llvm::StringSet<> const& {
  static const auto names = [] {
    auto out = llvm::StringSet<>();
#if SPP_PLATFORM_UNIX
    // The directory it was loaded from, and the name it was
    // opened by; not its link map, as "<link.h>" brings in
    // "<elf.h>", whose macros take the names of llvm::ELF's.
    constexpr auto soname = "libc.so.6";
    const auto handle = sys::dlopen(soname, sys::RTLD_LAZY | sys::RTLD_NOLOAD);
    if (handle == nullptr) { return out; }
    char origin[4096] = {};
    const auto found = sys::dlinfo(handle, sys::RTLD_DI_ORIGIN, origin) == 0;
    sys::dlclose(handle);
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

template <class ELFT>
auto spp::codegen::RelroGaps(llvm::object::ELFFile<ELFT> const &elf) -> Expected<Str, Str> {
  // Assume RELRO to be false.
  auto relro = false;

  // If any header in the ELF file is RELRO, then update the flag
  // (ie once true it doesn't become false). Todo: break on true?
  if (auto headers = elf.program_headers()) {
    for (auto const &ph : *headers) {
      relro = relro or ph.p_type == llvm::ELF::PT_GNU_RELRO;
    }
  }

  // Otherwise the file cannot be vouched for: an error, not a pass.
  else {
    return Unexpected(llvm::toString(headers.takeError()));
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

  // Likewise for dynamic entries that cannot be read.
  else {
    return Unexpected(llvm::toString(entries.takeError()));
  }

  // Based on the RELRO and NOW flags, return the correct value
  // that provides "gaps" in the RELRO checks.
  if (not relro and not now) { return "PT_GNU_RELRO and BIND_NOW"; }
  if (not relro) { return "PT_GNU_RELRO"; }
  if (not now) { return "BIND_NOW"; }
  return Str();
}

template <class ELFT>
auto spp::codegen::PlainFortifiableImport(llvm::object::ELFFile<ELFT> const &elf) -> Expected<Str, Str> {
  auto sections = elf.sections();
  if (not sections) { return Unexpected(llvm::toString(sections.takeError())); }

  auto const &fortifiable = FortifiableByLibc();
  auto plain = Str();
  for (auto const &sec : *sections) {
    if (sec.sh_type != llvm::ELF::SHT_DYNSYM) { continue; }
    auto symbols = elf.symbols(&sec);
    if (not symbols) { return Unexpected(llvm::toString(symbols.takeError())); }
    auto strings = elf.getStringTableForSymtab(sec);
    if (not strings) { return Unexpected(llvm::toString(strings.takeError())); }

    // Imports are the undefined symbols.
    for (auto const &sym : *symbols) {
      if (sym.st_shndx != llvm::ELF::SHN_UNDEF) { continue; }
      auto name = sym.getName(*strings);
      if (not name) {
        llvm::consumeError(name.takeError());
        continue;
      }
      if (name->starts_with("__") and name->ends_with("_chk")) { return Str(); }
      if (plain.empty() and fortifiable.contains(*name)) { plain = name->str(); }
    }
  }
  return plain;
}

template <class ELFT>
auto spp::codegen::SegmentFaults(llvm::object::ELFFile<ELFT> const &elf) -> Expected<Str, Str> {
  auto headers = elf.program_headers();
  if (not headers) { return Unexpected(llvm::toString(headers.takeError())); }

  auto loads = Vec<typename ELFT::Phdr>();
  for (auto const &ph : *headers) {
    if (ph.p_type == llvm::ELF::PT_LOAD and ph.p_filesz > 0) { loads.emplace_back(ph); }
  }

  for (auto const &ph : loads) {
    if ((ph.p_flags & llvm::ELF::PF_W) != 0 and (ph.p_flags & llvm::ELF::PF_X) != 0) {
      return "a segment both writable and executable";
    }
  }

  // Pages are counted at the larger of the two alignments, which
  // is at least the page size either is mapped with.
  for (auto const &code : loads) {
    if ((code.p_flags & llvm::ELF::PF_X) == 0) { continue; }
    if (code.p_offset == 0) { return "code sharing a page with the ELF header"; }
    for (auto const &other : loads) {
      if (&other == &code) { continue; }
      const auto page = std::max<std::uint64_t>({code.p_align, other.p_align, 1});
      const auto code_first = code.p_offset / page;
      const auto code_last = (code.p_offset + code.p_filesz - 1) / page;
      const auto other_first = other.p_offset / page;
      const auto other_last = (other.p_offset + other.p_filesz - 1) / page;
      if (code_first <= other_last and other_first <= code_last) { return "code sharing a page with data"; }
    }
  }
  return Str();
}

/// [CHECKED]
template <class ELFT, class F>
auto spp::codegen::VisitElfAs(const llvm::StringRef data, F &&f) -> Expected<Str, Str> {
  auto elf = llvm::object::ELFFile<ELFT>::create(data);
  if (not elf) { return Unexpected(llvm::toString(elf.takeError())); }
  return f(*elf);
}

/// [CHECKED]
template <class F>
auto spp::codegen::VisitElf(
  llvm::object::ObjectFile const *object, F &&f) -> Opt<Expected<Str, Str>> {
  if (not object->isELF()) { return Nullopt; }
  const auto data = object->getData();
  const auto wide = object->getBytesInAddress() == 8;

  // Hook into whichever ELF variation, based on 32- vs 64-bit and LE vs BE
  // endianness.
  return object->isLittleEndian()
    ? (wide ? VisitElfAs<llvm::object::ELF64LE>(data, f) : VisitElfAs<llvm::object::ELF32LE>(data, f))
    : (wide ? VisitElfAs<llvm::object::ELF64BE>(data, f) : VisitElfAs<llvm::object::ELF32BE>(data, f));
}

/// [CHECKED]
template <class F> // Todo: Constrain F?
auto spp::codegen::ElfFinding(char const *path, F &&f) -> Expected<Opt<Str>, Str> {
  auto object = llvm::object::ObjectFile::createObjectFile(path);
  if (not object) { return Unexpected(llvm::toString(object.takeError())); }

  const auto finding = VisitElf(object->getBinary(), f);
  if (not finding) { return Nullopt; }
  if (not *finding) { return Unexpected(finding->error()); }
  if ((*finding)->empty()) { return Nullopt; }
  return **finding;
}

/// [CHECKED]
constexpr auto spp::codegen::NotePadded(const std::size_t n) -> std::size_t {
  return (n + 3) & ~3uz;
}

/// [CHECKED]
auto spp::codegen::Put32(Str &bytes, const std::uint32_t v, const bool little) -> void {
  for (auto i = 0U; i < 4U; ++i) {
    bytes.push_back(static_cast<char>(v >> (8U * (little ? i : 3U - i))));
  }
}

/// [CHECKED]
auto spp::codegen::Get32(const llvm::StringRef data, const std::size_t at, const bool little) -> std::uint32_t {
  auto v = std::uint32_t{0};
  for (auto i = 0U; i < 4U; ++i) {
    const auto byte = static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + i]));
    v |= byte << (8U * (little ? i : 3U - i));
  }
  return v;
}

template <class ELFT>
auto spp::codegen::BranchTargetGaps(llvm::object::ELFFile<ELFT> const &elf) -> Expected<Str, Str> {
  auto sections = elf.sections();
  if (not sections) { return Unexpected(llvm::toString(sections.takeError())); }
  const auto little = ELFT::Endianness == llvm::endianness::little;
  const auto pad = ELFT::Is64Bits ? 8uz : 4uz;

  // Each property is a type, a size, and its data padded to the word
  // size; the one wanted holds the AArch64 features every object shares.
  for (auto const &sec : *sections) {
    if (sec.sh_type != llvm::ELF::SHT_NOTE) { continue; }
    auto err = llvm::Error::success();
    auto bti = false;
    for (auto const &note : elf.notes(sec, err)) {
      if (note.getType() != llvm::ELF::NT_GNU_PROPERTY_TYPE_0 or note.getName() != "GNU") { continue; }
      const auto desc = note.getDesc(sec.sh_addralign);
      const auto data = llvm::StringRef(reinterpret_cast<char const*>(desc.data()), desc.size());
      for (auto at = 0uz; at + 8 <= data.size();) {
        const auto type = Get32(data, at, little);
        const auto size = Get32(data, at + 4, little);
        if (type == llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_AND and size >= 4 and at + 12 <= data.size()) {
          bti = (Get32(data, at + 8, little) & llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_BTI) != 0;
        }
        at += 8 + ((size + pad - 1) & ~(pad - 1));
      }
    }
    if (err) { return Unexpected(llvm::toString(std::move(err))); }
    if (bti) { return Str(); }
  }
  return Str("no BTI property");
}

/// [CHECKED]
auto spp::codegen::EmitHardeningRecord(
  llvm::Module &llvm_mod, char const *record) -> void {
  // Cast the module to the correct type, and extract the context and target
  // triple for this build. The record is received (pre-built) into this
  // function.
  auto &ctx = llvm_mod.getContext();
  const auto &triple = TargetTriple();
  const auto text = llvm::StringRef(record);

  // The bytes is the final padded and generated hardening record, version
  // controlled and metadata complete.
  auto bytes = Str();
  auto section = llvm::StringRef();

  // On ELF binaries, embed the record as a note: its owner's name and the
  // record each padded to four bytes, behind a header of their sizes and the
  // note's type. All done in the target's byte order.
  if (triple.isOSBinFormatELF()) {
    const auto little = llvm_mod.getDataLayout().isLittleEndian();
    constexpr auto owner_size = kHardeningNoteOwner.size() + 1;
    Put32(bytes, static_cast<std::uint32_t>(owner_size), little);  // 4 bytes sizeof owner
    Put32(bytes, static_cast<std::uint32_t>(text.size()), little); // 4 bytes sizeof record
    Put32(bytes, kHardeningNoteType, little);                      // 4 bytes version
    bytes += kHardeningNoteOwner;                                  // n bytes owner
    bytes.resize(12 + NotePadded(owner_size), '\0');
    bytes += text;                                                 // n bytes record
    bytes.resize(12 + NotePadded(owner_size) + NotePadded(text.size()), '\0');
    section = kHardeningNoteSection;
  }

  // Otherwise (MachO and Coff builds), use the raw records text and the correct
  // section (platform-dependent).
  else {
    bytes = text.str();
    section = triple.isOSBinFormatMachO()
      ? llvm::StringRef("__TEXT,__spp_hardening") // kHardeningMachOSection
      : llvm::StringRef(kHardeningCoffSection);
  }

  // Nothing refers to it, so it is kept by being "used". Mock up a global
  // variable with private linkage to hold the "init" for the "bytes", and then
  // set its section into the specialised area. Force 4-byte alignment.
  const auto init = llvm::ConstantDataArray::getString(ctx, bytes, false);
  const auto global = new llvm::GlobalVariable(
    llvm_mod, init->getType(), true,
    llvm::GlobalValue::PrivateLinkage, init,
    "spp.hardening");
  global->setSection(section);
  global->setAlignment(llvm::Align(4));
  llvm::appendToUsed(llvm_mod, {global});
}

/// [CHECKED]
auto spp::codegen::ReadHardeningRecord(
  char const *exe_path) -> Expected<Opt<Str>, Str> {
  // Convert the executable path into an object file, and return an error if
  // that fails (ie this is not an executable file).
  auto object = llvm::object::ObjectFile::createObjectFile(exe_path);
  if (not object) { return Unexpected(llvm::toString(object.takeError())); }
  auto record = Str();

  // Convert the object file into its binary, allowing access to the sections of
  // it, allowing filtering by the specialised section name.
  auto const &file = *object->getBinary();
  for (auto const &sec : file.sections()) {
    auto name = sec.getName();

    // Unnameable section. Ignore it and continue onto the next section.
    if (not name) {
      llvm::consumeError(name.takeError());
      continue;
    }

    // Non-specialised section (check against all three as the tool can check
    // spp binary from any platform).
    if (*name != kHardeningNoteSection
      and *name != kHardeningMachOSection
      and *name != kHardeningCoffSection) {
      continue;
    }

    // At this point, the correct special section has been identified, and is
    // ready to be read from. Check for present and non-corrupt contents first.
    auto contents = sec.getContents();
    if (not contents) {
      return Unexpected(llvm::toString(contents.takeError()));
    }

    // An ELF note's record follows its header and owner's name, read back with
    // "Get32".
    auto data = *contents;
    if (file.isELF()) {
      // If the data is too small, stop processing (prevent index errors). This
      // is a present but no-key record. Otherwise, get the data.
      if (data.size() < 12) { break; }
      const auto little = file.isLittleEndian();
      const auto desc_at = 12 + NotePadded(Get32(data, 0, little));
      const auto desc_size = Get32(data, 4, little);
      if (desc_at + desc_size > data.size()) { break; }
      data = data.substr(desc_at, desc_size);
    }
    record = data.str();
    break;
  }

  // Otherwise, the raw data was being used, so read it unmodified (MachO and
  // Coff)
  if (record.empty()) { return Nullopt; }
  return record;
}

/// [CHECKED]
auto spp::codegen::ImportsAtomicLibcalls(char const *path) -> bool {
  // Create an object file for the path. If this fails, then no processing can
  // be done, so no atomics are found; return false.
  auto object = llvm::object::ObjectFile::createObjectFile(path);
  if (not object) {
    llvm::consumeError(object.takeError());
    return false;
  }

  // Iterate through the object file's symbols. If the symbol is missing a flag
  // or name, ignore and skip.
  for (auto const &sym : object->getBinary()->symbols()) {
    auto flags = sym.getFlags();
    auto name = sym.getName();
    if (not flags or not name) {
      if (not flags) { llvm::consumeError(flags.takeError()); }
      if (not name) { llvm::consumeError(name.takeError()); }
      continue;
    }

    // If the symbol is both undefined, and the name starts with "__atomic_",
    // then we have found an "imported" (different module / not found) "atomic"
    // lib call.
    const auto is_undefined = *flags & llvm::object::SymbolRef::SF_Undefined;
    if (is_undefined and name->starts_with("__atomic_")) { return true; }
  }
  return false;
}

/// [CHECKED]
auto spp::codegen::UnfortifiedImport(
  char const *path) -> Expected<Opt<Str>, Str> {
  return ElfFinding(path, [](auto const &e) { return PlainFortifiableImport(e); });
}

/// [CHECKED]
auto spp::codegen::UnhardenedLink(
  char const *path) -> Expected<Opt<Str>, Str> {
  return ElfFinding(path, [](auto const &e) { return RelroGaps(e); });
}

/// [CHECKED]
auto spp::codegen::UnguardedBranchTargets(
  char const *path) -> Expected<Opt<Str>, Str> {
  return ElfFinding(path, [](auto const &e) { return BranchTargetGaps(e); });
}

auto spp::codegen::UnhardenedSegments(
  char const *path) -> Expected<Opt<Str>, Str> {
  return ElfFinding(path, [](auto const &e) { return SegmentFaults(e); });
}
