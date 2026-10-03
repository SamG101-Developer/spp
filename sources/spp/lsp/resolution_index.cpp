module;
#include <spp/macros.hpp>

module spp.lsp.resolution_index;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.visibility_utils;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.error_formatter;
import spp.utils.files;
import genex;
import std;

namespace spp::lsp::resolution_index {
  namespace {
    /// Which uses are worth keeping. Not named "Scope": that
    /// is the analyser's own, and this file uses both.
    enum class Kept { None, Files, Project };

    /// The state is the master collection of indexed values.
    struct State {
      Kept Where = Kept::None;
      Vec<Str> Files;

      /// The entries provide the "hover" and "goto"-navigation
      /// data. They store the span encompassing the definition,
      /// against where it was used, and additional metadata.
      Vec<ResolvedName> Entries;
      Set<Str> Seen;

      /// Each type or namespace reached, proving data for the
      /// completion menu after "." or "::". Visibility filtered.
      Vec<MemberList> Members;
      Set<Str> SeenOwners;

      Vec<Signature> Signatures;
      Set<Str> SeenCalls;

      Vec<NamesInScope> Scopes;
      Set<Str> SeenScopes;

      Vec<CompTimeValue> Values;
      Set<Str> SeenValues;
    };

    /// Singleton instance to the static state for the current
    /// compilation / indexing.
    auto Current() -> State& {
      static auto state = State();
      return state;
    }

    /// How a function can be called, one entry per overload: its parameters and what it gives back.
    ///
    /// A method is a mock constant, and the overloads are the "sup $M ext FunXxx { fun m(..) }" blocks superimposed
    /// over that mock - the same blocks a call is resolved against.
    auto SignaturesOf(
      VariableSymbol const &sym, Scope const &scope) -> Vec<Str> {
      auto signatures = Vec<Str>();
      if (sym.Type == nullptr) { return signatures; }

      // Get the mock function type for the function symbol. This
      // will take the function "func" and get "$Type". This holds
      // the overloads via the sup scopes.
      const auto mock = scope.FindTypeSymbol(sym.Type.get());
      if (mock == nullptr or mock->LinkedScope == nullptr) { return signatures; }

      for (const auto overload : mock->LinkedScope->DirectSupScopes) {
        // Get the "sup-ext" asts, which are the superimpositions
        // of the "FunXXX" family of types. Extract the function
        // prototype from the "sup-ext".
        const auto ext = AstAs<SupPrototypeExtensionAst>(overload->AstNode);
        if (ext == nullptr) { continue; }
        const auto body = AstBody(ext);
        const auto proto = not body.IsEmpty() ? body[0]->To<FunctionPrototypeAst>() : nullptr;
        if (proto == nullptr or proto->FnParamGroup == nullptr or proto->ReturnType == nullptr) { continue; }

        // Manual stringification because ToString() leaves in the
        // trailing "," on parameters. Todo: Use .ToString() once
        // that bug is fixed.
        auto params = Str("(");
        for (auto const &param : proto->FnParamGroup->Params) {
          params += (params.length() > 1 ? ", " : "") + param->ToString();
        }
        signatures.EmplaceBack(params + ") -> " + proto->ReturnType->ToString());
      }
      return signatures;
    }

    /// The stringification of the symbol's representation; is
    /// this a "function" or an "attribute" etc, for the IDEA
    /// plugin to specify.
    auto KindOf(
      VariableSymbol const &sym) -> Str {
      switch (sym.Kind) {
        case VariableKind::FnMock: return "function";
        case VariableKind::Attribute: return "attribute";
        case VariableKind::Constant: return "constant";
        case VariableKind::GnCompParam:
        case VariableKind::GnCompArg: return "generic";
        default: return "variable";
      }
    }

    /// Convert the namespace symbol into the stringified namespace
    /// path joined with "::".
    auto NamespacePathOf(
      NamespaceSymbol const &sym)
      -> Str {
      // The current namespace part and all parent scopes up to the root.
      auto parts = Vec<Str>();
      for (auto const *scope : sym.LinkedScope->GetAncestors()) {
        parts.EmplaceBack(scope->LinkedNamespaceSymbol->Name->ToString());
      }

      // Join all the parts into a "::"-joined string, leaving
      // the "_global" root part. Todo: Move to a constant.
      auto path = Str();
      for (auto i = parts.Len(); i > 0; --i) {
        if (parts[i - 1] == "_global") { continue; }
        path += (path.empty() ? "" : "::") + parts[i - 1];
      }
      return path;
    }

    /// The whole of what an ast covers, across however many lines:
    /// a region, rather than the place a quote would underline.
    /// Uses a start and end position rather than the ast.
    auto RegionOf(
      Ast const *const ast, Scope const &scope) -> SourceSpan {
      // Create a span over the ast, using the error formatter
      // from the scope.
      const auto formatter = scope.GetErrorFormatter();
      if (ast == nullptr or formatter == nullptr or ast->PosStart() == 0) { return {}; }
      return formatter->SpanAcross(ast->PosStart(), ast->PosEnd());
    }

    /// Same as the region span, but using the ast rather than the
    /// positions.
    auto SpanOf(
      Ast const *const ast, Scope const &scope) -> SourceSpan {
      // Create a span over the ast, using the error formatter
      // from the scope.
      const auto formatter = scope.GetErrorFormatter();
      if (ast == nullptr or formatter == nullptr) { return {}; }
      return formatter->SpanOfAst(ast);
    }

    /// Where a type was declared. A "use" import is walked through to what it imported, because nobody wants to land
    /// on the import; a "type" statement is not, because the alias is the thing that was named.
    auto DefinitionOfType(
      TypeSymbol const &sym)
      -> spp::utils::errors::SourceSpan {
      auto const *const target = sym.UseTarget();

      // The definition requires a different ast and scope depending
      // on whether an alias is being used or not.
      if (target->Alias != nullptr
        and not target->Alias->IsFromUseStmt
        and target->Alias->Stmt != nullptr
        and target->Alias->WrittenIn != nullptr) {
        return SpanOf(target->Alias->Stmt->NewType.get(), *target->Alias->WrittenIn);
      }
      if (sym.Type != nullptr and sym.Type->GetAstScope() != nullptr) {
        return SpanOf(sym.Type->Name.get(), *sym.Type->GetAstScope());
      }
      if (sym.ScopeDefinedIn != nullptr) {
        return SpanOf(sym.Name.get(), *sym.ScopeDefinedIn);
      }
      return {};
    }

    /// Whether a type name is writable by the user or not. This
    /// filters out compiler-generated function mock $Types, and
    /// has a flag to determine whether to allow generics or not.
    auto IsWritableTypeName(
      TypeSymbol const &sym,
      const bool generics)
      -> bool {
      // Check the flags on the symbol, the name as a failsafe,
      // the generic state vs whether generics are allowed, and
      // the symbol kind.
      if (sym.Name == nullptr or sym.InstanceOf != nullptr or sym.IsMock() or sym.IsSelf()) { return false; }
      if (sym.Name->ToString().starts_with("$")) { return false; }
      if (sym.IsGn()) { return generics; }
      return sym.Kind == TypeKind::Cls or sym.Kind == TypeKind::Alias;
    }

    /// Whether a variable name is writable by the user or not.
    /// This filters out compiler-generated variable names like
    /// $xyz that are used in preprocessing / ast manipulation.
    auto IsWritableVarName(
      VariableSymbol const &sym)
      -> bool {
      // Check the flags on the symbol, and the name as a failsafe.
      if (sym.Name == nullptr or sym.Kind == VariableKind::Temporary) { return false; }
      return not sym.Name->ToString().starts_with("$");
    }

    /// The uniform key creation function the the "seen" tables.
    /// The joining character is a character that no variable or
    /// type name will ever contain. The first part is the "kind",
    /// followed by the uniquely identifying information.
    template <typename... Parts>
    auto KeyOf(Parts const &... parts) -> Str {
      auto key = Str();
      ((key += std::format("{}", parts), key += '|'), ...);
      return key;
    }

    /// The wrapper to generate a key from span information. Pull
    /// the attributes off of the span, and pass them into the
    /// general key creation method.
    auto KeyOfSpan(Str const &what, SourceSpan const &span) -> Str {
      return KeyOf(what, span.File, span.StartLine, span.StartCol, span.EndLine, span.EndCol);
    }

    /// Whether the analysis is at the stage where filling out
    /// completion lists is actually correct. Sup scope handling,
    /// visibility, etc, must have all been processed.
    auto WantsMembersYet(CompilerMetaData const &meta) -> bool {
      return meta.CurrentStage >= CompilerStage::kAnalyseSemantics;
    }

    /// Everything one type holds, its super types included - the
    /// same set a member access is resolved against, which is
    /// what makes the list an editor offers the list that would
    /// actually work.
    auto MembersOfTypeScope(
      Scope const &type_scope, ScopeManager const &sm,
      CompilerMetaData const &meta) -> Vec<Member> {
      auto members = Vec<Member>();
      auto seen = Set<Str>();

      // Iterate through all the variable symbols on the type,
      // which are reachable like "Type::constant" and also
      // "Type::method()".
      for (const auto sym : type_scope.GetAllVarSymbols(true, true)) {
        if (sym == nullptr or not IsWritableVarName(*sym)) { continue; }
        if (not analyse::utils::visibility_utils::IsTypeMemberVisible(*sym, type_scope, sm, meta)) { continue; }

        auto name = sym->Name->ToString();
        auto key = KeyOf("var", name, sym->Type != nullptr ? sym->Type->ToString() : Str());
        if (not seen.insert(std::move(key)).second) { continue; }

        members.EmplaceBack(Member{
          .Name = std::move(name),
          .Kind = KindOf(*sym),
          .Type = sym->Type != nullptr ? sym->Type->ToString() : Str(),
          .Definition = sym->ScopeDefinedIn != nullptr ? SpanOf(sym->Name.get(), *sym->ScopeDefinedIn) : SourceSpan(),
          .Signatures = SignaturesOf(*sym, type_scope)
        });
      }

      // Iterate through all the type symbols on the type, which
      // are reachable like "Type::Inner", defined on the sup
      // blocks.
      for (auto const *sym : type_scope.GetAllTypeSymbols(true, true)) {
        if (sym == nullptr or not IsWritableTypeName(*sym, false)) { continue; }
        if (not analyse::utils::visibility_utils::IsTypeTypeVisible(*sym, type_scope, sm, meta)) { continue; }

        auto name = sym->Name->ToString();
        auto key = KeyOf("type", name);
        if (not seen.insert(std::move(key)).second) { continue; }

        members.EmplaceBack(Member{
          .Name = std::move(name),
          .Kind = "type",
          .Type = Str(),
          .Definition = DefinitionOfType(*sym)
        });
      }
      return members;
    }

    /// Everything one namespace holds - the same set a member
    /// access is resolved against, which is what makes the
    /// list an editor offers the list that would actually work.
    auto MembersOfNamespaceScope(
      Scope const &ns_scope, ScopeManager const &sm,
      CompilerMetaData const &meta) -> Vec<Member> {
      auto members = Vec<Member>();
      auto seen = Set<Str>();

      // Iterate through all the variable symbols on the namespace,
      // which are reachable like "namespace::constant" and also
      // "namespace::inner::constant".
      for (const auto sym : ns_scope.GetAllVarSymbols(true, false)) {
        if (sym == nullptr or not IsWritableVarName(*sym)) { continue; }
        const auto definition_scope = sym->ScopeDefinedIn != nullptr ? sym->ScopeDefinedIn : &ns_scope;
        if (not analyse::utils::visibility_utils::IsModuleMemberVisible(*sym, *definition_scope, sm, meta)) { continue; }

        auto name = sym->Name->ToString();
        auto key = KeyOf("var", name, sym->Type != nullptr ? sym->Type->ToString() : Str());
        if (not seen.insert(std::move(key)).second) { continue; }

        members.EmplaceBack(Member{
          .Name = std::move(name),
          .Kind = KindOf(*sym),
          .Type = sym->Type != nullptr ? sym->Type->ToString() : Str(),
          .Definition = sym->ScopeDefinedIn != nullptr ? SpanOf(sym->Name.get(), *sym->ScopeDefinedIn) : SourceSpan(),
          .Signatures = SignaturesOf(*sym, ns_scope)
        });
      }

      // Iterate through all the type symbols on the namespace,
      // which are reachable like "namespace::Type".
      for (auto const *sym : ns_scope.GetAllTypeSymbols(true, false)) {
        if (sym == nullptr or not IsWritableTypeName(*sym, false)) { continue; }
        const auto definition_scope = sym->ScopeDefinedIn != nullptr ? sym->ScopeDefinedIn : &ns_scope;
        if (not analyse::utils::visibility_utils::IsModuleTypeVisible(*sym, *definition_scope, sm, meta)) { continue; }

        auto name = sym->Name->ToString();
        auto key = KeyOf("type", name);
        if (not seen.insert(std::move(key)).second) { continue; }

        members.EmplaceBack(Member{
          .Name = std::move(name),
          .Kind = "type",
          .Type = Str(),
          .Definition = DefinitionOfType(*sym)
        });
      }

      // Iterate through all the namespace symbols on the namespace,
      // which are reachable like "namespace::inner_ns".
      for (const auto sym : ns_scope.GetAllNsSymbols(true)) {
        if (sym == nullptr or sym->Name == nullptr) { continue; }
        members.EmplaceBack(Member{
          .Name = sym->Name->ToString(),
          .Kind = "namespace",
          .Type = Str(),
          .Definition = SourceSpan()
        });
      }
      return members;
    }

    /// Keep what a name resolved to, dropping it when it is not
    /// in a file being indexed. Every other recorder ends here.
    auto Record(ResolvedName &&entry) -> void {
      // A name with no place in the source is one the compiler
      // made up (internally generated), and a name written in
      // another file is not what was asked for; neither kept.
      if (entry.Use.Generated or not Wants(entry.Use.File)) { return; }
      auto key = KeyOfSpan("use", entry.Use) + KeyOf(entry.Kind, entry.Name);
      if (not Current().Seen.insert(std::move(key)).second) { return; }
      Current().Entries.EmplaceBack(std::move(entry));
    }

    /// Keep what a call resolved to, so that the names between its
    /// brackets can be offered.
    auto RecordSignature(
      Ast const &arguments, ScopeManager const &sm, CompilerMetaData const &,
      Str name, Vec<Member> params) -> void {
      // If we are not bothered about this scope, then return early;
      // no work needs to be done here.
      auto const &use_scope = *sm.CurrentScope;
      if (not WantsScope(use_scope)) { return; }
      auto span = RegionOf(&arguments, use_scope);
      if (span.Generated) { return; }

      // One record per call, however many times the node is analysed.
      auto key = KeyOfSpan("call", span);
      if (not Current().SeenCalls.insert(std::move(key)).second) { return; }
      auto signature = Signature{
        .Arguments = std::move(span), .Name = std::move(name), .Params = std::move(params)
      };
      Current().Signatures.EmplaceBack(std::move(signature));
    }

    /// Get a list of the members of the type, push it into the
    /// state's members. Scope check for to prevent duplication.
    auto RecordTypeMembers(
      Str owner, Scope const &type_scope, ScopeManager const &sm,
      CompilerMetaData const &meta) -> void {
      // Get the members of a type and create a member list containing
      // them, assigned against the owner type (pre-serialized).
      if (owner.empty() or not WantsMembersYet(meta) or not type_scope.SupsAttached) { return; }
      if (not Current().SeenOwners.insert(KeyOf("type", owner)).second) { return; }
      auto members = MembersOfTypeScope(type_scope, sm, meta);
      Current().Members.EmplaceBack(MemberList{
        .Owner = std::move(owner), .Of = "type", .Members = std::move(members)
      });
    }

    /// Get a list of the members of the namespaces, push it into
    /// the state's members. Scope check for to prevent duplication.
    auto RecordNamespaceMembers(
      Str owner, Scope const &ns_scope, ScopeManager const &sm,
      CompilerMetaData const &meta) -> void {
      // Get the members of a namespace and create a member list
      // containing them, assigned against the owner namespace
      // (pre-serialized).
      if (owner.empty() or not WantsMembersYet(meta)) { return; }
      if (not Current().SeenOwners.insert(KeyOf("ns", owner)).second) { return; }
      auto members = MembersOfNamespaceScope(ns_scope, sm, meta);
      Current().Members.EmplaceBack(MemberList{
        .Owner = std::move(owner), .Of = "namespace", .Members = std::move(members)
      });
    }

    /// Keep the use of a namespace - the "std", "mem" and "ops" of
    /// "std::mem::ops::mem_cmp". A namespace is a folder or a file
    /// rather than a declaration, so navigate to the representing
    /// file.
    auto RecordNamespace(
      Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta,
      NamespaceSymbol const *const sym_ptr) -> void {
      // If we are not bothered about this scope, then return early;
      // no work needs to be done here.
      auto const &use_scope = *sm.CurrentScope;
      if (sym_ptr == nullptr or not WantsScope(use_scope)) { return; }
      auto const &sym = *sym_ptr;

      // The definition is the span of the name within the scope
      // that the symbol was defined in.
      auto definition = SourceSpan();
      if (sym.LinkedScope != nullptr) {
        if (const auto module = AstAs<ModulePrototypeAst>(sym.LinkedScope->AstNode); module != nullptr) {
          definition = SourceSpan{
            .File = spp::utils::files::DisplayString(module->FilePath), .Generated = false
          };
        }
      }

      // And what the namespace holds is what is offered after a
      // "::" on it.
      if (sym.LinkedScope != nullptr) {
        RecordNamespaceMembers(NamespacePathOf(sym), *sym.LinkedScope, sm, meta);
      }

      // Call the master recorder function with the information
      // about this variable symbol.
      Record({
        .Use = SpanOf(&use, use_scope),
        .Kind = "namespace",
        .Name = sym.Name != nullptr ? sym.Name->ToString() : Str(),
        .Type = NamespacePathOf(sym),
        .Definition = std::move(definition)
      });
    }
  }
}

auto spp::lsp::resolution_index::EnableFiles(
  Vec<Str> files)
  -> void {
  // Tell the state to keep the files if there are any,
  // otherwise nothing is being kept. Store the file list.
  Current().Where = files.IsEmpty() ? Kept::None : Kept::Files;
  Current().Files = std::move(files);
}

auto spp::lsp::resolution_index::EnableProject()
  -> void {
  // Tell the state to maintain indexes for the current
  // project (no vcs dependencies).
  Current().Where = Kept::Project;
}

auto spp::lsp::resolution_index::Disable()
  -> void {
  // Tell the state to not maintain indexes, and clear
  // the file list.
  Current().Where = Kept::None;
  Current().Files.Clear();
}

auto spp::lsp::resolution_index::IsEnabled()
  -> bool {
  // The indexing is only enabled if the state is not
  // storing nothing.
  return Current().Where != Kept::None;
}

auto spp::lsp::resolution_index::Wants(
  Str const &file)
  -> bool {
  // A file is only wanted if the state is indexing the current
  // project and this is a non-vcs file, or a restricted file list
  // that contains this file. Otherwise, it is not wanted.
  switch (Current().Where) {
    case Kept::Files: return genex::contains(Current().Files, file);
    case Kept::Project: return not file.contains("/vcs/") and not file.contains("\\vcs\\");
    case Kept::None:
    default: return false;
  }
}

auto spp::lsp::resolution_index::WantsScope(
  Scope const &scope)
  -> bool {
  // Asked at every name a compile resolves, and almost always
  // answered "no": that answer is given before the scope's
  // module is even looked for.
  if (not IsEnabled()) { return false; }
  const auto formatter = scope.GetErrorFormatter();
  return formatter != nullptr and Wants(formatter->FilePath());
}

auto spp::lsp::resolution_index::RecordVariable(
  Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta,
  VariableSymbol const *const sym_ptr) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  auto const &use_scope = *sm.CurrentScope;
  if (sym_ptr == nullptr or not WantsScope(use_scope)) { return; }
  auto const &sym = *sym_ptr;

  // For symbols introduced via a "use" statement, we want to
  // get back to the actual cmp/fun definition in a different
  // module. Todo: Can we remove the "hop" mechanic? The "use"
  // uses private visibility so shouldn't have > 1 hop.
  auto target = &sym;
  for (auto hops = 0; target->AliasSymbol != nullptr and hops < 8; ++hops) { target = target->AliasSymbol.get(); }

  // The definition is the span of the name within the scope
  // that the symbol was defined in.
  auto definition = SourceSpan();
  if (target->ScopeDefinedIn != nullptr) {
    definition = SpanOf(target->Name.get(), *target->ScopeDefinedIn);
  }

  // What a value of this type holds is what an editor offers
  // after a "." on it. Track members of this variable's type
  // to load the completion menu.
  if (sym.Type != nullptr) {
    if (const auto type_sym = use_scope.FindTypeSymbol(sym.Type.get());
      type_sym != nullptr and type_sym->LinkedScope != nullptr) {
      RecordTypeMembers(sym.Type->ToString(), *type_sym->LinkedScope, sm, meta);
    }
  }

  // Call the master recorder function with the information
  // about this variable symbol.
  Record({
    .Use = SpanOf(&use, use_scope),
    .Kind = KindOf(*target),
    .Name = sym.Name != nullptr ? sym.Name->ToString() : Str(),
    .Type = sym.Type != nullptr ? sym.Type->ToString() : Str(),
    .Definition = std::move(definition),

    // Only cmp constants have their value recorded; a function
    // providing the $Func() value doesn't make any sense to
    // expose.
    .Value = target->Kind == VariableKind::Constant and target->CompTimeValue != nullptr
    ? target->CompTimeValue->ToString()
    : Str()
  });
}

auto spp::lsp::resolution_index::RecordType(
  Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta,
  TypeSymbol const *const sym_ptr) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  auto const &use_scope = *sm.CurrentScope;
  if (sym_ptr == nullptr or not WantsScope(use_scope)) { return; }
  auto const &sym = *sym_ptr;

  // And what the type itself holds is what is offered after a
  // "::" on its name.
  if (sym.LinkedScope != nullptr and sym.FqName() != nullptr) {
    RecordTypeMembers(sym.FqName()->ToString(), *sym.LinkedScope, sm, meta);
  }

  // Call the master recorder function with the information
  // about this variable symbol.
  Record({
    .Use = SpanOf(&use, use_scope),
    .Kind = "type",
    .Name = sym.Name != nullptr ? sym.Name->ToString() : Str(),
    .Type = sym.FqName() != nullptr ? sym.FqName()->ToString() : Str(),
    .Definition = DefinitionOfType(sym)
  });
}

auto spp::lsp::resolution_index::RecordIdentifier(
  Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta,
  VariableSymbol const *const sym) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  if (not WantsScope(*sm.CurrentScope)) { return; }

  // A name with no variable behind it can still be the root
  // of a path - the "std" of "std::mem::ops::mem_cmp".
  if (sym != nullptr) { return RecordVariable(use, sm, meta, sym); }
  RecordNamespace(
    use, sm, meta, sm.CurrentScope->FindNsSymbol(use.ToUnchecked<IdentifierAst>()));
}

auto spp::lsp::resolution_index::RecordExpression(
  Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta,
  std::function<Shared<TypeAst>()> const &infer) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  auto const &use_scope = *sm.CurrentScope;
  if (not WantsScope(use_scope)) { return; }

  // Get the type of the expression via the inference. The type
  // isn't passed in because we might not need it, which is why
  // the inference gets deferred.
  auto type = Shared<TypeAst>(nullptr);
  try { type = infer(); }
  catch (...) { return; }
  if (type == nullptr) { return; }

  // What the expression produced is what a "." on it reaches
  // into, and a call or a chain of accesses is not a name that
  // anything else records.
  const auto written = type->ToString();
  if (const auto type_sym = use_scope.FindTypeSymbol(type.get());
    type_sym != nullptr and type_sym->LinkedScope != nullptr) {
    RecordTypeMembers(written, *type_sym->LinkedScope, sm, meta);
  }

  // Call the master recorder function with the information
  // about this variable symbol.
  Record({
    .Use = SpanOf(&use, use_scope),
    .Kind = "expression",
    .Name = Str(),
    .Type = written,
    .Definition = SourceSpan() // No written symbolic definition.
  });
}

auto spp::lsp::resolution_index::RecordNamespaceMember(
  IdentifierAst const &use, ScopeManager const &sm, CompilerMetaData const &meta,
  Scope const &ns_scope) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  if (not WantsScope(*sm.CurrentScope)) { return; }

  // When we have found a namespace member that it itself a
  // namespace, record it as so (pre-detected from postfix
  // static member access.
  RecordNamespace(use, sm, meta, ns_scope.FindNsSymbol(&use, true));
}

auto spp::lsp::resolution_index::RecordObjectInitializerArguments(
  Vec<ObjectInitializerArgumentKeywordAst*> const &args,
  Vec<Tup<Shared<IdentifierAst>, TypeRef, Scope*>> const &attrs,
  ScopeManager const &sm, CompilerMetaData const &) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  auto const &use_scope = *sm.CurrentScope;
  if (not WantsScope(use_scope)) { return; }

  const auto attr_name_of = [](auto const &attr) -> decltype(auto) {
    return genex::meta::deref(spp::get<0>(attr));
  };

  for (const auto arg : args) {
    // Find the corresponding attribute for the argument, skip
    // any non-matches.
    if (arg == nullptr or arg->Name == nullptr) { continue; }
    const auto match = genex::find(attrs, *arg->Name, attr_name_of);
    if (match == attrs.end()) { continue; }

    auto const &[attr_name, attr_type, attr_scope] = *match;
    if (attr_scope == nullptr) { continue; }

    // Call the master recorder function with the information
    // about this object initializer argument.
    Record({
      .Use = SpanOf(arg->Name.get(), use_scope),
      .Kind = "attribute",
      .Name = arg->Name->ToString(),
      .Type = attr_type.Id != nullptr ? attr_type.AstIn(*attr_scope)->ToString() : Str(),
      .Definition = SpanOf(attr_name.get(), *attr_scope)
    });
  }
}

auto spp::lsp::resolution_index::RecordFnCallArguments(
  FunctionCallArgumentGroupAst const &arg_group, ScopeManager const &sm, CompilerMetaData const &meta,
  FunctionPrototypeAst const &proto, Scope const &overload_scope) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  auto const &use_scope = *sm.CurrentScope;
  if (not WantsScope(use_scope) or proto.FnParamGroup == nullptr) { return; }
  auto const &params = proto.FnParamGroup->Params;

  // What the call takes, for the names offered between its
  // brackets.
  auto taken = Vec<Member>();
  for (auto const &param : params) {
    const auto param_name = param->ExtractName();
    if (param_name == nullptr) { continue; }
    taken.EmplaceBack(Member{
      .Name = param_name->ToString(),
      .Kind = "parameter",
      .Type = param->Type != nullptr ? param->Type->ToString() : Str(),
      .Definition = {}
    });
  }

  // Record the signature.
  RecordSignature(
    arg_group, sm, meta, proto.Name != nullptr ? proto.Name->ToString() : Str(), std::move(taken));

  for (const auto kw_arg : arg_group.GetKeywordArgs()) {
    // Find the parameter the written name refers to, skipping
    // any non-matches.
    auto const &written = kw_arg->Name;
    const auto match = genex::find_if(params, [&](auto const &param) {
      const auto param_name = param->ExtractName();
      return param_name != nullptr and *param_name == *written;
    });
    if (match == params.end()) { continue; }

    auto const &param = *match;
    const auto param_name = param->ExtractName();

    // Call the master recorder function with the information
    // about this object initializer argument.
    Record({
      .Use = SpanOf(written.get(), use_scope),
      .Kind = "parameter",
      .Name = written->ToString(),
      .Type = param->Type != nullptr ? param->Type->ToString() : Str(),
      .Definition = SpanOf(param_name.get(), overload_scope)
    });
  }
}

auto spp::lsp::resolution_index::RecordCompTimeValue(
  Ast const &name, ScopeManager const &sm, CompilerMetaData const &,
  Str value) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  auto const &use_scope = *sm.CurrentScope;
  if (value.empty() or not WantsScope(use_scope)) { return; }

  // Check that the constant is not a compiler-generated $Func
  // mock type.
  auto where = SpanOf(&name, use_scope);
  if (where.Generated) { return; }
  auto key = KeyOfSpan("value", where);
  if (not Current().SeenValues.insert(std::move(key)).second) { return; }

  // Give the compiler generated value to the entry that holds
  // its definition (resolve reserved slot).
  for (auto &entry : Current().Entries) {
    if (entry.Definition.Generated or entry.Definition.File != where.File) { continue; }
    if (entry.Definition.StartLine != where.StartLine or entry.Definition.StartCol != where.StartCol) { continue; }
    entry.Value = value;
  }

  // Store the value into the state too.
  Current().Values.EmplaceBack(CompTimeValue{
    .Where = std::move(where), .Name = name.ToString(), .Value = std::move(value)
  });
}

auto spp::lsp::resolution_index::RecordScopeOf(
  Ast const &node, ScopeManager const &sm, const bool whole_file) -> void {
  // If we are not bothered about this scope, then return early;
  // no work needs to be done here.
  auto const &scope = *sm.CurrentScope;
  if (not WantsScope(scope)) { return; }

  // A module's own scope covers its file; anything else covers
  // what the node that made it does [function/class]-enclosing.
  auto where = whole_file ? scope.GetErrorFormatter()->SpanOfWholeFile() : RegionOf(&node, scope);
  if (where.Generated) { return; }
  auto key = KeyOfSpan("scope", where);
  if (not Current().SeenScopes.insert(std::move(key)).second) { return; }

  // The variable symbols within the scope.
  auto names = Vec<Member>();
  auto seen = Set<Str>();
  for (const auto sym : scope.GetAllVarSymbols(true, false)) {
    if (sym == nullptr or not IsWritableVarName(*sym)) { continue; }
    auto name = sym->Name->ToString();
    if (not seen.insert(KeyOf("var", name)).second) { continue; }
    names.EmplaceBack(Member{
      .Name = std::move(name),
      .Kind = KindOf(*sym),
      .Type = sym->Type != nullptr ? sym->Type->ToString() : Str(),
      .Definition = sym->ScopeDefinedIn != nullptr ? SpanOf(sym->Name.get(), *sym->ScopeDefinedIn) : SourceSpan()
    });
  }

  // The type symbols within the scope.
  for (const auto sym : scope.GetAllTypeSymbols(true, false)) {
    if (sym == nullptr or not IsWritableTypeName(*sym, true)) { continue; }
    auto name = sym->Name->ToString();
    if (not seen.insert(KeyOf("type", name)).second) { continue; }
    names.EmplaceBack(Member{.Name = std::move(name), .Kind = "type", .Type = Str(), .Definition = {}});
  }

  // Store all the members against the span being tracked.
  if (names.IsEmpty()) { return; }
  Current().Scopes.EmplaceBack(
    NamesInScope{.Where = std::move(where), .Names = std::move(names)});
}

auto spp::lsp::resolution_index::GetAllEntries()
  -> Vec<ResolvedName> const& {
  // Getter for the entries, for json for the IDEA plugin.
  return Current().Entries;
}

auto spp::lsp::resolution_index::GetAllMembers()
  -> Vec<MemberList> const& {
  // Getter for the members, for json for the IDEA plugin.
  return Current().Members;
}

auto spp::lsp::resolution_index::GetAllComptimeValues()
  -> Vec<CompTimeValue> const& {
  // Getter for the comptime values, for json for the IDEA plugin.
  return Current().Values;
}

auto spp::lsp::resolution_index::GetAllSignatures()
  -> Vec<Signature> const& {
  // Getter for the signatures, for json for the IDEA plugin.
  return Current().Signatures;
}

auto spp::lsp::resolution_index::GetAllScopes()
  -> Vec<NamesInScope> const& {
  // Getter for the scopes, for json for the IDEA plugin.
  return Current().Scopes;
}

auto spp::lsp::resolution_index::Clear()
  -> void {
  Current().Entries.Clear();
  Current().Seen.clear();
  Current().Members.Clear();
  Current().SeenOwners.clear();
  Current().Signatures.Clear();
  Current().SeenCalls.clear();
  Current().Scopes.Clear();
  Current().SeenScopes.clear();
  Current().Values.Clear();
  Current().SeenValues.clear();
}
