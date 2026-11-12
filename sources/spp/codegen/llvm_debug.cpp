module;
#include <spp/macros.hpp>
#include <spp/version.hpp>

module spp.codegen.llvm_debug;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_target;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.utils.error_formatter;
import spp.utils.features;
import spp.utils.files;
import spp.utils.types;
import genex;
import llvm;
import std;

namespace spp::codegen {
  using analyse::scopes::TypeRef;
  using analyse::scopes::TypeSymbol;

  /// Where "position" is, as a file and a line and column: the position's own
  /// file, or the module's with line 0 for one with no line.
  static auto PlaceOf(
    LlvmCtx &ctx,
    std::size_t position) -> Tup<llvm::DIFile*, unsigned, unsigned>;

  /// The scope code generated now is placed in: the innermost lexical block
  /// open in the builder's function, else that function's subprogram. Null
  /// outside a function, or in one with no subprogram.
  static auto CurrentDebugScope(LlvmCtx const &ctx) -> llvm::DILocalScope*;

  /// A type's size and ABI alignment in bits, as the target lays it out.
  static auto BitsOf(
    LlvmCtx const &ctx, llvm::Type *type) -> Pair<std::uint64_t, std::uint32_t>;

  /// The symbol a type's layout belongs to: the class "Self" names, an
  /// alias's target, or the symbol itself.
  static auto CanonicalOf(
    LlvmCtx const &ctx, TypeSymbol const &sym) -> TypeSymbol* ;

  /// A member of a structure, union or class: "name", of "type", at "offset"
  /// bits into "scope".
  static auto MemberOf(
    LlvmCtx const &ctx, llvm::DIScope *scope, StrView name, llvm::DIType *type,
    llvm::Type *llvm_type, std::uint64_t offset) -> llvm::DIType*;

  /// A structure of "llvm_type"'s size, named "name", put in the cache before
  /// its fields are described, so a type reached again through one of them (a
  /// pointer to itself) ends at it; "fill" then gives the fields.
  template <typename F>
  static auto StructureOf(
    LlvmCtx &ctx, TypeSymbol const *key, StrView name, llvm::Type *llvm_type,
    F &&fill) -> llvm::DIType*;

  /// The top-level arguments of a mangled type name, "Vec[T=S32]" giving
  /// "T=S32": what is between its outer brackets, split at the commas not
  /// inside an argument's own brackets.
  static auto TopLevelArgsOf(StrView mangled) -> Vec<Str>;

  /// A type's name as S++ spells it, qualified by its module: "S32",
  /// "&main::Point", "std::vector::Vec[S32, std::allocator::GlobalAlloc]",
  /// "(U64, Bool)", "std::option::None or std::option::Some[S32]": the name a
  /// debugger shows for it, and the one std's formatters match on.
  static auto NameOf(LlvmCtx &ctx, TypeRef const &ref) -> Str;

  /// "NameOf" for a type with a layout of its own: its template's name, then
  /// its generic arguments, each named the same way; a tuple's elements in
  /// parentheses, and a variant's members joined by "or".
  static auto DisplayNameOf(
    LlvmCtx &ctx, TypeSymbol const &sym,
    analyse::scopes::Scope const &scope) -> Str;
}

/// [CHECKED]
auto spp::codegen::PlaceOf(
  LlvmCtx &ctx, const std::size_t position)
  -> Tup<llvm::DIFile*, unsigned, unsigned> {
  // Use the error formatter's position handler (single source of truth).
  const auto formatter = utils::errors::ErrorFormatter::Owning(position);
  const auto at = formatter != nullptr ? formatter->LineColOf(position) : Nullopt;

  // Bundle the debug file from the formatter with the line and column
  // positions.
  if (not at.has_value()) { return {ctx.DiUnit->getFile(), 0u, 0u}; }
  return {
    DebugFileOf(ctx, formatter->FilePath()),
    static_cast<unsigned>(at->Line), static_cast<unsigned>(at->Column)
  };
}

/// [CHECKED]
auto spp::codegen::CurrentDebugScope(
  LlvmCtx const &ctx) -> llvm::DILocalScope* {
  // Get the current block's parent block (if the block exists), and check for
  // a corresponding debug implementation.
  const auto block = ctx.Builder.GetInsertBlock();
  const auto fn = block != nullptr ? block->getParent() : nullptr;
  if (fn == nullptr or fn->getSubprogram() == nullptr) { return nullptr; }

  // The innermost block of this function; a closure's blocks sit above its
  // parent's, and are skipped while the parent's code is generated.
  for (auto const &[owner, lexical, _] : ctx.DiBlocks | genex::views::reverse) {
    if (owner == fn) { return lexical; }
  }
  return fn->getSubprogram();
}

/// [CHECKED]
auto spp::codegen::BitsOf(
  LlvmCtx const &ctx, llvm::Type *type) -> Pair<std::uint64_t, std::uint32_t> {
  // If a type is invalid (nullptr), or non-sized or scalable, return the
  // fallback (0, 0).
  auto const &dl = ctx.Module->getDataLayout();
  if (type == nullptr or not type->isSized() or type->isScalableTy()) {
    return {0, 0};
  }

  // Otherwise, get the size allocated for the type, and the ABI alignment for
  // it, as a pair.
  return {
    static_cast<std::uint64_t>(dl.getTypeAllocSizeInBits(type).getFixedValue()),
    static_cast<std::uint32_t>(dl.getABITypeAlign(type).value() * 8)
  };
}

/// [CHECKED]
auto spp::codegen::CanonicalOf(
  LlvmCtx const &ctx, TypeSymbol const &sym) -> TypeSymbol* {
  // Look through a symbol to what it is bound against, look through aliases,
  // and return the resolved symbol. Todo: Is this a duplicate?
  auto linked = sym.AsBound();
  if (sym.Alias != nullptr and ctx.Sm != nullptr) {
    const auto target = sym.AliasTarget(*ctx.Sm->CurrentScope);
    if (target != nullptr) { linked = target; }
  }
  // A symbol is only read here; "TypeRef::Of" takes one as codegen keeps them.
  return const_cast<TypeSymbol*>(linked != nullptr ? linked : &sym);
}

/// [CHECKED]
auto spp::codegen::MemberOf(
  LlvmCtx const &ctx, llvm::DIScope *scope, const StrView name,
  llvm::DIType *type, llvm::Type *llvm_type,
  const std::uint64_t offset) -> llvm::DIType* {
  // Get the size/alignment of a type, and then create a member type access into
  // it, for debugging / debugger view.
  const auto [size, align] = BitsOf(ctx, llvm_type);
  return ctx.DiBuilder->createMemberType(
    scope, name, ctx.DiUnit->getFile(), 0, size, align, offset,
    llvm::DINode::FlagZero, type);
}

/// [CHECKED]
template <typename F>
auto spp::codegen::StructureOf(
  LlvmCtx &ctx, TypeSymbol const *key, const StrView name,
  llvm::Type *llvm_type, F &&fill) -> llvm::DIType* {
  // Get the size/alignment of a type, and build a debug struct type for it,
  // provided the type symbol's information.
  const auto [size, align] = BitsOf(ctx, llvm_type);
  auto structure = ctx.DiBuilder->createStructType(
    ctx.DiUnit, name, ctx.DiUnit->getFile(), 0,
    size, align, llvm::DINode::FlagZero, nullptr,
    ctx.DiBuilder->getOrCreateArray({}));

  // Store the struct against the key (type symbol), and run the "fill" function
  // to set the struct body.
  ctx.DiTypes[key] = structure;
  const auto fields = fill(static_cast<llvm::DIScope*>(structure));
  ctx.DiBuilder->replaceArrays(
    structure, ctx.DiBuilder->getOrCreateArray(fields));
  return structure;
}

auto spp::codegen::TopLevelArgsOf(const StrView mangled) -> Vec<Str> {
  auto out = Vec<Str>();
  const auto open = mangled.find('[');
  if (open == StrView::npos or mangled.back() != ']') { return out; }
  auto depth = 0;
  auto start = open + 1;
  for (auto i = start; i + 1 < mangled.size(); ++i) {
    if (mangled[i] == '[') { ++depth; }
    else if (mangled[i] == ']') { --depth; }
    else if (mangled[i] == ',' and depth == 0) {
      out.emplace_back(mangled.substr(start, i - start));
      start = i + 1;
      while (start < mangled.size() and mangled[start] == ' ') { ++start; }
    }
  }
  out.emplace_back(mangled.substr(start, mangled.size() - 1 - start));
  return out;
}

auto spp::codegen::NameOf(LlvmCtx &ctx, TypeRef const &ref) -> Str {
  const auto type = DebugTypeOf(ctx, ref);
  return type != nullptr ? type->getName().str() : Str("Void");
}

auto spp::codegen::DisplayNameOf(
  LlvmCtx &ctx, TypeSymbol const &sym,
  analyse::scopes::Scope const &scope) -> Str {
  using namespace asts::generate::common_types_precompiled;
  const auto cls_ref = TypeRef::Of(const_cast<TypeSymbol&>(sym), scope);
  const auto join = [&](Vec<TypeRef> const &refs, const StrView between) {
    auto out = Str();
    for (auto const &ref : refs) {
      out += (out.empty() ? "" : Str(between)) + NameOf(ctx, ref);
    }
    return out;
  };
  if (cls_ref.IsA(*VAR, scope)) {
    return join(analyse::utils::type_members::VariantMemberRefs(cls_ref, scope),
                " or ");
  }
  if (analyse::utils::type_predicates::IsTypeTuple(
    TypeRef::ForKindCheck(sym, scope), scope)) {
    const auto elems = sym.TypeArgRefs();
    return "(" + join(elems, ", ") + (elems.size() == 1 ? ",)" : ")");
  }

  // The template's name with its module path ("std::vector::Vec"): the
  // debugger formatters tell std's types from a user's by it, as nothing
  // else in the debug info qualifies a name. Then each argument: the
  // mangled name lists them in order, a type or comp
  // argument by its parameter's name ("T=...", "n=3_uz"), a pack's elements
  // unnamed. A type is named as "NameOf" names it, a value as written.
  const auto mangled = mangle::MangleTypeName(sym);
  const auto base = mangled.substr(0, mangled.find('['));
  if (mangled.find('[') == Str::npos) { return base; }

  auto args = Str();
  auto pack = sym.TypeArgRefs();
  auto next_in_pack = 0uz;
  for (auto const &part : TopLevelArgsOf(mangled)) {
    const auto eq = part.find('=');
    auto arg = Str();
    if (eq == Str::npos) {
      arg = next_in_pack < pack.size()
        ? NameOf(ctx, pack[next_in_pack++])
        : part;
    }
    else if (const auto type_arg = sym.TypeArgRef(part.substr(0, eq)); type_arg.
      Symbol != nullptr) {
      arg = NameOf(ctx, type_arg);
      ++next_in_pack; // a pack's elements follow the named type arguments
    }
    else { arg = part.substr(eq + 1); }
    args += (args.empty() ? "" : ", ") + arg;
  }
  return base + "[" + args + "]";
}

/// [CHECKED]
auto spp::codegen::StartDebugInfo(
  LlvmCtx &ctx, const StrView path, Path const &root,
  const utils::features::DebugInfo level) -> void {
  // If we are not building the project with debug info, then no work needs to
  // be done in this function (noop), so return.
  if (level == utils::features::DebugInfo::None) { return; }

  // Create the debug info builder for the module, and set it on the context,
  // and assign the root too. This function is only called once (from the
  // compiler boot), so there is no overwrite danger.
  ctx.DiBuilder = MakeUnique<llvm::DIBuilder>(*ctx.Module);
  ctx.DiRoot = root;
  ctx.DiVariables = level == utils::features::DebugInfo::Full;

  // Line tables alone, or with variables and their types ("full"), under
  // Rust's language code (see the module's own comment). Not
  // marked optimised, as the level is chosen after the module is built. No
  // name index (".debug_names"): a debugger builds its own, and coroutine
  // splitting gives its parts linkage names the index would list against
  // entries that carry none.
  ctx.DiUnit = ctx.DiBuilder->createCompileUnit(
    llvm::dwarf::DW_LANG_Rust, DebugFileOf(ctx, path),
    Str("spp ") + SPP_VERSION, false, "", 0, "",
    ctx.DiVariables ? llvm::DICompileUnit::FullDebug : llvm::DICompileUnit::LineTablesOnly, 0, true, false,
    llvm::DICompileUnit::DebugNameTableKind::None);

  // The version of the debug metadata, and which debug format the object
  // carries: CodeView for an MSVC target, DWARF 5 otherwise. "Warning", as
  // every module of a program agrees on both.
  ctx.Module->addModuleFlag(
    llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
  TargetTriple().isWindowsMSVCEnvironment()
    ? ctx.Module->addModuleFlag(llvm::Module::Warning, "CodeView", 1)
    : ctx.Module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
}

/// [CHECKED]
auto spp::codegen::FinishDebugInfo(LlvmCtx const &ctx) -> void {
  // If a builder was added to the context in the "StartDebugInfo" function,
  // then finalise it finish up, otherwise a noop.
  if (ctx.DiBuilder != nullptr) { ctx.DiBuilder->finalize(); }
}

/// [CHECKED]
auto spp::codegen::DebugFileOf(
  LlvmCtx &ctx, const StrView path) -> llvm::DIFile* {
  // Given the string path, find the corresponding "DIFile" from the lookup map.
  // If it's found, return it.
  const auto it = ctx.DiFiles.find(Str(path));
  if (it != ctx.DiFiles.end()) { return it->second; }

  // The path is relative to the projects root, so search from there; any
  // external files (none at the moment)
  const auto relative = Path(path).lexically_relative(
    ctx.DiRoot);
  const auto inside = not relative.empty() and *relative.begin() != "..";
  const auto file = inside ? relative : Path(path);
  const auto di_file = ctx.DiBuilder->createFile(
    utils::files::DisplayString(file), ".");

  // Add the di-file against the path and emplace it in the context. A future
  // retrieval to this path will use the already-created di-file.
  ctx.DiFiles.emplace(Str(path), di_file);
  return di_file;
}

/// [CHECKED]
auto spp::codegen::AttachSubprogram(
  LlvmCtx &ctx, llvm::Function &fn, const std::size_t position) -> void {
  // Get the place of the position in the context, which extracts the file, line
  // and column (ignored). This allows us to create mock functions in the debug
  // builder at the exact correct positions.
  if (ctx.DiBuilder == nullptr) { return; }
  const auto [file, line, _] = PlaceOf(ctx, position);

  // The S++ name is the mangled one without its hash ("main::bump$h..."), as
  // the crash report prints it. No linkage name: line tables only carry none,
  // and a name index listing one would disagree with the entry it indexes.
  const auto name = fn.getName().substr(0, fn.getName().find("$h"));
  const auto type = ctx.DiBuilder->createSubroutineType(
    ctx.DiBuilder->getOrCreateTypeArray({}));

  // The flag depends on the line being 0, which means it was compiler-generated
  // code (artificial) rather than "flag zero", meaning no flag.
  const auto flags = line == 0 ? llvm::DINode::FlagArtificial : llvm::DINode::FlagZero;
  fn.setSubprogram(ctx.DiBuilder->createFunction(
    file, name, "", file, line, type, line, flags,
    llvm::DISubprogram::SPFlagDefinition));
}

/// [CHECKED]
auto spp::codegen::SetDebugLocation(
  LlvmCtx &ctx, const std::size_t position) -> void {
  // Get the current debug scope from the builder, and if it's nullptr, then
  // create a new debug location and set it into the builder.
  if (ctx.DiBuilder == nullptr) { return; }
  const auto scope = CurrentDebugScope(ctx);
  if (scope == nullptr) {
    ctx.Builder.SetCurrentDebugLocation(llvm::DebugLoc());
    return;
  }

  // Get the place (line, column) of the position in the context, get the
  // (definitely existing) debug location, and set it to the current place.
  const auto [_, line, column] = PlaceOf(ctx, position);
  ctx.Builder.SetCurrentDebugLocation(
    llvm::DILocation::get(*ctx.Context, line, column, scope));
}

/// [CHECKED]
auto spp::codegen::EnterDebugBlock(
  LlvmCtx &ctx, const std::size_t position) -> void {
  // Get the current debug location, and extract its file, line and column.
  if (ctx.DiBuilder == nullptr) { return; }
  const auto scope = CurrentDebugScope(ctx);
  if (scope == nullptr) { return; }
  const auto [file, line, column] = PlaceOf(ctx, position);

  // Emplace the tuple of the building block, the debug building block, and the
  // current debug location into the blocks list maintained on the LLVM context.
  ctx.DiBlocks.emplace_back(
    ctx.Builder.GetInsertBlock()->getParent(),
    ctx.DiBuilder->createLexicalBlock(scope, file, line, column),
    ctx.Builder.getCurrentDebugLocation());
}

/// [CHECKED]
auto spp::codegen::LeaveDebugBlock(LlvmCtx &ctx) -> void {
  // Only a block "EnterDebugBlock" opened is closed: one opened outside a
  // function with a subprogram never was.
  const auto block = ctx.Builder.GetInsertBlock();
  const auto fn = block != nullptr ? block->getParent() : nullptr;
  if (ctx.DiBlocks.empty() or get<0>(ctx.DiBlocks.back()) != fn) { return; }
  ctx.Builder.SetCurrentDebugLocation(get<2>(ctx.DiBlocks.back()));
  ctx.DiBlocks.pop_back();
}

auto spp::codegen::DebugTypeOf(
  LlvmCtx &ctx, TypeRef const &ref) -> llvm::DIType* {
  using namespace asts::generate::common_types_precompiled;
  if (ctx.DiBuilder == nullptr or not ctx.DiVariables or ref.Symbol == nullptr) { return nullptr; }
  auto &di = *ctx.DiBuilder;
  auto const &dl = ctx.Module->getDataLayout();
  const auto ptr_bits = dl.getPointerSizeInBits();
  const auto ptr_align = static_cast<std::uint32_t>(dl.getPointerABIAlignment(0).value() * 8);

  // A borrow is a pointer to what it borrows, named for its convention.
  if (ref.IsBorrowed()) {
    const auto inner = DebugTypeOf(ctx, ref.WithoutConvention());
    const auto prefix = ref.Conv == asts::ConventionTag::MUT ? "&mut " : "&";
    const auto inner_name = inner != nullptr ? inner->getName().str() : Str("Void");
    return di.createPointerType(inner, ptr_bits, ptr_align, Nullopt, prefix + inner_name);
  }

  // Each type is made once per module, for the symbol its layout belongs to.
  auto *const sym = CanonicalOf(ctx, *ref.Symbol);
  if (const auto it = ctx.DiTypes.find(sym); it != ctx.DiTypes.end()) { return it->second; }
  const auto llvm_type = GetLlvmTypeOf(ref.WithoutConvention(), &ctx);
  if (llvm_type == nullptr or IsValuelessType(llvm_type)) { return nullptr; }
  auto *const scope = sym->LinkedScope != nullptr
    ? sym->LinkedScope
    : sym->Type != nullptr ? sym->Type->GetAstScope() : nullptr;
  const auto [size, align] = BitsOf(ctx, llvm_type);
  const auto remember = [&](llvm::DIType *type) {
    ctx.DiTypes[sym] = type;
    return type;
  };

  // Without a scope to read it in, all that can be said is its size. A type
  // with no fixed size has no offsets for its fields either (a struct with a
  // field not laid out, or holding a scalable vector), so it is named and no
  // more: LLVM's layout of one is not defined.
  if (scope == nullptr or not llvm_type->isSized() or llvm_type->isScalableTy()) {
    return StructureOf(
      ctx, sym, mangle::MangleTypeName(*sym), llvm_type, [](llvm::DIScope*) { return Vec<llvm::Metadata*>(); });
  }
  const auto cls_ref = TypeRef::Of(*sym, *scope);
  const auto is = [&](asts::TypeAst const &tmpl) { return cls_ref.IsA(tmpl, *scope); };

  // "Bool", the numbers, and "F8", stored as its bits.
  if (is(*BOOL)) {
    return remember(di.createBasicType("Bool", size, llvm::dwarf::DW_ATE_boolean));
  }
  if (is(*SIZED_INTEGER)) {
    const auto is_signed = [&] {
      const auto id = sym->CompArgId("signed");
      auto const *const value = id != nullptr ? id->AsBool() : nullptr;
      return value != nullptr and *value;
    }();
    const auto width = llvm_type->getIntegerBitWidth();
    return remember(di.createBasicType(
      std::format("{}{}", is_signed ? 'S' : 'U', width), size,
      is_signed ? llvm::dwarf::DW_ATE_signed : llvm::dwarf::DW_ATE_unsigned));
  }
  if (is(*F8)) {
    return remember(di.createBasicType("F8", size, llvm::dwarf::DW_ATE_unsigned));
  }
  if (is(*SIZED_FLOAT)) {
    return remember(di.createBasicType(
      std::format("F{}", llvm_type->getPrimitiveSizeInBits().getFixedValue()), size, llvm::dwarf::DW_ATE_float));
  }

  // Anything else is named as S++ spells it ("DisplayNameOf").
  const auto name = DisplayNameOf(ctx, *sym, *scope);

  // "NonNull[T]" and "Ptr[T]": a pointer to "T". Remembered before "T" is
  // described, as "T" may point back here.
  if (is(*NON_NULL) or is(*PTR)) {
    auto *const pointer = di.createPointerType(nullptr, ptr_bits, ptr_align, Nullopt, name);
    remember(pointer);
    const auto pointee = DebugTypeOf(ctx, sym->TypeArgRef("T"));
    auto *const typed = di.createPointerType(pointee, ptr_bits, ptr_align, Nullopt, name);
    return remember(typed);
  }

  // "Arr[T, n]": "n" of "T".
  if (is(*ARR)) {
    const auto elem = DebugTypeOf(ctx, sym->TypeArgRef("T"));
    const auto count = static_cast<std::int64_t>(llvm::cast<llvm::ArrayType>(llvm_type)->getNumElements());
    return remember(di.createArrayType(
      size, align, elem, di.getOrCreateArray({di.getOrCreateSubrange(0, count)})));
  }

  // A variant: its tag, an enumeration named by each member in tag order, and
  // its payload, a union of the members.
  if (is(*VAR)) {
    return StructureOf(ctx, sym, name, llvm_type, [&](llvm::DIScope *variant) {
      const auto members = analyse::utils::type_members::VariantMemberRefs(cls_ref, *scope);
      auto enumerators = Vec<llvm::Metadata*>();
      auto alternatives = Vec<llvm::Metadata*>();
      const auto payload_type = llvm::cast<llvm::StructType>(llvm_type)->getElementType(1);
      auto *payload = di.createUnionType(
        variant, "payload", ctx.DiUnit->getFile(), 0, BitsOf(ctx, payload_type).first,
        BitsOf(ctx, payload_type).second, llvm::DINode::FlagZero, di.getOrCreateArray({}));
      for (auto i = 0uz; i < members.size(); ++i) {
        const auto member_type = DebugTypeOf(ctx, members[i]);
        const auto member_name = NameOf(ctx, members[i]);
        enumerators.push_back(di.createEnumerator(member_name, static_cast<std::uint64_t>(i), true));
        if (member_type != nullptr) {
          alternatives.push_back(MemberOf(ctx, payload, member_name, member_type, GetLlvmTypeOf(members[i], &ctx), 0));
        }
      }
      di.replaceArrays(payload, di.getOrCreateArray(alternatives));
      const auto tag_type = GetVariantTagType(&ctx);
      const auto [tag_size, tag_align] = BitsOf(ctx, tag_type);
      const auto tag = di.createEnumerationType(
        variant, name + "::tag", ctx.DiUnit->getFile(), 0, tag_size, tag_align, di.getOrCreateArray(enumerators),
        di.createBasicType("tag", tag_size, llvm::dwarf::DW_ATE_unsigned), 0, "", true);
      auto const *const layout = dl.getStructLayout(llvm::cast<llvm::StructType>(llvm_type));
      return Vec<llvm::Metadata*>{
        MemberOf(ctx, variant, "tag", tag, tag_type, layout->getElementOffsetInBits(0)),
        di.createMemberType(
          variant, "payload", ctx.DiUnit->getFile(), 0, BitsOf(ctx, payload_type).first,
          BitsOf(ctx, payload_type).second, layout->getElementOffsetInBits(1), llvm::DINode::FlagZero, payload)
      };
    });
  }

  // A structure: a tuple's elements ("0", "1", ...), or a class's attributes,
  // after the fat pointer of a class superimposing a "Fun*" or "Gen*" type,
  // each at its offset in the layout the class was given.
  if (const auto structure = llvm::dyn_cast<llvm::StructType>(llvm_type); structure != nullptr and
    not structure->isOpaque()) {
    return StructureOf(ctx, sym, name, llvm_type, [&](llvm::DIScope *owner) {
      auto const *const layout = dl.getStructLayout(structure);
      auto fields = Vec<llvm::Metadata*>();
      const auto add = [&](const StrView field_name, std::size_t logical, llvm::DIType *type) {
        const auto physical = GetPhysicalFieldIndex(*sym->LlvmInfo, logical);
        if (type == nullptr or physical >= structure->getNumElements()) { return; }
        fields.push_back(MemberOf(
          ctx, owner, field_name, type, structure->getElementType(physical), layout->getElementOffsetInBits(physical)));
      };

      // A function value's own pair, which is all a "Fun*" type or closure is.
      if (analyse::utils::type_members::FatPointerWidthOf(TypeRef::ForKindCheck(*sym, *scope), *scope) == 2) {
        const auto void_ptr = di.createPointerType(nullptr, ptr_bits, ptr_align);
        fields.push_back(MemberOf(ctx, owner, "fn", void_ptr, structure->getElementType(0), 0));
        fields.push_back(MemberOf(
          ctx, owner, "env", void_ptr, structure->getElementType(1), layout->getElementOffsetInBits(1)));
        return fields;
      }

      if (analyse::utils::type_predicates::IsTypeTuple(TypeRef::ForKindCheck(*sym, *scope), *scope)) {
        const auto elems = sym->TypeArgRefs();
        for (auto i = 0uz; i < elems.size(); ++i) { add(std::format("{}", i), i, DebugTypeOf(ctx, elems[i])); }
        return fields;
      }

      const auto prefix = analyse::utils::type_members::GetSuperimposedFatPointerFieldCount(*sym);
      if (prefix > 0) {
        const auto void_ptr = di.createPointerType(nullptr, ptr_bits, ptr_align);
        add("fn", 0, void_ptr);
        if (prefix > 1) { add("env", 1, void_ptr); }
      }
      auto index = prefix;
      for (auto const &[attr_name, attr_ref, attr_scope] : analyse::utils::type_members::GetAllAttrs(*sym)) {
        static_cast<void>(attr_scope);
        add(attr_name->Val, index++, DebugTypeOf(ctx, attr_ref));
      }
      return fields;
    });
  }

  // A single pointer (a generator's handle), or anything else of a size.
  if (llvm_type->isPointerTy()) {
    return remember(di.createPointerType(nullptr, ptr_bits, ptr_align, Nullopt, name));
  }
  return StructureOf(ctx, sym, name, llvm_type, [](llvm::DIScope*) { return Vec<llvm::Metadata*>(); });
}

auto spp::codegen::DeclareVariable(
  LlvmCtx &ctx, LlvmVarSymbolInfo &info, const StrView name, TypeRef const &ref, const std::size_t position,
  const unsigned arg_no) -> void {
  const auto storage = info.Alloca;
  // A compiler temporary ("$temp...") is no variable the program names.
  if (ctx.DiBuilder == nullptr or not ctx.DiVariables or storage == nullptr) { return; }
  if (name.starts_with('$')) { return; }
  const auto scope = CurrentDebugScope(ctx);
  if (scope == nullptr) { return; }
  const auto type = DebugTypeOf(ctx, ref);
  if (type == nullptr) { return; }

  // At the binding's own line, in its block (a parameter in its function); a
  // debugger shows it there on, to the end of the block.
  const auto [file, line, column] = PlaceOf(ctx, position);
  const auto variable = arg_no > 0
    ? ctx.DiBuilder->createParameterVariable(scope->getSubprogram(), name, arg_no, file, line, type, true)
    : ctx.DiBuilder->createAutoVariable(scope, name, file, line, type, true);
  const auto at = llvm::DILocation::get(*ctx.Context, line, column, scope);

  // A binding that can be moved is described from where it is given a value
  // ("ResumeVariable", by its "let" once the value is stored) to where it is
  // moved ("EndVariable"); any other for its whole block, which nothing ends.
  const auto movable = not ref.IsBorrowed() and ref.Symbol != nullptr and not ref.Symbol->IsCopyable();
  if (movable) {
    info.DiVariable = variable;
    return;
  }
  ctx.DiBuilder->insertDeclare(storage, variable, ctx.DiBuilder->createExpression(), at, ctx.Builder.GetInsertPoint());
}

namespace spp::codegen {
  namespace {
    /// Where a move or an assignment of "variable" is: the builder's location,
    /// when it is in the variable's own function. Null elsewhere (a binding of
    /// the parent read inside a closure's body), where nothing is said of it.
    auto VariableEventAt(
      LlvmCtx const &ctx, llvm::DILocalVariable const &variable) -> llvm::DILocation* {
      const auto scope = CurrentDebugScope(ctx);
      if (scope == nullptr or scope->getSubprogram() != variable.getScope()->getSubprogram()) { return nullptr; }
      const auto current = ctx.Builder.getCurrentDebugLocation();
      return current
        ? llvm::DILocation::get(*ctx.Context, current.getLine(), current.getCol(), scope)
        : llvm::DILocation::get(*ctx.Context, 0, 0, scope);
    }
  }
}

auto spp::codegen::EndVariable(
  LlvmCtx &ctx, LlvmVarSymbolInfo const &info) -> void {
  if (ctx.DiBuilder == nullptr or info.DiVariable == nullptr) { return; }
  const auto at = VariableEventAt(ctx, *info.DiVariable);
  if (at == nullptr) { return; }
  ctx.DiBuilder->insertDbgValueIntrinsic(
    llvm::PoisonValue::get(info.Alloca->getType()), info.DiVariable, ctx.DiBuilder->createExpression(), at,
    ctx.Builder.GetInsertPoint());
}

auto spp::codegen::ResumeVariable(
  LlvmCtx &ctx, LlvmVarSymbolInfo const &info) -> void {
  if (ctx.DiBuilder == nullptr or info.DiVariable == nullptr) { return; }
  const auto at = VariableEventAt(ctx, *info.DiVariable);
  if (at == nullptr) { return; }
  ctx.DiBuilder->insertDbgValueIntrinsic(
    info.Alloca, info.DiVariable,
    ctx.DiBuilder->createExpression(llvm::ArrayRef<std::uint64_t>{llvm::dwarf::DW_OP_deref}), at,
    ctx.Builder.GetInsertPoint());
}
