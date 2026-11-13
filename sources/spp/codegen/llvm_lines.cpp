module;
#include <spp/macros.hpp>

module spp.codegen.llvm_lines;
import spp.codegen.llvm_target;
import spp.utils.files;
import spp.utils.ice;
import spp.utils.types;
import genex;
import llvm;
import std;

namespace spp::codegen {
  static constexpr auto kNone = std::uint32_t{0xffffffff};

  namespace {
    /// A place in the source: a function's name, a file, a line and a column,
    /// and the frame it was inlined into.
    struct Frame {
      std::uint32_t Name;
      std::uint32_t File;
      std::uint32_t Line;
      std::uint32_t Column;
      std::uint32_t InlinedInto;
    };

    /// Where in a function a range of its code starts, and its innermost frame.
    struct Range {
      std::uint32_t Offset;
      std::uint32_t Frame;
    };

    /// A function: its name's hash, and its ranges.
    struct Function {
      std::uint64_t Hash;
      Vec<Range> Ranges;
    };

    /// The strings and frames of a table, each stored once.
    struct Places {
      Path Root;
      Str Strings;
      Map<Str, std::uint32_t> StringAt;
      Vec<Frame> Frames;
      Map<Str, std::uint32_t> FrameAt;

      auto StringOf(Str const &s) -> std::uint32_t {
        if (const auto it = StringAt.find(s); it != StringAt.end()) { return it->second; }
        const auto at = static_cast<std::uint32_t>(Strings.size());
        Strings += s;
        Strings += '\0';
        StringAt.emplace(s, at);
        return at;
      }

      /// A file as a report shows it, relative to the project. The debug info
      /// records it so already ("./src/main.spp" as LLVM joins it to the
      /// compile directory); a whole path under the project is made so too.
      auto FileOf(Str const &file) -> std::uint32_t {
        auto path = Path(file).lexically_normal();
        if (path.is_absolute()) {
          const auto relative = path.lexically_relative(Root);
          if (not relative.empty() and *relative.begin() != "..") { path = relative; }
        }
        return StringOf(utils::files::DisplayString(path));
      }

      auto FrameOf(Frame const &f) -> std::uint32_t {
        const auto key = std::format("{} {} {} {} {}", f.Name, f.File, f.Line, f.Column, f.InlinedInto);
        if (const auto it = FrameAt.find(key); it != FrameAt.end()) { return it->second; }
        const auto at = static_cast<std::uint32_t>(Frames.size());
        Frames.push_back(f);
        FrameAt.emplace(key, at);
        return at;
      }

      /// The innermost frame of an address's inlining chain, built outermost
      /// first. A frame with no line (code the compiler made) is left out,
      /// leaving its caller's; none at all for an address with no line.
      auto ChainOf(llvm::DIInliningInfo const &chain) -> std::uint32_t {
        auto inner = kNone;
        for (auto i = chain.getNumberOfFrames(); i-- > 0;) {
          auto const &at = chain.getFrame(i);
          if (at.Line == 0 or at.FileName == llvm::DILineInfo::BadString) { continue; }
          const auto name = at.FunctionName == llvm::DILineInfo::BadString ? Str() : Str(at.FunctionName);
          inner = FrameOf({StringOf(name), FileOf(at.FileName), at.Line, at.Column, inner});
        }
        return inner;
      }
    };

    /// Where each code section of an object is placed to read its debug info:
    /// one after another, as a linker would place them. In an object every
    /// section starts at 0, and LLVM finds a compile unit by address alone, so
    /// a function in ".text.unlikely." would otherwise be looked up in the
    /// unit with code at that address in ".text".
    struct SectionLayout final : llvm::LoadedObjectInfo {
      Map<std::uint64_t, std::uint64_t> Bases;

      auto getSectionLoadAddress(llvm::object::SectionRef const &section) const -> std::uint64_t override {
        const auto it = Bases.find(section.getIndex());
        return it != Bases.end() ? it->second : 0;
      }

      auto clone() const -> Unique<llvm::LoadedObjectInfo> override {
        return MakeUnique<SectionLayout>(*this);
      }
    };

    /// The bytes of a table, each field in the target's byte order.
    struct Writer {
      bool Little;
      Vec<std::uint8_t> Bytes;

      auto Put(const std::uint64_t value, const std::size_t width) -> void {
        for (auto i = 0uz; i < width; ++i) {
          const auto shift = 8 * (Little ? i : width - 1 - i);
          Bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }
      }
      auto Put32(const std::uint32_t value) -> void { Put(value, 4); }
      auto Put64(const std::uint64_t value) -> void { Put(value, 8); }
    };
  }

  /// The format's constants, as sppc reads them (backtrace/common.h).
  static constexpr auto kMagic = std::uint32_t{0x4c505053}; // "SPPL", read little-endian

  static constexpr auto kVersion = std::uint32_t{1};

  /// The table's symbol, and the sppc call that "main" hands it to.
  static constexpr auto kTableSymbol = "__spp_lines";

  static constexpr auto kRegisterFn = "sppc_set_source_lines";

  /// The 64-bit FNV-1a hash of a symbol name, as sppc computes it.
  static auto HashOf(const llvm::StringRef name) -> std::uint64_t;
}

auto spp::codegen::HashOf(const llvm::StringRef name) -> std::uint64_t {
  auto h = std::uint64_t{0xcbf29ce484222325};
  for (const auto c : name) {
    h ^= static_cast<std::uint8_t>(c);
    h *= 0x100000001b3;
  }
  return h;
}

auto spp::codegen::BuildLineTable(
  Path const &object_path, Path const &root, const char global_prefix) -> Opt<Vec<std::uint8_t>> {
  auto owned = llvm::object::ObjectFile::createObjectFile(utils::files::NativeString(object_path));
  if (not owned) {
    llvm::consumeError(owned.takeError());
    return Nullopt;
  }
  auto const &object = *owned->getBinary();
  auto layout = SectionLayout();
  auto next = std::uint64_t{0x10000};
  for (auto const &section : object.sections()) {
    if (not section.isText()) { continue; }
    layout.Bases[section.getIndex()] = next;
    next = (next + section.getSize() + 31) & ~std::uint64_t{15};
  }
  const auto dwarf = llvm::DWARFContext::create(
    object, llvm::DWARFContext::ProcessDebugRelocations::Process, &layout);

  // Each function's code, by section and at its section's place: from its
  // symbol to the next one, or to its section's end. Mach-O's symbols have no
  // sizes, so none is used.
  struct Symbol {
    std::uint64_t Start;
    Str Name;
  };
  auto by_section = Map<std::uint64_t, Vec<Symbol>>();
  auto section_ends = Map<std::uint64_t, std::uint64_t>();
  for (auto const &symbol : object.symbols()) {
    auto type = symbol.getType();
    auto flags = symbol.getFlags();
    auto section = symbol.getSection();
    auto address = symbol.getAddress();
    auto name = symbol.getName();
    if (not type or not flags or not section or not address or not name) {
      const auto drop = [](auto &expected) { if (not expected) { llvm::consumeError(expected.takeError()); } };
      drop(type);
      drop(flags);
      drop(section);
      drop(address);
      drop(name);
      continue;
    }
    if (*type != llvm::object::SymbolRef::ST_Function) { continue; }
    if ((*flags & llvm::object::SymbolRef::SF_Undefined) != 0 or *section == object.section_end()) { continue; }
    const auto index = (*section)->getIndex();
    if (not layout.Bases.contains(index)) { continue; }
    const auto base = layout.Bases[index];
    section_ends[index] = base + (*section)->getSize();
    auto plain = *name;
    if (global_prefix != '\0' and plain.starts_with(llvm::StringRef(&global_prefix, 1))) { plain = plain.drop_front(); }
    by_section[index].push_back({base + *address - (*section)->getAddress(), plain.str()});
  }

  // Where each inlined call's code starts and ends. A line-table row need not
  // change there: a row with no line (code the compiler made) can run on into
  // an inlined call, whose caller's line is then the one to report.
  auto inline_edges = Map<std::uint64_t, Vec<std::uint64_t>>();
  for (auto const &unit : dwarf->compile_units()) {
    for (auto i = 0u; i < unit->getNumDIEs(); ++i) {
      const auto die = unit->getDIEAtIndex(i);
      if (die.getTag() != llvm::dwarf::DW_TAG_inlined_subroutine) { continue; }
      auto ranges = die.getAddressRanges();
      if (not ranges) {
        llvm::consumeError(ranges.takeError());
        continue;
      }
      for (auto const &r : *ranges) {
        inline_edges[r.SectionIndex].push_back(r.LowPC);
        inline_edges[r.SectionIndex].push_back(r.HighPC);
      }
    }
  }

  // Each function's ranges: one at its start, one per line-table row and one
  // per inlined call's edge, each the innermost place of its inlining chain; a
  // run of one place is one range. A function without a line is left out.
  const auto spec = llvm::DILineInfoSpecifier(
    llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
    llvm::DILineInfoSpecifier::FunctionNameKind::ShortName);
  auto places = Places{.Root = root, .Strings = {}, .StringAt = {}, .Frames = {}, .FrameAt = {}};
  auto functions = Vec<Function>();
  for (auto &[index, symbols] : by_section) {
    genex::actions::sort(symbols, {}, &Symbol::Start);
    auto &edges = inline_edges[index];
    edges |= genex::actions::sort;
    auto edge = edges.begin();
    for (auto i = 0uz; i < symbols.size(); ++i) {
      const auto start = symbols[i].Start;
      const auto end = i + 1 < symbols.size() ? symbols[i + 1].Start : section_ends[index];
      if (end <= start) { continue; }

      auto addresses = Vec<std::uint64_t>{start};
      for (auto const &[address, row] : dwarf->getLineInfoForAddressRange({start, index}, end - start, spec)) {
        static_cast<void>(row);
        addresses.push_back(address);
      }
      for (; edge != edges.end() and *edge < end; ++edge) {
        if (*edge >= start) { addresses.push_back(*edge); }
      }
      addresses |= genex::actions::sort;
      addresses |= genex::actions::unique;

      auto function = Function{.Hash = HashOf(symbols[i].Name), .Ranges = {}};
      for (const auto address : addresses) {
        const auto frame = places.ChainOf(dwarf->getInliningInfoForAddress({address, index}, spec));
        const auto same = function.Ranges.empty() ? frame == kNone : function.Ranges.back().Frame == frame;
        if (not same) { function.Ranges.push_back({static_cast<std::uint32_t>(address - start), frame}); }
      }
      if (not function.Ranges.empty()) { functions.push_back(std::move(function)); }
    }
  }

  // Sorted by hash, for sppc's search. Two names of one hash are both left
  // out: neither could be told apart from the other.
  genex::actions::sort(functions, {}, &Function::Hash);
  auto kept = Vec<Function>();
  for (auto i = 0uz; i < functions.size(); ++i) {
    const auto clash = (i > 0 and functions[i - 1].Hash == functions[i].Hash)
      or (i + 1 < functions.size() and functions[i + 1].Hash == functions[i].Hash);
    if (not clash) { kept.push_back(std::move(functions[i])); }
  }

  // The bytes: the header, the functions, every function's ranges, the
  // frames, then the strings.
  auto n_ranges = 0uz;
  for (auto const &f : kept) { n_ranges += f.Ranges.size(); }
  auto out = Writer{.Little = object.isLittleEndian(), .Bytes = {}};
  out.Put32(kMagic);
  out.Put32(kVersion);
  out.Put32(static_cast<std::uint32_t>(kept.size()));
  out.Put32(static_cast<std::uint32_t>(n_ranges));
  out.Put32(static_cast<std::uint32_t>(places.Frames.size()));
  out.Put32(static_cast<std::uint32_t>(places.Strings.size()));
  auto first_range = std::uint32_t{0};
  for (auto const &f : kept) {
    out.Put64(f.Hash);
    out.Put32(first_range);
    out.Put32(static_cast<std::uint32_t>(f.Ranges.size()));
    first_range += static_cast<std::uint32_t>(f.Ranges.size());
  }
  for (auto const &f : kept) {
    for (auto const &r : f.Ranges) {
      out.Put32(r.Offset);
      out.Put32(r.Frame);
    }
  }
  for (auto const &f : places.Frames) {
    for (const auto field : {f.Name, f.File, f.Line, f.Column, f.InlinedInto}) { out.Put32(field); }
  }
  out.Bytes.insert(out.Bytes.end(), places.Strings.begin(), places.Strings.end());
  return std::move(out.Bytes);
}

auto spp::codegen::EmitLineTableObject(
  llvm::Module const &program, Vec<std::uint8_t> const &table, Path const &path,
  const unsigned opt_level) -> bool {
  auto &ctx = program.getContext();
  auto module = llvm::Module("spp.lines", ctx);
  module.setTargetTriple(program.getTargetTriple());
  module.setDataLayout(program.getDataLayout());
  auto flags = llvm::SmallVector<llvm::Module::ModuleFlagEntry>();
  program.getModuleFlagsMetadata(flags);
  for (auto const &flag : flags) { module.addModuleFlag(flag.Behavior, flag.Key->getString(), flag.Val); }

  const auto data = llvm::ConstantDataArray::get(ctx, llvm::ArrayRef<std::uint8_t>(table));
  const auto global = new llvm::GlobalVariable(
    module, data->getType(), true, llvm::GlobalValue::ExternalLinkage, data, kTableSymbol);
  global->setVisibility(llvm::GlobalValue::HiddenVisibility);
  global->setDSOLocal(true);
  global->setAlignment(llvm::Align(8));
  return EmitObjectFile(module, utils::files::NativeString(path).c_str(), opt_level);
}

auto spp::codegen::RegisterLineTable(
  llvm::Module &program) -> void {
  const auto main_fn = program.getFunction("main");
  SPP_ASSERT(main_fn != nullptr and not main_fn->isDeclaration());
  auto &ctx = program.getContext();
  const auto ptr_ty = llvm::PointerType::get(ctx, 0);

  // Declared as an array of no known length; its definition is the other
  // object's. Hidden, as nothing outside the program reads it but through
  // this call.
  const auto table = llvm::cast<llvm::GlobalVariable>(
    program.getOrInsertGlobal(kTableSymbol, llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 0)));
  table->setVisibility(llvm::GlobalValue::HiddenVisibility);
  table->setDSOLocal(true);

  const auto register_fn = program.getOrInsertFunction(
    kRegisterFn, llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {ptr_ty}, false));
  auto &entry = main_fn->getEntryBlock();
  auto builder = llvm::IRBuilder<>(&entry, entry.getFirstInsertionPt());
  builder.CreateCall(register_fn, {table});
}
