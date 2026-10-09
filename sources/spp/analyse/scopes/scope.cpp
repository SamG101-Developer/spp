module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbol_table;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.utils.packs;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.class_prototype_ast;
import spp.asts.closure_expression_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.float_literal_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import spp.asts.module_prototype_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.compiler.module_tree;
import spp.lex.tokens;
import spp.utils.algorithms;
import spp.utils.error_formatter;
import spp.utils.interner;
import spp.utils.ptr;
import genex;
import numex.big_dec;
import numex.big_int;

namespace spp::analyse::scopes {
  namespace {
    /// [CHECKED]
    /// The parent module scope: the first ancestor of "scope" (itself included) with an "IdentifierAst"-wrapped scope
    /// name. One walk for both constnesses ("Scope::GetParentModule").
    template <typename S>
    auto ParentModuleOf(S *scope) -> S* {
      for (; scope != nullptr; scope = scope->Parent) {
        if (std::holds_alternative<ScopeIdentifierName>(scope->Name)) { return scope; }
      }
      return nullptr;
    }

    /// [CHECKED]
    /// The top level parent module scope: the ancestor of "scope" (itself included) whose parent is the global scope,
    /// the one with no parent ("Scope::GetTopLevelParentModule").
    template <typename S>
    auto TopLevelParentModuleOf(S *scope) -> S* {
      for (; scope != nullptr; scope = scope->Parent) {
        if (scope->Parent != nullptr and scope->Parent->Parent == nullptr) { return scope; }
      }
      return nullptr;
    }

    /// The symbol (a variable or a type) a scope's super scopes file under "name": each direct super scope asked
    /// exclusively, which searches its own in turn, so the search stays in the sup graph rather than escaping into the
    /// enclosing lexical scopes.
    template <typename Symbol, typename Name>
    auto FindInSupScopesByName(Scope const &scope, Name const *name) -> Symbol* {
      for (const auto sup_scope : scope.GetDirectSupScopes()) {
        auto *const sym = [&] {
          if constexpr (std::same_as<Symbol, VariableSymbol>) { return sup_scope->FindVarSymbol(name, true); }
          else { return sup_scope->FindTypeSymbol(name, true); }
        }();
        if (sym != nullptr) { return sym; }
      }
      return nullptr;
    }

    /// The symbol "scope"'s super scopes file for a parameter identity, searched as a name is
    /// ("FindInSupScopesByName"): each direct super scope, then its own.
    template <typename Symbol, typename ByParam>
    auto FindInSupScopesByParam(Scope const &scope, const std::uint64_t id, ByParam const &by_param) -> Symbol* {
      for (const auto sup_scope : scope.GetSupScopes()) {
        if (auto *const found = by_param(*sup_scope, id); found != nullptr) { return found; }
      }
      return nullptr;
    }

    /// Whether "id" can be stamped on a written type as the identity it was written with ("StampTypeId"): it reads
    /// the same wherever the stamp is read - no "Self" (parameter 0, read as the reading scope's enclosing type) and
    /// nothing unresolved. Parameters are allowed: a stamp is read through the bindings where it is read.
    auto IsStampableTypeId(const TypeId id) -> bool {
      return id != nullptr and not id->HasSelf and not id->HasUnresolved;
    }

    /// The decimal places "value" needs to print exactly, or none when its decimal does not terminate: its denominator
    /// (in lowest terms) has a prime factor other than 2 and 5.
    auto ExactDecimalPlaces(numex::BigDec const &value) -> std::optional<std::uint64_t> {
      auto den = value.GetDenominator();
      auto twos = static_cast<std::uint64_t>(0);
      auto fives = static_cast<std::uint64_t>(0);
      while (den % numex::BigInt(2) == numex::BigInt(0)) {
        den /= numex::BigInt(2);
        ++twos;
      }
      while (den % numex::BigInt(5) == numex::BigInt(0)) {
        den /= numex::BigInt(5);
        ++fives;
      }
      return den == numex::BigInt(1) ? std::optional(std::max(twos, fives)) : std::nullopt;
    }

    /// The binding a scope gives a generic parameter, by the parameter's identity
    /// ("IndividualSymbolTable::FindByParam"): the nearest scope filing one, where a scope asks its own table, then
    /// (inside a "sup" block, whose class's parameters are bound by the block's "Self" instantiation) that
    /// instantiation's; then the super scopes. A scope that files only the parameter itself (or a copy of it) leaves it
    /// unbound there, and the search goes on for a binding further out. Nothing binding it leaves the parameter itself.
    template <typename Symbol, typename ByParam>
    auto BindingOf(
      Scope const &from, Symbol &param, const std::uint64_t id,
      ByParam const &find_by_param) -> Symbol* {
      // The binding function returns a symbol if that symbol's
      // binding parameter's id is the fixed id.
      using generate::common_types_precompiled::SELF_TYPE;
      const auto binding = [id](Symbol *found) -> Symbol* {
        return found != nullptr and found->BindsParamId == id ? found : nullptr;
      };

      // Move upwards through the ancestors, starting from the
      // "from" scope. For each symbol in the scope (discovered
      // by id), if it is bound to the inputted id, then return
      // it.
      for (auto scope = &from; scope != nullptr; scope = scope->Parent) {
        if (const auto found = binding(find_by_param(*scope, id)); found != nullptr) { return found; }

        // Do a search in the class of the "Self" scope.
        if (const auto self_sym = scope->InternalTable.TypeTable.Find(
            SELF_TYPE->ToUnchecked<TypeIdentifierAst>());
          self_sym != nullptr
          and self_sym->IsSelf()
          and self_sym->LinkedScope != nullptr
          and self_sym->LinkedScope != scope) {
          if (const auto found = binding(find_by_param(*self_sym->LinkedScope, id)); found != nullptr) { return found; }
        }
      }

      // Finally, search in the sup-scopes because of inheritance
      // discovery.
      if (const auto found = binding(FindInSupScopesByParam<Symbol>(
        from, id, find_by_param)); found != nullptr) {
        return found;
      }

      return &param;
    }

    /// The 2 memoisation generations, starting at "1" because
    /// "0" means "never computed" rather than "computed before
    /// anything moved".
    std::uint64_t _ScopeLinkageGeneration = 1;
    std::uint64_t _TypeStructureGeneration = 1;
  }
}

/// [CHECKED]
auto spp::analyse::scopes::ScopeLinkageGeneration() -> std::uint64_t {
  // Get the current scope linkage generation based on scope
  // movement in trees. Used by the symbols to cache the fully
  // qualified name on a type symbol.
  return _ScopeLinkageGeneration;
}

/// [CHECKED]
auto spp::analyse::scopes::BumpScopeLinkageGeneration() -> void {
  // A scope moving in the tree changes the method set, so
  // bump the scope linkage generation, forcing caches to be
  // cleared; this is the coarser of the two cache bumps.
  ++_ScopeLinkageGeneration;
  ++_TypeStructureGeneration;
}

/// [CHECKED]
auto spp::analyse::scopes::TypeStructureGeneration() -> std::uint64_t {
  // Get the current type structure generation based on sup
  // scope attachment and similar scope-reachability in the
  // [scope manager]'s tree.
  return _TypeStructureGeneration;
}

/// [CHECKED]
auto spp::analyse::scopes::BumpTypeStructureGeneration() -> void {
  // A change in the sup-scope or overall scope-reaching
  // capability of the tree requires this generation to be bumped,
  // because new fields are accessible or no longer accessible.
  ++_TypeStructureGeneration;
}

SPP_MOD_BEGIN
namespace {
  using spp::Pair;
  using spp::Vec;
  using spp::analyse::scopes::Scope;
  using spp::analyse::scopes::TypeId;
  using spp::analyse::scopes::TypeSymbol;
  using spp::asts::IdentifierAst;
  using spp::asts::TypeAst;
  using spp::asts::TypeIdentifierAst;

  /// Given a scope and a fully qualified type, move through the namespace parts of the type into the next namespace
  /// scopes. Returns the innermost scope and the unqualified type (for that scope). Only reached
  /// ("Scope::FindTypeSymbol") for a type that is not a plain identifier, so there is always a namespace or nested-type
  /// part to shift through.
  auto ShiftForNamespacedType(
    Scope const &scope, TypeAst const &fq_type)
    -> Pair<Scope const*, TypeIdentifierAst const*> {
    // Get the namespace and type parts, to get the scopes. Use
    // the appending form, so a type of any depth only uses one
    // allocation per list rather than one per level of the chain.
    auto ns_parts = Vec<IdentifierAst const*>();
    auto type_parts = Vec<TypeIdentifierAst const*>();
    fq_type.NsPartsInto(ns_parts);
    fq_type.TypePartsInto(type_parts);
    // The namespace parts first ("FindNsScope"): a part naming no namespace names no type.
    auto shifted_scope = scope.FindNsScope(ns_parts);
    if (shifted_scope == nullptr) { return {nullptr, type_parts.Back()}; }

    // Iterate through the type parts (except the final one) next.
    for (auto const *type_part : type_parts | genex::views::drop_last(1)) {
      const auto sym = shifted_scope->FindTypeSymbol(type_part);
      if (sym == nullptr or sym->IsGn()) { break; }
      shifted_scope = sym->LinkedScope;
    }

    // Return the type scope, and the final type part.
    auto const *final = type_parts.Back();
    return {shifted_scope, final};
  }

  /// The binary operator a comp identity's operation spells ("+", "<<"), as the token a written one carries.
  auto CompOperatorToken(
    const spp::StrView op)
    -> std::optional<spp::lex::SppTokenType> {
    using spp::lex::SppTokenType;
    for (const auto tok : {
           SppTokenType::TK_ADD, SppTokenType::TK_SUB, SppTokenType::TK_MUL, SppTokenType::TK_DIV, SppTokenType::TK_REM,
           SppTokenType::TK_BIT_IOR, SppTokenType::TK_BIT_AND, SppTokenType::TK_BIT_XOR, SppTokenType::TK_BIT_SHL,
           SppTokenType::TK_BIT_SHR, SppTokenType::TK_EQ, SppTokenType::TK_NE, SppTokenType::TK_LT, SppTokenType::TK_LE,
           SppTokenType::TK_GT, SppTokenType::TK_GE
         }) {
      if (spp::lex::TokToString(tok) == op) { return tok; }
    }
    return std::nullopt;
  }

  /// The template a name with arguments instantiates, found
  /// from "scope", and the identity its arguments give that
  /// instance there ("Scope::InstanceIdOf"). The identity is
  /// only made when the template has instances to match.
  auto NamedInstance(
    Scope const &scope, TypeIdentifierAst const &name) -> Pair<TypeSymbol*, TypeId> {
    // Get the template the name's head names (the name without
    // its arguments); with no instances registered for it,
    // return it with no type id.
    const auto tmpl = scope.FindHeadSymbol(name);
    if (tmpl == nullptr or tmpl->Instances.empty()) { return {tmpl, nullptr}; }

    // Otherwise, return the template and the identity the
    // name's generic args give its instance here.
    return {tmpl, scope.InstanceIdOf(*tmpl, name.GnArgGroup->GetAllArgs(), tmpl->GnParams())};
  }
}

namespace {
  /// A type key, and when the type is a variant, the keys of its members, so a variant member of a variant is
  /// flattened into it.
  struct TypeKeyParts {
    TypeKey Key;
    Vec<TypeKey> Members;
  };

  /// The aliases being keyed through their targets: a target that names its own alias (directly or down a chain) is keyed
  /// as the alias inside itself rather than expanded without end.
  thread_local auto keying_aliases = std::vector<spp::analyse::scopes::TypeSymbol const*>();

  /// "alias" marked as being keyed through its target ("keying_aliases") for as long as this lives, unless it already
  /// is: only the outermost keying of an alias ("Active") may expand it.
  class AliasKeying {
  public:
    explicit AliasKeying(TypeSymbol const &alias) : _Active(not genex::contains(keying_aliases, &alias)) {
      if (_Active) { keying_aliases.push_back(&alias); }
    }

    ~AliasKeying() { if (_Active) { keying_aliases.pop_back(); } }

    AliasKeying(AliasKeying const &) = delete;
    auto operator=(AliasKeying const &) -> AliasKeying& = delete;

    SPP_ATTR_NODISCARD auto Active() const -> bool { return _Active; }

  private:
    bool _Active;
  };
}

namespace spp::analyse::scopes {
  namespace {
    /// The key of a written type ("t"), or of a type already resolved to "given" (a "TypeRef"'s symbol), read in "scope";
    /// and when it is a variant, the keys of its members, so a variant member of a variant is flattened into it.
    SPP_ATTR_HOT auto RawTypeKey(TypeAst const *t, TypeSymbol const *given, Scope const &scope) -> TypeKeyParts;

    /// Continue "out" with a key already made: a variant's members are carried with it, so a variant it is a member of
    /// flattens it.
    SPP_ATTR_HOT auto AppendKeyId(
      TypeKeyParts &out, const TypeId id) -> void {
      using namespace spp::analyse::scopes;
      out.Key.PushSpliced(*id);
      auto const &head = HeadOf(id);
      if (head.Kind == TypeKey::Tag::Variant and head.Conv == 0) {
        for (const auto m : head.Members) { out.Members.EmplaceBack(TypeKey(*m)); }
      }
    }

    /// The alias an identity names whole, when it is one: a name stamped as an alias ("NameTypeIdOf") is keyed as the
    /// alias's target when it is read, as the alias itself would be.
    auto AliasHeadOf(
      const spp::analyse::scopes::TypeId id) -> spp::analyse::scopes::TypeSymbol const* {
      using spp::analyse::scopes::HeadOf;
      using spp::analyse::scopes::TypeKey;
      if (id == nullptr or HeadOf(id).Kind != TypeKey::Tag::Symbol or HeadOf(id).Conv != 0) { return nullptr; }
      auto const *const sym = HeadOf(id).Symbol();
      return sym != nullptr and sym->Alias != nullptr ? sym : nullptr;
    }

    /// An alias keyed as its target ("TypeSymbol::AliasTargetId"). One already being keyed through (a target naming its
    /// own alias) is keyed as itself; one with no target as "unresolved"'s spelling, where given, else as itself.
    auto KeyAliasTarget(
      TypeKeyParts &out, TypeSymbol const &alias, TypeAst const *unresolved) -> void {
      using spp::analyse::scopes::TypeKey;
      const auto keying = AliasKeying(alias);
      if (not keying.Active()) {
        out.Key.PushSymbol(&alias);
        return;
      }
      const auto target = alias.AliasTargetId();
      if (target == nullptr) {
        if (unresolved != nullptr) { out.Key.PushText(TypeKey::Tag::Unresolved, unresolved->ToString()); }
        else { out.Key.PushSymbol(&alias); }
        return;
      }

      // A target recorded as another alias (stamped before that alias's own statement resolved it) is that alias's target.
      if (auto const *const next = AliasHeadOf(target); next != nullptr) {
        KeyAliasTarget(out, *next, nullptr);
        return;
      }
      AppendKeyId(out, target);
    }

    /// What keying a type by its stamp came to: keyed; the symbol its stamp names here, for the caller to key; no
    /// stamp, or one naming nothing made yet, so it is looked up by name; or an open instantiation of an alias, which
    /// is not looked up by name (its spelling names the alias's parameters where it was written, not here) but keyed
    /// from its written arguments ("KeyUnmade").
    enum class StampKeying { Keyed, Symbol, ByName, OpenAlias };

    /// "KeyStamped"'s outcome, with the symbol for "StampKeying::Symbol".
    struct StampedKey {
      StampKeying How;
      TypeSymbol const *Sym = nullptr;
    };

    /// A type keyed by the identity stamped where it was written ("TypeAst::StampedTypeId"), before any lookup by name.
    /// An open class instantiation is that identity read here ("ReadIn"); an instantiation of an alias is the alias's
    /// target. An open instantiation of an alias names its arguments where it was written, which its own target is
    /// recorded in terms of: that target read here, else "StampKeying::OpenAlias". Anything else is the symbol its stamp
    /// names here ("Scope::FindBoundTypeSymbolById"), as "Scope::FindTypeSymbol" would answer it.
    auto KeyStamped(
      TypeKeyParts &out, Scope const &scope, TypeAst const &t) -> StampedKey {
      const auto written = t.LastTypePart()->StampedTypeId();
      if (written == nullptr) { return {.How = StampKeying::ByName}; }
      if (auto const *const alias = AliasHeadOf(written); alias != nullptr) {
        KeyAliasTarget(out, *alias, nullptr);
        return {.How = StampKeying::Keyed};
      }

      // Read here (an operation over a parameter bound here is rewritten and folded by identity,
      // "scopes::SubstituteCompId"); one whose identity does not read so is keyed from its written arguments.
      auto const *const named = HeadOf(written).Kind == TypeKey::Tag::Inst
        ? scope.FindTypeSymbolById(written)
        : nullptr;
      const auto read = named != nullptr and named->Kind == TypeKind::Cls and named->Alias == nullptr
        and not named->IsConcrete
        ? scope.ReadIn(written)
        : nullptr;
      if (read != nullptr) {
        AppendKeyId(out, read);
        return {.How = StampKeying::Keyed};
      }
      // Not an alias: the symbol its stamp names here - a parameter's binding, a class, a made instance. One naming an
      // instance not made yet finds nothing, so it is looked up by name, then keyed from its arguments ("KeyUnmade").
      if (named == nullptr or named->Alias == nullptr) {
        auto const *const sym = scope.FindBoundTypeSymbolById(written);
        return sym != nullptr
          ? StampedKey{.How = StampKeying::Symbol, .Sym = sym}
          : StampedKey{.How = StampKeying::ByName};
      }
      if (not DoesTypeIdNameAnyGnParams(written)) {
        KeyAliasTarget(out, *named, nullptr);
        return {.How = StampKeying::Keyed};
      }

      // An open instantiation of an alias stands for its target ("TypeSymbol::AliasTargetId"), recorded in the same
      // terms as its arguments: that target read here.
      if (const auto target = named->AliasTargetId(); target != nullptr and HeadOf(target).IsInstance()) {
        if (const auto target_read = scope.ReadIn(target); target_read != nullptr) {
          AppendKeyId(out, target_read);
          return {.How = StampKeying::Keyed};
        }
      }
      return {.How = StampKeying::OpenAlias};
    }

    /// A name nothing resolves (an instantiation not made, reached by spelling) is keyed by what it names: its arguments
    /// recorded as the instantiation would record them ("RecordedArgsFor"), then an alias as its target with them
    /// substituted, a class as its template and them. True when "t" is keyed.
    auto KeyUnmade(
      TypeKeyParts &out, Scope const &scope, TypeAst const &t) -> bool {
      auto const &written_args = *t.LastTypePart()->GnArgGroup;
      auto const *head = scope.FindHeadSymbol(t);

      // A "use" passes its arguments straight to what it names, so it is followed to that first: a class, or a "type"
      // alias ("use ..::SizedIntegerSigned" names the alias "SizedIntegerSigned[cmp w]").
      auto const *const named = head != nullptr ? head->UseTarget() : nullptr;
      if (named != nullptr and named->Alias != nullptr and not named->Alias->IsFromUseStmt) {
        // A "type" alias: its target ("TypeSymbol::AliasTargetId"), its parameters bound to the arguments.
        const auto keying = AliasKeying(*named);
        auto const &alias = *named->Alias;
        if (keying.Active() and alias.Params != nullptr and alias.WrittenIn != nullptr) {
          const auto recorded = spp::analyse::utils::type_resolution::RecordedArgsFor(
            written_args, *alias.Params, scope, *alias.WrittenIn);
          const auto recorded_args = recorded | genex::views::ptr | genex::to<spp::Vec>();
          if (not recorded.IsEmpty()) {
                      if (const auto id = named->AliasTargetId(scope.ArgsIdOf(recorded_args, alias.Params.get())); id != nullptr) {
              AppendKeyId(out, id);
              return true;
            }
          }
        }
      }

      // A class, as its template and its arguments. A variadic class (a variant, a tuple) has no defaults to record:
      // its arguments are keyed as written.
      if (named != nullptr and named != head and named->Alias == nullptr) { head = named; }
      if (head == nullptr or head->Alias != nullptr or head->Type == nullptr or head->InstanceOf != nullptr) {
        return false;
      }
      if (head->Type->GnParamGroup->GetVariadicParam() != nullptr) {
        AppendKeyId(out, scope.InstanceIdOf(*head, written_args.GetAllArgs()));
        return true;
      }
      const auto recorded = spp::analyse::utils::type_resolution::RecordedArgsFor(
        written_args, *head->Type->GnParamGroup, scope, *head->LinkedScope);
      if (recorded.IsEmpty()) { return false; }
      AppendKeyId(out, scope.InstanceIdOf(*head, recorded | genex::views::ptr | genex::to<spp::Vec>()));
      return true;
    }

    /// A binding ("T" bound to "Str", written "t" where it was written), keyed as it holds its value. What it is bound
    /// to, for "RawTypeKey" to key; null when the binding is keyed whole here.
    auto KeyBinding(
      TypeKeyParts &out, TypeSymbol const &sym, TypeAst const *t) -> TypeSymbol const* {
      using namespace spp::analyse::scopes;
      using spp::asts::ConventionTag;
      using Tag = TypeKey::Tag;

      // Held as the binding holds it ("T" bound to "&mut Str" is "&mut Str"), as "TypeRef::Of" reads it, unless written
      // with a convention of its own.
      const auto held = sym.HeldConvention();
      if (held != ConventionTag::MOV and (t == nullptr or t->GetConvention() == nullptr)) {
        out.Key.Push(Tag::Conv, static_cast<std::uint64_t>(held));
      }

      auto const *const bound = sym.AsBound();
      if (bound == &sym) {
        // Bound to "Self" ("Rhs=Self"): "Self", as reading the parameter means here.
        if (sym.BoundTypeVal != nullptr and sym.BoundTypeVal->IsSelfType()) {
          out.Key.Push(Tag::TypeParam, 0);
          return nullptr;
        }
        out.Key.Push(Tag::TypeBound, static_cast<std::uint64_t>(sym.BindsParamId));
        if (sym.BoundTypeVal != nullptr) { out.Key.PushText(Tag::TypeBound, sym.BoundTypeVal->ToString()); }
        else { out.Key.PushText(Tag::TypeBound, sym.Name->ToView()); }
        return nullptr;
      }
      return bound;
    }

    /// The value each opaque comp identity (an "Opaque" node, its spelling) stands for, recorded as it is keyed
    /// ("RawCompKey"): no identity names it, so this is the only way back to it ("Scope::CompAstOf"). Keys are
    /// interned for the process, and the values are clones owned here, their names recording what they mean where
    /// they were first keyed.
    auto OpaqueValues() -> spp::Map<spp::analyse::scopes::CompId, spp::Shared<spp::asts::ExpressionAst>>& {
      static auto values = spp::Map<spp::analyse::scopes::CompId, spp::Shared<spp::asts::ExpressionAst>>();
      return values;
    }

    /// What "Self" is where "scope" is, read there: the nearest "sup" block's type as written ("Box[T]", over the block's
    /// own parameters, which its instantiation binds), else the nearest class over its own parameters; and the scope
    /// that says so (where a "Self::n" written in the block is declared, before the block is attached). A method's own
    /// "sup $M ext FunXxx" block (stage 1 lowers each method into one) is passed over: its "Self" is the mock, not the
    /// method's owner.
    auto SelfOwnerIn(
      spp::analyse::scopes::Scope const &scope)
      -> spp::Pair<spp::analyse::scopes::TypeId, spp::analyse::scopes::Scope const*> {
      for (auto const *s = &scope; s != nullptr; s = s->Parent) {
        if (s->AstNode == nullptr) { continue; }
        if (s->AstNode->To<spp::asts::ClassPrototypeAst>() != nullptr and s->LinkedTypeSymbol != nullptr) {
          return {s->TypeIdOf(*s->LinkedTypeSymbol->GnSelfName()), s};
        }
        if (const auto name = spp::asts::AstNameOrNull(s->AstNode); name != nullptr and not name->
          IsCompilerGeneratedType()) {
          return {s->TypeIdOf(*name), s};
        }
      }
      return {nullptr, nullptr};
    }

    /// A variant, by the set of the members its "Variants" argument lists, read in "read_in": flattened, deduplicated,
    /// and in one order, whatever order they were written or substituted in.
    auto VariantKey(
      TypeSymbol const &tmpl, Vec<TypeAst const*> const &members, Scope const &read_in) -> TypeKeyParts {
      using namespace spp::analyse::scopes;
      auto out = TypeKeyParts();
      for (auto const *member : members) {
        auto parts = RawTypeKey(member, nullptr, read_in);
        auto flat = parts.Members.IsEmpty() ? spp::Vec<TypeKey>{std::move(parts.Key)} : std::move(parts.Members);
        for (auto &m : flat) {
          if (not genex::contains(out.Members, m)) { out.Members.EmplaceBack(std::move(m)); }
        }
      }
      out.Members |= genex::actions::sort([](auto const &a, auto const &b) { return StableKeyLess(a, b); });
      out.Key.Push(TypeKey::Tag::Variant);
      out.Key.PushSymbol(&tmpl);
      for (auto const &m : out.Members) { out.Key.PushTypePart(InternTypeKey(TypeKey(m))); }
      return out;
    }

    SPP_ATTR_HOT auto RawTypeKey(
      TypeAst const *t, TypeSymbol const *given, Scope const &scope) -> TypeKeyParts {
      auto out = TypeKeyParts();

      if (t != nullptr) {
        // If a convention is present, push it into the key. Only
        // used for & and &mut.
        if (const auto conv = t->GetConvention(); conv != nullptr) {
          out.Key.Push(TypeKey::Tag::Conv, static_cast<std::uint64_t>(conv->Tag()));
        }

        // If the type is the "Self" type, push parameter 0 into
        // the key, and return because there will not be any more.
        if (t->IsSelfType()) {
          out.Key.Push(TypeKey::Tag::TypeParam, 0);
          return out;
        }
      }

      const auto stamped = t != nullptr ? KeyStamped(out, scope, *t) : StampedKey{.How = StampKeying::ByName};
      if (stamped.How == StampKeying::Keyed) { return out; }

      // Anything else is what it resolves to here: the symbol given, else the one its stamp names, else found by name
      // (an unstamped type, or one naming an instance not made yet). A parameter's binding answers what the binding
      // links, which for one still waiting on an alias's target is only that target's template.
      auto const *sym = given != nullptr ? given : stamped.Sym;
      if (sym == nullptr and t != nullptr and stamped.How == StampKeying::ByName) { sym = scope.FindTypeSymbol(t); }
      if (sym == nullptr and t != nullptr and not t->LastTypePart()->GnArgGroup->Args.IsEmpty()
        and KeyUnmade(out, scope, *t)) { return out; }
      if (sym == nullptr) {
        out.Key.PushText(TypeKey::Tag::Unresolved, t->ToString());
        return out;
      }

      // A type resolved to "sym": an alias as its target, a binding as what it is bound to, a parameter by its
      // identity, "Self" as parameter 0, a made instantiation as the identity it was filed under ("Scope::InstanceIdOf"),
      // anything else by its address.
      if (sym->Alias != nullptr and sym->Alias->Resolved != nullptr) {
        KeyAliasTarget(out, *sym, sym->Alias->Resolved.get());
        return out;
      }
      if (sym->Kind == TypeKind::GnTypeArg) {
        sym = KeyBinding(out, *sym, t);
        if (sym == nullptr) { return out; }
      }
      if (sym->Kind == TypeKind::GnTypeParam) {
        out.Key.Push(TypeKey::Tag::TypeParam, static_cast<std::uint64_t>(sym->OwnParamId));
      }
      else if (sym->Kind == TypeKind::Self) { out.Key.Push(TypeKey::Tag::TypeParam, 0); }
      else if (sym->Kind == TypeKind::Cls and sym->InstanceOf != nullptr) { AppendKeyId(out, sym->Id); }
      else { out.Key.PushSymbol(sym); }
      return out;
    }

    /// A value no identity names, keyed as its spelling ("x.f()", or a name that is no comp generic), with the value
    /// recorded under it, its names recording what they mean here: the only way back to it ("Scope::CompAstOf").
    auto RecordedOpaque(ExpressionAst const &expr, Scope const &scope) -> CompKey {
      const auto id = InternCompKey(CompKey::OfOpaque(expr.ToString()));
      if (not OpaqueValues().contains(id)) {
        auto recorded = AstCloneShared(&expr);
        utils::type_resolution::StampCompParts(*recorded, scope);
        OpaqueValues().emplace(id, std::move(recorded));
      }
      return *id;
    }

    /// [CHECKED]
    /// An expression's identity, as a comp key: folded as it is
    /// built, as a type's key is normalised as it is built
    /// ("VariantKey"), so "1 + 2" keys as "3". This is used to key
    /// a comp generic.
    auto RawCompKey(ExpressionAst const &expr, Scope const &scope) -> CompKey {
      using namespace spp::asts;

      // A literal is its own value, however it is spelled:
      // "0x10_uz" and "16_uz" are one value, as are "1.5_f64"
      // and "1.50_f64" (a float as its exact rational).
      if (const auto lit = expr.To<FloatLiteralAst>(); lit != nullptr) {
        return CompKey::OfFloat(lit->BigVal(), lit->Type);
      }

      if (const auto lit = expr.To<IntegerLiteralAst>(); lit != nullptr) {
        return CompKey::OfInt(lit->BigVal(), lit->Type);
      }

      if (const auto lit = expr.To<BooleanLiteralAst>(); lit != nullptr) {
        return CompKey::OfBool(lit->CppVal());
      }

      // Create a pack from a tuple of elements, recursively
      // interning the inner elements.
      if (const auto tup = expr.To<TupleLiteralAst>(); tup != nullptr) {
        auto elems = std::vector<CompId>();
        for (auto const &elem : tup->Elems) { elems.push_back(InternCompKey(RawCompKey(*elem, scope))); }
        return CompKey::OfPack(std::move(elems));
      }

      // For a parenthesis expression, extract the inner
      // expression and generate the node for it.
      if (const auto paren = expr.To<ParenthesisedExpressionAst>(); paren != nullptr) {
        return RawCompKey(*paren->Expr, scope);
      }

      // A comp generic is the parameter at the end of its chain
      // of bindings to other generics. A binding to a value is
      // that value: its identity, keyed where it was bound
      // ("BoundCompId"), else its value keyed here.
      if (const auto id = expr.To<IdentifierAst>(); id != nullptr) {
        auto const *var = scope.FindVarSymbol(id);
        if (var == nullptr or not var->IsGn()) { return RecordedOpaque(expr, scope); }
        var = var->AsBound();
        if (var->BoundCompId != nullptr) { return *var->BoundCompId; }
        if (const auto value = var->BoundCompVal(); value != nullptr and value->To<IdentifierAst>() == nullptr) {
          return RawCompKey(*value, scope);
        }
        return CompKey::OfParam(var->ParamId());
      }

      // Create a node for a binary expression, creating nodes
      // for the two operands and keeping the token, folded into
      // one value when both operands are values.
      if (const auto bin = expr.To<BinaryExpressionAst>(); bin != nullptr) {
        if (const auto [lhs, rhs] = bin->Operands(); lhs != nullptr and rhs != nullptr) {
          const auto op = spp::lex::TokToString(bin->TokOp->TokenType);
          auto lhs_key = RawCompKey(*lhs, scope);
          auto rhs_key = RawCompKey(*rhs, scope);
          if (lhs_key.Kind == CompKey::Part::Value and rhs_key.Kind == CompKey::Part::Value) {
            if (auto folded = FoldCompValues(op, lhs_key, rhs_key); folded.has_value()) { return std::move(*folded); }
          }
          return CompKey::OfOp(op, InternCompKey(std::move(lhs_key)), InternCompKey(std::move(rhs_key)));
        }
      }

      // A constant named through a type ("Self::mo_seq_cst",
      // "T::SIZE").
      if (const auto pf = expr.To<PostfixExpressionAst>(); pf != nullptr) {
        const auto lhs_type = pf->Lhs->To<TypeAst>();
        const auto member = pf->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>();
        // A "Self::n" is also read where its block declares "n", which it does before the block is attached.
        const auto [owner, self_block] = lhs_type == nullptr or member == nullptr
          ? Pair<TypeId, Scope const*>{nullptr, nullptr}
          : lhs_type->IsSelfType()
          ? SelfOwnerIn(scope)
          : Pair<TypeId, Scope const*>{scope.TypeIdOf(*lhs_type), nullptr};

        if (owner != nullptr) {
          const auto read = IsClosedTypeId(owner)
            ? utils::comp_generics::FindCompMemberId(owner, member->Name->ToView(), self_block)
            : nullptr;

          if (read != nullptr) { return *read; }
          return CompKey::OfMember(TypeIdWord(owner), member->Name->ToView());
        }
      }

      // Anything else is its spelling ("x.f()").
      return RecordedOpaque(expr, scope);
    }
  }
}

Scope::Scope(
  ScopeName name, Scope *parent, Ast *ast,
  ErrorFormatter *error_formatter) :
  Name(std::move(name)),
  Parent(parent),
  AstNode(ast),
  LinkedTypeSymbol(nullptr),
  LinkedNamespaceSymbol(nullptr),
  NonGnScope(this),
  _ErrorFormatter(error_formatter) {
}

Scope::Scope(Scope const &other) :
  Name(other.Name),
  Parent(other.Parent),
  AstNode(other.AstNode),
  LinkedTypeSymbol(other.LinkedTypeSymbol),
  LinkedNamespaceSymbol(other.LinkedNamespaceSymbol),
  NonGnScope(other.NonGnScope),
  Deferred(other.Deferred),
  _ErrorFormatter(nullptr) {
  InternalTable.ShallowCopyFrom(other.InternalTable);

  // Copy the children recursively.
  for (auto const &child_scope : other.Children) {
    auto child_copy = MakeUnique<Scope>(*child_scope);
    child_copy->Parent = this;
    Children.EmplaceBack(std::move(child_copy));
  }
}

Scope::~Scope() = default;

/// [CHECKED]
auto Scope::NewGlobal(Module const &mod)
  -> Shared<Scope> {
  // Create a new global scope (no parent or ast for the global
  // scope). This is a master scope for all scope managers.
  auto scope_name = ScopeBlockName::FromParts(
    "__global__", {}, 0);
  auto glob_scope = MakeShared<Scope>(
    std::move(scope_name), nullptr, nullptr, mod.Formatter.get());

  // Inject the "_global" namespace symbol into this scope to make
  // lookups orthogonal.
  auto glob_ns_sym_name = MakeShared<IdentifierAst>(0uz, "_global");
  auto glob_ns_sym = MakeShared<NamespaceSymbol>(
    std::move(glob_ns_sym_name), glob_scope.get());
  glob_scope->LinkedNamespaceSymbol = std::move(glob_ns_sym);

  // Return the global scope.
  return glob_scope;
}

/// [CHECKED]
auto Scope::GetErrorFormatter() const -> ErrorFormatter* {
  // Return this scope's error formatter, or the parent's if
  // it doesn't exist.
  const auto this_formatter = _ErrorFormatter;
  return this_formatter != nullptr
    ? this_formatter
    : Parent->GetErrorFormatter();
}

/// [CHECKED]
auto Scope::IsFromPrelude(
  Ast const &ast) const -> bool {
  // The prelude is appended behind the author's code, so a
  // position past what they wrote is one of its nodes.
  const auto formatter = GetErrorFormatter();
  return formatter != nullptr and formatter->IsPastUserSource(ast.PosStart());
}

/// [CHECKED]
auto Scope::GetGns() const -> Vec<Unique<GenericArgumentAst>> {
  // Create the symbols list.
  const auto scopes = GetAncestors();
  auto syms = Vec<Unique<GenericArgumentAst>>();
  auto type_names = Vec<Shared<TypeIdentifierAst>>();
  auto comp_names = Vec<Shared<IdentifierAst>>();

  // Check each ancestor scope, accumulating generic type
  // and comp symbols.
  for (const auto scope : scopes) {
    auto all_type_syms = scope->GetAllTypeSymbols(true)
      | genex::views::filter([](auto const &sym) { return sym->IsGn(); })
      | genex::to<Vec>();

    auto all_comp_syms = scope->GetAllVarSymbols(true)
      | genex::views::filter([](auto const &sym) { return sym->IsGn(); })
      | genex::to<Vec>();

    // Bindings only, of both kinds: an unbound parameter
    // is no generic argument of this scope.
    for (auto const &t : all_type_syms) {
      if (t->Kind == TypeKind::GnTypeParam) { continue; }
      if (t->LinkedSymbol() == t and t->BoundTypeVal == nullptr) { continue; }
      if (genex::contains(type_names, *t->Name, genex::meta::deref)) { continue; }
      syms.EmplaceBack(GenericArgumentAst::FromSymbol(*t));
      type_names.EmplaceBack(t->Name);
    }

    for (auto const &v : all_comp_syms) {
      if (genex::contains(comp_names, *v->Name, genex::meta::deref)) { continue; }
      if (v->Kind == VariableKind::GnCompParam or v->BoundCompVal() == nullptr) { continue; }
      syms.EmplaceBack(GenericArgumentAst::FromSymbol(*v));
      comp_names.EmplaceBack(v->Name);
    }
  }

  // Return the list of generic symbols.
  return syms;
}

/// [CHECKED]
auto Scope::AddVarSymbol(
  Shared<VariableSymbol> const &sym)
  -> void {
  // Add a variable symbol to the corresponding symbol table.
  InternalTable.VarTable.Add(sym->Name.get(), sym);
}

/// [CHECKED]
auto Scope::AddVarSymbolCheckConflict(
  Shared<VariableSymbol> const &sym)
  -> void {
  // Cannot allow for duplicate comp definitions. A variable
  // declared here may shadow an import.
  const auto existing_sym = FindVarSymbol(sym->Name.get(), false);
  if (existing_sym != nullptr) {
    // Detect function and "use" (different target) variable
    // symbol declaration.
    const auto is_functional = existing_sym->Kind == VariableKind::FnMock;
    const auto is_shadowed_import = existing_sym->IsImport() and not sym->IsImport();

    // The prelude is appended behind the author's code, so a
    // name it imports is reported through the author's side
    // alone.
    const auto existing_from_prelude = IsFromPrelude(*existing_sym->Name);
    if (existing_from_prelude != IsFromPrelude(*sym->Name)) {
      auto const &mine = existing_from_prelude ? *sym : *existing_sym;
      RaiseIf<errors::SppIdentifierDuplicateError>(
        not is_functional and not is_shadowed_import,
        {mine.ScopeDefinedIn ? mine.ScopeDefinedIn : this},
        ERR_ARGS(*mine.Name, "comptime variable identifier", mine.IsImport()));
    }

    // Standard raise for non-functional symbols.
    RaiseIf<errors::SppIdentifierDuplicateError>(
      not is_functional and not is_shadowed_import,
      {
        existing_sym->ScopeDefinedIn ? existing_sym->ScopeDefinedIn : this,
        sym->ScopeDefinedIn ? sym->ScopeDefinedIn : this
      },
      ERR_ARGS(*existing_sym->Name, *sym->Name, "comptime variable identifier"));
  }

  // Add a variable symbol to the corresponding symbol table.
  InternalTable.VarTable.Add(sym->Name.get(), sym);
}

/// [CHECKED]
auto Scope::AddTypeSymbol(
  Shared<TypeSymbol> const &sym)
  -> void {
  // Add a type symbol to the corresponding symbol table.
  InternalTable.TypeTable.Add(sym->Name.get(), sym);
}

/// [CHECKED]
auto Scope::AddTypeSymbolCheckConflict(
  Shared<TypeSymbol> const &sym)
  -> void {
  // Cannot allow for duplicate type definitions. Unlike a
  // comptime variable, a type declared here may not shadow
  // an import either, the prelude's included.
  const auto existing_sym = FindTypeSymbol(sym->Name.get(), false);
  if (existing_sym != nullptr) {
    // A function's mock class ("$F") is made once per block
    // that declares the function, so another block's is met
    // here; it is the same mock, not a redefinition.
    const auto is_functional = existing_sym->IsMock();

    // The prelude is appended behind the author's code, so a
    // name it imports is reported through the author's side
    // alone.
    const auto existing_from_prelude = IsFromPrelude(*existing_sym->Name);
    if (existing_from_prelude != IsFromPrelude(*sym->Name)) {
      auto const &mine = existing_from_prelude ? *sym : *existing_sym;
      RaiseIf<errors::SppIdentifierDuplicateError>(
        not is_functional,
        {mine.ScopeDefinedIn ? mine.ScopeDefinedIn : this},
        ERR_ARGS(*mine.Name, "type identifier", mine.IsImport()));
    }

    // Standard raise for non-functional symbols.
    RaiseIf<errors::SppIdentifierDuplicateError>(
      not is_functional,
      {
        existing_sym->ScopeDefinedIn ? existing_sym->ScopeDefinedIn : this,
        sym->ScopeDefinedIn ? sym->ScopeDefinedIn : this
      },
      ERR_ARGS(*existing_sym->Name, *sym->Name, "type identifier"));
  }

  // Add a type symbol to the corresponding symbol table.
  InternalTable.TypeTable.Add(sym->Name.get(), sym);
}

/// [CHECKED]
auto Scope::AddNsSymbol(
  Shared<NamespaceSymbol> const &sym) -> void {
  // Add a namespace symbol to the symbol table.
  InternalTable.NsTable.Add(sym->Name.get(), sym);
}

/// [CHECKED]
auto Scope::AddNsSymbolCheckConflict(
  Shared<NamespaceSymbol> const &sym) -> void {
  // A namespace may not be declared twice in one scope,
  // so if it is found in the immediate parent (exclusive)
  // then raise an error.
  const auto existing_sym = FindNsSymbol(sym->Name.get(), true);
  RaiseIf<errors::SppIdentifierDuplicateError>(
    existing_sym != nullptr, {this},
    ERR_ARGS(*existing_sym->Name, *sym->Name, "namespace identifier"));
  InternalTable.NsTable.Add(sym->Name.get(), sym);
}

/// [CHECKED]
auto Scope::RemVarSymbol(
  IdentifierAst const *sym_name) -> Shared<VariableSymbol> {
  // Remove a variable symbol from the symbol table.
  return InternalTable.VarTable.Rem(sym_name);
}

/// [CHECKED]
auto Scope::RemTypeSymbol(
  TypeIdentifierAst const *sym_name) -> Shared<TypeSymbol> {
  // Remove a type symbol from the symbol table.
  return InternalTable.TypeTable.Rem(sym_name);
}

/// [CHECKED]
auto Scope::RemNsSymbol(
  IdentifierAst const *sym_name) -> Shared<NamespaceSymbol> {
  // Remove a namespace symbol from the symbol table.
  return InternalTable.NsTable.Rem(sym_name);
}

/// [CHECKED]
auto Scope::GetAllVarSymbols(
  const bool exclusive, const bool sup_scope_search) const -> Vec<VariableSymbol*> {
  // Get all the variable symbols from this scope.
  auto syms = InternalTable.VarTable.GetAll();

  // For non-exclusive searches where a parent is present,
  // get the variable symbols from the parent scope too.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->GetAllVarSymbols(exclusive, sup_scope_search));
  }

  // For sup-allowed searches, for each sup scope get the,
  // variable symbols from the sup scopes too.
  if (sup_scope_search) {
    for (auto const *sup_scope : GetSupScopes()) {
      syms.AppendRange(sup_scope->GetAllVarSymbols(true, false));
    }
  }

  return syms;
}

/// [CHECKED]
auto Scope::GetAllTypeSymbols(
  const bool exclusive, const bool sup_scope_search) const -> Vec<TypeSymbol*> {
  // Get all the type symbols from this scope.
  auto syms = InternalTable.TypeTable.GetAll();

  // For non-exclusive searches where a parent is present,
  // get the type symbols from the parent scope too.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->GetAllTypeSymbols(exclusive, sup_scope_search));
  }

  // For sup-allowed searches, for each sup scope get the,
  // type symbols from the sup scopes too.
  if (sup_scope_search) {
    for (const auto sup_scope : GetSupScopes()) {
      syms.AppendRange(sup_scope->GetAllTypeSymbols(true, false));
    }
  }

  return syms;
}

/// [CHECKED]
auto Scope::GetAllNsSymbols(
  const bool exclusive) const -> Vec<NamespaceSymbol*> {
  // Get all the namespace symbols from this scope.
  auto syms = InternalTable.NsTable.GetAll();

  // For non-exclusive searches where a parent is present,
  // get the namespace symbols from the parent scope too.
  if (not exclusive and Parent != nullptr) {
    syms.AppendRange(Parent->GetAllNsSymbols(exclusive));
  }
  return syms;
}

/// [CHECKED]
auto Scope::HasVarSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const -> bool {
  // Check if getting the symbol returns nullptr or not.
  return FindVarSymbol(sym_name, exclusive) != nullptr;
}

/// [CHECKED]
auto Scope::HasTypeSymbol(
  TypeAst const *sym_name, const bool exclusive) const -> bool {
  // Check if getting the symbol returns nullptr or not.
  return FindTypeSymbol(sym_name, exclusive) != nullptr;
}

/// [CHECKED]
auto Scope::HasNsSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const -> bool {
  // Check if getting the symbol returns nullptr or not.
  return FindNsSymbol(sym_name, exclusive) != nullptr;
}

/// [CHECKED]
auto Scope::FindVarSymbol(
  IdentifierAst const *sym_name, const bool exclusive,
  const bool sup_scope_search) const -> VariableSymbol* {
  if (sym_name == nullptr) { return nullptr; }

  // If an identifier has been stamped with an identity, then
  // lookup the symbol by the stamped id. If found, return it.
  if (not exclusive and sym_name->StampedCompId() != nullptr) {
    if (auto *const canon = FindBoundVarSymbolById(sym_name->StampedCompId()); canon != nullptr) { return canon; }
  }
  const auto scope = this;

  // Get the symbol from the symbol table if it exists, then
  // try the parent scope and the ancestors (if exclusivity
  // allows for it). Then try the super scopes.
  auto sym = InternalTable.VarTable.Find(sym_name);
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->FindVarSymbol(sym_name, exclusive, sup_scope_search);
  }
  if (sym == nullptr and sup_scope_search) {
    sym = FindInSupScopesByName<VariableSymbol>(*scope, sym_name);
  }

  // Check for a linked aliased variable symbol.
  if (sym != nullptr and sym->AliasSymbol != nullptr) {
    sym = sym->AliasSymbol.get();
  }
  return sym;
}

/// [CHECKED]
auto Scope::FindBoundVarSymbolById(
  const CompId written) const -> VariableSymbol* {
  // Get the variable symbol by the comp id normally, canon it
  // into this scope, and return the result.
  const auto param = FindVarSymbolById(written);
  return param != nullptr ? _CanonVar(*param) : nullptr;
}

/// [CHECKED]
auto Scope::FindVarSymbolOutermost(
  Ast const &expr) const -> Pair<VariableSymbol*, Scope const*> {
  if (IsRuntimeMemberAccess(&expr)) {
    // Keep moving into the left-hand-side until there is
    // no left-hand-side: "a.b.c" becomes "a".
    auto adjusted_name = &expr;
    while (IsRuntimeMemberAccess(adjusted_name) or IsDeref(adjusted_name)) {
      adjusted_name = adjusted_name->To<PostfixExpressionAst>()->Lhs.get();
    }

    // Get the symbol (will be in this scope), and return
    // it with the scope.
    auto sym = FindVarSymbol(adjusted_name->To<IdentifierAst>());
    return {sym, this};
  }

  if (IsStaticMemberAccess(&expr)) {
    // This is possible with a left-hand-side type or
    // namespace.
    const auto postfix_expr = expr.ToUnchecked<PostfixExpressionAst>();
    const auto postfix_op = postfix_expr->Op->ToUnchecked<PostfixExpressionOperatorStaticMemberAccessAst>();

    // Type based left-hand-side, such as
    // "some_namespace::Type::static_member()"
    if (const auto type_lhs = postfix_expr->Lhs->To<TypeAst>()) {
      const auto type_sym = FindTypeSymbol(type_lhs);
      if (type_sym == nullptr or type_sym->LinkedScope == nullptr) { return {nullptr, this}; }
      const auto var_sym = type_sym->LinkedScope->FindVarSymbol(postfix_op->Name.get());
      return {var_sym, const_cast<Scope const*>(type_sym->LinkedScope)};
    }

    // Namespace based left-hand-side, such as
    // "a::b::c::my_function()"
    const auto namespace_scope = FindNsScope(postfix_expr->Lhs.get());
    auto sym = namespace_scope ? namespace_scope->FindVarSymbol(postfix_op->Name.get()) : nullptr;
    return {sym, namespace_scope};
  }

  // Identifiers or non-symbolic expressions can use the
  // normal lookup.
  auto sym = FindVarSymbol(expr.To<IdentifierAst>());
  return {sym, this};
}

/// [CHECKED]
auto Scope::FindTypeSymbol(
  TypeAst const *sym_name, const bool exclusive,
  const bool sup_scope_search) const -> TypeSymbol* {
  if (sym_name == nullptr) { return nullptr; }

  // If a type has been stamped with an identity, then lookup
  // the symbol by the stamped id. If found, return it.
  if (not exclusive) {
    if (const auto type_id = sym_name->StampedTypeId(); type_id != nullptr) {
      if (const auto found = FindBoundTypeSymbolById(type_id); found != nullptr) {
        return found;
      }
    }
  }

  // Adjust the scope for the namespace of the type identifier
  // if there is one.
  auto scope = this;
  auto sym_name_extracted = static_cast<TypeIdentifierAst const*>(nullptr);
  if (sym_name->IsTypeIdentifier()) {
    sym_name_extracted = sym_name->ToUnchecked<TypeIdentifierAst>();
  }
  else {
    auto [scope_, sym_name_extracted_] = ShiftForNamespacedType(*this, *sym_name);
    if (scope_ == nullptr) { return nullptr; }
    scope = scope_;
    sym_name_extracted = sym_name_extracted_;
  }

  // Re-try the stamped bind lookup now that the qualification
  // had been done by the scopes.
  if (not exclusive and sym_name_extracted != sym_name) {
    if (const auto written = sym_name_extracted->StampedTypeId(); written != nullptr) {
      if (const auto found = FindBoundTypeSymbolById(written); found != nullptr) {
        return found;
      }
    }
  }

  // An instantiation is answered by identity alone: its template
  // and what its arguments resolve to from here, filed in the
  // template's instances, or nothing when it is not made yet.
  if (not sym_name_extracted->GnArgGroup->Args.IsEmpty()) {
    const auto [tmpl, id] = NamedInstance(*scope, *sym_name_extracted);
    if (id == nullptr) { return nullptr; }
    const auto hit = tmpl->Instances.find(id);
    return hit != tmpl->Instances.end() ? hit->second : nullptr;
  }

  // Get the symbol from the symbol table if it exists, then
  // try the parent scope and the ancestors (if exclusivity
  // allows for it). Then try the super scopes.
  auto sym = scope->InternalTable.TypeTable.Find(sym_name_extracted);
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->FindTypeSymbol(sym_name_extracted, exclusive, sup_scope_search);
  }
  if (sym == nullptr and sup_scope_search) {
    sym = FindInSupScopesByName<TypeSymbol>(*scope, sym_name_extracted);
  }
  return sym;
}

/// [CHECKED]
auto Scope::FindBoundTypeSymbolById(
  const TypeId type_id) const -> TypeSymbol* {
  // Lookup the symbol as if it were in this scope; find it
  // normally, and then canon it into the scope.
  if (type_id == nullptr or IsSelfTypeId(type_id)) { return nullptr; }
  const auto sym = FindTypeSymbolById(type_id);
  return sym != nullptr ? _CanonType(*sym) : nullptr;
}

/// [CHECKED]
auto Scope::FindSelfSymbol(
  const bool exclusive) const -> TypeSymbol* {
  using generate::common_types_precompiled::SELF_TYPE;
  return FindTypeSymbol(SELF_TYPE.get(), exclusive);
}

/// [CHECKED]
auto Scope::FindHeadSymbol(
  TypeAst const &type, const bool exclusive) const -> TypeSymbol* {
  // To find the symbol for the head of a type, strip the
  // generics, find the stripped symbol, and pass through
  // the "exclusive" flag.
  return FindTypeSymbol(type.WithoutGns().get(), exclusive);
}

/// [CHECKED]
auto Scope::FindNsSymbol(
  IdentifierAst const *sym_name, const bool exclusive) const -> NamespaceSymbol* {
  // Get the symbol from the symbol table if it exists.
  if (sym_name == nullptr) { return nullptr; }
  const auto scope = this;
  auto sym = InternalTable.NsTable.Find(sym_name);

  // If the symbol doesn't exist, and this is a non-exclusive
  // search, check the parent scope.
  if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
    sym = scope->Parent->FindNsSymbol(sym_name, exclusive);
  }

  // Return the found symbol, or nullptr.
  return sym;
}

/// [CHECKED]
auto Scope::ReadIn(const TypeId written) const -> TypeId {
  // If the type id doesn't hold any generic parameters under
  // it, then there is no changes to be made, so early return.
  if (written == nullptr) { return nullptr; }
  if (not DoesTypeIdNameAnyGnParams(written)) { return written; }

  // Given the existing type id (written), and what this scope
  // binds the parameters it names to, call the internal
  // substitution function.
  return SubstituteTypeId(written, _BindingsOf(ParamsNamedBy(written)));
}

auto Scope::ReadCompIn(const CompId written) const -> CompId {
  // The comp parameters it names, each replaced by what this scope binds it to.
  if (written == nullptr) { return nullptr; }
  auto params = TypeIdParams();
  params.CompParams = ParamsNamedBy(written);
  if (params.CompParams.empty()) { return written; }
  return SubstituteCompId(written, _BindingsOf(params));
}

auto Scope::_BindingsOf(TypeIdParams const &params) const -> GenericSubst {
  auto subst = GenericSubst();

  // For every type parameter, get the type symbol for each
  // generic parameter id, and canon it into this scope. No
  // change in symbol => continue looping.
  for (const auto gn_param_id : params.TypeParams) {
    const auto param = FindGnTypeParamById(gn_param_id);
    const auto binding = param != nullptr ? _CanonType(*param) : nullptr;
    if (binding == nullptr or binding == param) { continue; }

    // Map the new type symbol from the canon process into a
    // type id. If we have a type id match, continue the loop
    // too as it isn't a true change.
    const auto bound = TypeIdOfSymbol(*binding);
    if (bound == nullptr) { continue; }
    auto const &head = HeadOf(bound);
    const auto is_itself = (head.Kind == TypeKey::Tag::TypeParam or head.Kind == TypeKey::Tag::TypeBound)
      and head.TypeParamId == gn_param_id;
    if (is_itself) { continue; }

    // Record the type id -> type id substitution into the
    // type params map on the overall substitution, and
    // record if it was a pack or not.
    subst.TypeParams.emplace_back(gn_param_id, bound);
    if (param->IsVariadic) { subst.TypePackParams.push_back(gn_param_id); }
  }

  // For every comp parameter, get the comp symbol for each
  // generic parameter id, and canon it into this scope. No
  // change in symbol => continue looping.
  for (const auto comp : params.CompParams) {
    const auto param = FindGnCompParamById(comp);
    const auto binding = param != nullptr ? _CanonVar(*param) : nullptr;
    if (binding == nullptr or binding == param) { continue; }

    // Map the new var symbol from the canon process into a
    // comp id. If we have a comp id match, continue the loop
    // too as it isn't a true change.
    const auto bound = CompIdOfSymbol(*binding);
    if (bound == ParamCompId(comp)) { continue; }

    // Record the comp id -> comp id substitution into the
    // comp params map on the overall substitution, and
    // record if it was a pack or not.
    subst.CompParams.emplace_back(comp, bound);
    if (param->IsVariadic) { subst.CompPackParams.push_back(comp); }
  }

  return subst;
}

/// [CHECKED]
auto Scope::ArgsIdOf(
  Vec<GenericArgumentAst*> const &args,
  GenericParameterGroupAst const *params) const -> TypeId {
  // Start with an empty instance key. This will get built upon
  // as the generics are considered.
  auto key = TypeKey();

  // Each argument is keyed by the parameter it is for ("ParamId"): a keyword by the identity its name carries (a
  // parameter's own name, "FromParams", or a variadic pack's, "GnPackParamId"), else by the parameter of that name in
  // "params", the group it is written against, and "Self" as parameter 0; a positional argument by the parameter it
  // binds ("ParamsBoundByArgs"). A pack's elements, and anything past the parameters, stay positional. A keyword naming
  // no parameter of the group is the one argument spelled, an error the analysis reports. A comp argument is its
  // value's identity: folded where closed, its parameters by identity where not.
  const auto targets = params != nullptr
    ? utils::type_resolution::ParamsBoundByArgs(args, params->GetAllParams())
    : Vec<GenericParameterAst*>();
  const auto param_named = [params](TypeIdentifierAst const &name) -> std::uint64_t {
    if (const auto stamped = name.StampedTypeId(); stamped != nullptr) {
      auto const &head = HeadOf(stamped);
      if (head.Kind == TypeKey::Tag::TypeParam or head.Kind == TypeKey::Tag::TypeBound) { return head.TypeParamId; }
    }
    if (params == nullptr) { return 0; }
    for (auto const *param : params->GetAllParams()) {
      if (param->Name->ToUnchecked<TypeIdentifierAst>()->ToView() == name.ToView()) { return param->ParamId(); }
    }
    return 0;
  };

  for (auto i = 0uz; i < args.Len(); ++i) {
    const auto arg = args[i];
    const auto param = arg->KeywordName() == nullptr and i < targets.Len() ? targets[i] : nullptr;

    // A keyword: the parameter it names, "Self" as 0, else spelled.
    if (arg->KeywordName() != nullptr) {
      auto const &name = *arg->KeywordName()->ToUnchecked<TypeIdentifierAst>();
      if (const auto id = param_named(name); id != 0) { key.Push(TypeKey::Tag::Arg, id); }
      else if (name.ToView() == "Self") { key.Push(TypeKey::Tag::Arg, 0); }
      else { key.PushText(TypeKey::Tag::Name, name.ToView()); }
    }

    // A positional argument binding a parameter (not a pack, whose
    // elements stay positional): that parameter, or its name before
    // it has an identity.
    else if (param != nullptr and not param->IsVariadic()) {
      if (param->ParamId() != 0) { key.Push(TypeKey::Tag::Arg, param->ParamId()); }
      else { key.PushText(TypeKey::Tag::Name, param->Name->ToUnchecked<TypeIdentifierAst>()->ToView()); }
    }

    // Otherwise we have a genuinely positional argument, so push
    // the index.
    else {
      key.Push(TypeKey::Tag::Pos, i);
    }

    // Once the name has been pushed into the key, we push the
    // type or comp value in afterwards. Note the asymmetric calls
    // below, because types can be unresolved but still need keying.
    if (arg->IsTypeArg()) { key.PushTypePart(InternTypeKey(_TypeKey(*arg->TypeVal))); }
    if (arg->IsCompArg()) { key.PushCompPart(CompIdOf(*arg->CompVal)); }
  }

  // Finally, intern the instance key into a type id, which is the
  // final interned value for the "args id of".
  return InternTypeKey(std::move(key));
}

/// [CHECKED]
auto Scope::InstanceIdOf(
  TypeSymbol const &named, Vec<GenericArgumentAst*> const &args,
  GenericParameterGroupAst const *params) const -> TypeId {
  // Keyed under the template its instances are filed under: through
  // a "use" of a class, that class, so the instance has one identity
  // whichever name reached it ("TypeSymbol::InstanceTemplate").
  auto const &head = *named.InstanceTemplate();

  // Get the template of the symbol, if the symbol is an instantiation,
  // otherwise it is itself, a template.
  using generate::common_types_precompiled::VAR;
  const auto tmpl = head.InstanceOf != nullptr ? head.InstanceOf : &head;

  // Handle the case where the input symbol is not an alias, and the
  // template is the variant type template (Var).
  if (head.Alias == nullptr and tmpl == PrecompiledTemplate(*VAR, *this)) {
    // Read the variants off of the "..Variants" generic type pack.
    const auto variants = args.Len() == 1 and args[0]->KeywordName() != nullptr and args[0]->ViewName() == "Variants"
      ? args[0]
      : nullptr;
    auto members = Vec<TypeAst const*>();

    // If we have extracted the variants as a valid "Variants..."
    // type argument, read each member off. An unbound pack named
    // there ("Var[Variants]" in its own block) is the member that
    // stands for all of them, as a pattern binds it.
    if (variants != nullptr and variants->IsTypeArg()) {
      if (variants->TypeVal->LastTypePart()->GnArgGroup->Args.IsEmpty()
        and utils::packs::DoesTypeNameAnUnboundPack(*variants->TypeVal, *this)) {
        members.EmplaceBack(variants->TypeVal.get());
      }
      for (const auto member : variants->TypeVal->LastTypePart()->GnArgGroup->GetTypeArgs()) {
        members.EmplaceBack(member->TypeVal.get());
      }
    }

    // Otherwise, read of the non-tuple converted pack into the
    // members.
    else {
      for (const auto arg : args) {
        if (arg->KeywordName() == nullptr and arg->IsTypeArg()) { members.EmplaceBack(arg->TypeVal.get()); }
      }
    }

    // Create a variant-based instance key and intern it into a
    // type id.
    return InternTypeKey(VariantKey(head, members, *this).Key);
  }

  // Otherwise, this is for any normal type, so build a type id
  // based off of the args and the type: each argument keyed by the
  // parameter it is for, of the template it is filed under unless
  // the caller names the group.
  return InstanceIdOfArgs(head, ArgsIdOf(args, params != nullptr ? params : head.GnParams()));
}

/// [CHECKED]
auto Scope::FindTypeSymbolById(const TypeId id) const -> TypeSymbol* {
  if (id == nullptr or id->HasUnresolved) { return nullptr; }

  // Branch on the kind of the identity's head ("HeadOf"); its
  // symbol is the class, or the template ("Vec" for the head
  // of "Vec[T=Str]").
  auto const &head = HeadOf(id);
  const auto ptr = head.Symbol();
  switch (head.Kind) {
    // If the instance key was built off of a symbol directly,
    // just return the symbol.
    case TypeKey::Tag::Symbol:
      return ptr;

    // For a type parameter, build the type symbol based on the
    // generic parameter id; "Self" (0) is what this scope's
    // "Self" symbol is bound to.
    case TypeKey::Tag::TypeParam: {
      if (head.TypeParamId != 0) { return FindGnTypeParamById(head.TypeParamId); }
      const auto self = FindSelfSymbol();
      return self != nullptr ? self->AsBound() : nullptr;
    }

    // For a generic instantiation or variant, find the actual
    // instantiation based on the type id.
    case TypeKey::Tag::Inst:
    case TypeKey::Tag::Variant: {
      const auto hit = ptr->Instances.find(BareOf(id));
      return hit != ptr->Instances.end() ? hit->second : nullptr;
    }


    // Unreachable state.
    default:
      return nullptr;
  }
}

/// [CHECKED]
auto Scope::FindVarSymbolById(
  const CompId id) const -> VariableSymbol* {
  // Only a parameter has a symbol: get the variable symbol of
  // the generic parameter its identity names.
  return id != nullptr and id->Kind == CompKey::Part::Param ? FindGnCompParamById(id->ParamId) : nullptr;
}

/// [CHECKED]
auto Scope::TypeAstOf(const TypeId id) const -> Shared<TypeAst> {
  using generate::common_types_precompiled::SELF_TYPE;
  using generate::common_types_precompiled::TUP;
  if (id == nullptr) { return nullptr; }
  auto const &head = HeadOf(id);
  const auto bare = BareOf(id);

  auto out = Shared<TypeAst>(nullptr);
  const auto sym = bare->HasSelf or head.IsInstance() ? nullptr : FindTypeSymbolById(bare);
  if (sym != nullptr) { out = sym->FqName(); }
  else {
    switch (head.Kind) {
      // For a type parameter, or a bound type parameter, get the
      // type symbol from the parameter identity. Get the fully
      // qualified type from it, and stamp the id into it. "Self"
      // (0) is the "SELF" precompiled constant.
      case TypeKey::Tag::TypeParam:
      case TypeKey::Tag::TypeBound: {
        if (head.Kind == TypeKey::Tag::TypeParam and head.TypeParamId == 0) {
          out = AstCloneShared(SELF_TYPE.get());
          break;
        }
        const auto param = FindGnTypeParamById(head.TypeParamId);
        if (param == nullptr) { return nullptr; }
        out = AstCloneShared(param->FqName().get());
        out->LastTypePart()->StampTypeId(
          head.Kind == TypeKey::Tag::TypeParam ? bare : NameTypeIdOf(*param));
        break;
      }


      // For a generic instantiation, get each generic argument's
      // name, and reconstruct the generic argument's equivalent
      // ast. Attach the group to the template.
      case TypeKey::Tag::Inst: {
        const auto tmpl = head.Symbol();
        auto group = GenericArgumentGroupAst::NewEmpty();
        for (auto const &arg : ArgsOf(head.Args)) {
          auto name = arg.Named
            ? TypeIdentifierAst::FromString(ArgNameOf(arg))
            : nullptr;
          if (arg.TypeVal != nullptr) {
            auto type = TypeAstOf(arg.TypeVal);
            if (type == nullptr) { return nullptr; }
            group->Args.EmplaceBack(GenericArgumentAst::NewType(std::move(name), std::move(type)));
            continue;
          }
          auto value = CompAstOf(arg.CompVal);
          if (value == nullptr) { return nullptr; }
          group->Args.EmplaceBack(GenericArgumentAst::NewComp(std::move(name), std::move(value)));
        }

        auto head = tmpl->Alias != nullptr ? AstCloneShared(tmpl->Name.get()) : tmpl->FqName()->WithoutGns();
        out = head->WithGns(std::move(group));
        out->LastTypePart()->StampTemplateId(NameTypeIdOf(*tmpl));
        if (IsStampableTypeId(bare)) { out->LastTypePart()->StampTypeId(bare); }
        break;
      }

      // For variant types, recursively convert each inner member
      // of the variant, and apply the ordering. Build a variant
      // type and insert the type arguments.
      case TypeKey::Tag::Variant: {
        auto unordered = Vec<Pair<Shared<TypeAst>, TypeSymbol const*>>();
        for (const auto member : head.Members) {
          auto type = TypeAstOf(member);
          if (type == nullptr) { return nullptr; }
          unordered.EmplaceBack(std::move(type), FindTypeSymbolById(BareOf(member)));
        }
        auto members = utils::type_compare::OrderVariantMembers(std::move(unordered));
        out = generate::common_types::VariantType(0, {});
        out->LastTypePart()->GnArgGroup->Args.EmplaceBack(GenericArgumentAst::NewType(
          TypeIdentifierAst::FromString("Variants"), generate::common_types::TupleType(0, std::move(members))));
        if (IsStampableTypeId(bare)) {
          out->LastTypePart()->StampTypeId(bare);
          // Its members' tuple is the "Tup" instance over them, keyed
          // from its template and arguments (as an unmade one is,
          // "KeyUnmade"): its members are each stamped already, so
          // nothing is looked up by name.
          if (const auto variants = out->LastTypePart()->GnArgGroup->At("Variants"); variants != nullptr) {
            auto const &tup = *variants->TypeVal->LastTypePart();
            if (auto const *const tup_tmpl = PrecompiledTemplate(*TUP, *this)) {
              if (const auto tup_id = InstanceIdOf(*tup_tmpl, tup.GnArgGroup->GetAllArgs()); tup_id != nullptr) {
                tup.StampTypeId(tup_id);
              }
            }
          }
        }
        break;
      }

      // Failsafe (should never be reached by case-enum member
      // exhaustion).
      default:
        return nullptr;
    }
  }

  // Apply the convention if required.
  if (out == nullptr or head.Conv == 0) { return out; }
  return out->WithConvention(ConventionAstOf(static_cast<ConventionTag>(head.Conv)));
}

/// [CHECKED]
auto Scope::CompAstOf(const CompId id) const -> Unique<ExpressionAst> {
  if (id == nullptr) { return nullptr; }
  auto const &node = *id;
  switch (node.Kind) {
    // A value is its literal ("2_uz", "-3_s32", "true"). A float
    // whose decimal terminates is its literal, exactly; one that
    // does not ("1.0 / 3.0") has no literal, so it is its numerator
    // over its denominator, which folds back to the same value
    // ("FoldCompValues"): a literal cut short would be another
    // value, so another type.
    case CompKey::Part::Value: {
      if (auto const *const val = node.AsBool(); val != nullptr) { return BooleanLiteralAst::FromCppVal(*val); }
      if (auto const *const val = node.AsInt(); val != nullptr) { return IntegerLiteralAst::FromBigVal(*val, node.Text); }
      if (auto const *const val = node.AsFloat(); val != nullptr) {
        if (const auto places = ExactDecimalPlaces(*val); places.has_value()) {
          return FloatLiteralAst::FromBigVal(*val, node.Text, *places);
        }
        auto num = FloatLiteralAst::FromBigVal(numex::BigDec(val->GetNumerator(), numex::BigInt(1)), node.Text, 0uz);
        auto den = FloatLiteralAst::FromBigVal(numex::BigDec(val->GetDenominator(), numex::BigInt(1)), node.Text, 0uz);
        return MakeUnique<BinaryExpressionAst>(
          std::move(num), MakeUnique<TokenAst>(0uz, lex::SppTokenType::TK_DIV, lex::TokToString(lex::SppTokenType::TK_DIV)),
          std::move(den));
      }
      return nullptr;
    }

    // For a generic comp parameter, get the corresponding
    // variable symbol for the generic parameter id, clone
    // the name, and bind the generic param id to the clone.
    case CompKey::Part::Param: {
      const auto param = FindGnCompParamById(node.ParamId);
      if (param == nullptr) { return nullptr; }
      auto name = AstClone(param->Name.get());
      name->StampCompId(id);
      return name;
    }

    // For a generic comp parameter pack, convert each
    // element, and then wrap the load inside a tuple literal
    // ast.
    case CompKey::Part::Pack: {
      auto values = Vec<Unique<ExpressionAst>>();
      for (const auto elem : node.Kids) {
        auto value = CompAstOf(elem);
        if (value == nullptr) { return nullptr; }
        values.EmplaceBack(std::move(value));
      }
      return MakeUnique<TupleLiteralAst>(nullptr, std::move(values), nullptr);
    }

    // For a binary expression, convert the two operands,
    // extract the operator token, and build the binary
    // expression back up.
    case CompKey::Part::Op: {
      const auto tok = CompOperatorToken(node.Text);
      auto lhs = CompAstOf(node.Kids[0]);
      auto rhs = CompAstOf(node.Kids[1]);
      if (not tok.has_value() or lhs == nullptr or rhs == nullptr) { return nullptr; }
      return MakeUnique<BinaryExpressionAst>(
        std::move(lhs), MakeUnique<TokenAst>(0uz, *tok, lex::TokToString(*tok)), std::move(rhs));
    }

    // The opaque expression don't have a conversion method,
    // so instead, use the static map to grab the value out,
    // and clone the correct target.
    case CompKey::Part::Opaque: {
      const auto hit = OpaqueValues().find(id);
      return hit != OpaqueValues().end() ? AstClone(hit->second.get()) : nullptr;
    }

    // For a member access, take the interned type off of the
    // comp node, build it back to a type id, and then convert
    // that to a type ast. Then create "Type::value".
    case CompKey::Part::Member: {
      const auto owner = TypeAstOf(TypeIdOfWord(node.Type));
      if (owner == nullptr) { return nullptr; }
      return MakeUnique<PostfixExpressionAst>(
        AstClone(owner.get()), MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(
          nullptr, MakeShared<IdentifierAst>(0uz, node.Text)));
    }

    // Failsafe (should never be reached by case-enum member
    // exhaustion).
    default:
      return nullptr;
  }
}

/// [CHECKED]
auto Scope::FoldedCompAstOf(
  ExpressionAst const &value) const -> Unique<ExpressionAst> {
  // Key the value (folding it), and read back only a value: an
  // identity naming anything open would read back as itself.
  const auto id = CompIdOf(value);
  return IsValueCompId(id) ? CompAstOf(id) : nullptr;
}

/// [CHECKED]
auto Scope::PartialTypeIdOf(TypeAst const &type) const -> TypeId {
  // Convert the type into an instance key, and then intern that
  // into the TypeId.
  return InternTypeKey(_TypeKey(type));
}

/// [CHECKED]
auto Scope::TypeIdOf(TypeAst const &type) const -> TypeId {
  // Convert the type into an instance key. If the type is fully
  // resolved, intern it into the TypeId, otherwise return nullptr.
  auto key = _TypeKey(type);
  return key.HasUnresolved ? nullptr : InternTypeKey(std::move(key));
}

/// [CHECKED]
auto Scope::CompIdOf(ExpressionAst const &value) const -> CompId {
  // Convert the comp into a comp node, and then intern that into
  // the CompId.
  auto key = _CompKey(value);
  return InternCompKey(std::move(key));
}

/// [CHECKED]
auto Scope::TypeIdOfSymbol(TypeSymbol const &sym) const -> TypeId {
  // Key the type symbol into the scope, and read off the "Key"
  // field (ignore unused variant "Members".
  auto key = RawTypeKey(nullptr, &sym, *this).Key;
  return key.HasUnresolved ? nullptr : InternTypeKey(std::move(key));
}

/// [CHECKED]
auto Scope::CompIdOfSymbol(VariableSymbol const &sym) const -> CompId {
  // If there is a comp val bound to the symbol, get its comp
  // id. Otherwise, work off of the param id.
  if (sym.BoundCompId != nullptr) { return sym.BoundCompId; }
  if (const auto bound = sym.BoundCompVal(); bound != nullptr) { return CompIdOf(*bound); }
  return ParamCompId(sym.ParamId());
}

/// [CHECKED]
auto Scope::DepthDiff(const Scope *scope) const -> sys::ssize_t {
  // Create an internal function to call recursively with
  // a counter.
  auto func = [](this auto &&self, Scope const *source, Scope const *target, const sys::ssize_t depth) -> sys::ssize_t {
    if (source == target) { return depth; }
    for (auto const *sup_scope : source->DirectSupScopes) {
      if (const auto result = self(sup_scope, target, depth + 1z); result >= 0z) {
        return result;
      }
    }
    return -1;
  };

  return func(this, scope, 0z);
}

/// [CHECKED]
auto Scope::GetFinalChildScope() const -> Scope const* {
  // If there are no children, return this scope (base case
  // for the recursion). Otherwise, return the final child
  // scope (recursively searching).
  return Children.IsEmpty()
    ? this
    : Children.Back()->GetFinalChildScope();
}

/// [CHECKED]
auto Scope::GetAncestors() const -> Vec<Scope const*> {
  // Get all ancestor scopes, including this scope, and the
  // global scope.
  auto scopes = Vec<Scope const*>();
  for (auto scope = this; scope != nullptr; scope = scope->Parent) {
    scopes.EmplaceBack(scope);
  }
  return scopes;
}

/// [CHECKED]
auto Scope::GetParentModule() -> Scope* {
  return ParentModuleOf(this);
}

auto Scope::GetParentModule() const -> Scope const* {
  return ParentModuleOf(this);
}

/// [CHECKED]
auto Scope::GetTopLevelParentModule() -> Scope* {
  return TopLevelParentModuleOf(this);
}

auto Scope::GetTopLevelParentModule() const -> Scope const* {
  return TopLevelParentModuleOf(this);
}

/// [CHECKED]
auto Scope::FindEnclosingSelfType(
  CompilerMetaData const &meta) const -> Shared<TypeAst> {
  // If we are already in a module scope, there is no self
  // type. Easy shortcut (and downstream error prevention).
  auto current_scope = this;
  if (std::holds_alternative<ScopeIdentifierName>(current_scope->Name)) {
    return nullptr;
  }

  const auto is_closure_outer = [](Scope const *scope) {
    return AstAs<ClosureExpressionParameterAndCaptureGroupAst>(scope->AstNode) != nullptr;
  };

  // Escape closure scopes for the Self type. Prefer using
  // the "Self" symbol type first.
  if (is_closure_outer(current_scope) and meta.OverriddenScopeForClosure != nullptr) {
    current_scope = meta.OverriddenScopeForClosure;
  }

  // Inside a template, "Self" is the template over its own
  // parameters ("TypeSymbol::GnSelfName"): Vec => Vec[T=T].
  if (const auto self_sym = current_scope->FindSelfSymbol();
    self_sym != nullptr and self_sym->LinkedSymbol() != self_sym) {
    return self_sym->LinkedSymbol()->GnSelfName();
  }

  // Use a "seen" walker to prevent scope searching cycles due
  // to nested closures, whose inner/outer scopes don't follow
  // normal scoping hierarchies. The parent chain is a tree, so
  // the only way back to a scope is the closure redirect below;
  // a once-only flag is not the same guard, as the override can
  // be an ancestor already walked (the set stops there).
  auto seen = Set<Scope const*>();
  while (true) {
    if (not seen.insert(current_scope).second) { return nullptr; }

    // Escape closure scopes for the Self type.
    if (is_closure_outer(current_scope) and meta.OverriddenScopeForClosure != nullptr) {
      current_scope = meta.OverriddenScopeForClosure;
      continue;
    }

    // Nothing above this scope, so nothing encloses it.
    if (current_scope->Parent == nullptr) { return nullptr; }

    // Only get a scope right under the module scope.
    if (not std::holds_alternative<ScopeIdentifierName>(current_scope->Parent->Name)) {
      current_scope = current_scope->Parent;
      continue;
    }

    // The walk lands on whatever ast owns the scope under
    // the module, which is not always one that declares a
    // type - a module-level type statement or function owns
    // one too, and neither encloses a "Self".
    return current_scope->AstNode != nullptr
      ? AstNameOrNull(current_scope->AstNode)
      : nullptr;
  }
  return nullptr;
}

/// [CHECKED]
auto Scope::GetSupScopes() const -> SupScopeList {
  // Get all super scopes, recursively, yielding each one once.
  // Use the cache if available, saving on a massive number of
  // allocations.
  const auto generation = TypeStructureGeneration();
  if (_SupScopesCache != nullptr and _SupScopesGen == generation) {
    return SupScopeList(_SupScopesCache);
  }

  // The function for walking scopes; it involves walking the
  // direct sup-scopes of a type, and then recursively walking
  // the sup-scopes' sup-scopes, etc.
  auto scopes = Vec<Scope*>();
  auto seen = Set<Scope const*>();
  const auto walk = [&](this auto &&self, Scope const &from) -> void {
    for (const auto sup_scope : from.GetDirectSupScopes()) {
      if (not seen.insert(sup_scope).second) { continue; }
      scopes.push_back(sup_scope);
      self(*sup_scope);
    }
  };
  walk(*this);

  // Cache the result given we didn't use the cache if we reach
  // this point. A new list replaces the old one rather than
  // overwriting it, so the answers handed out stay intact.
  _SupScopesCache = std::make_shared<Vec<Scope*> const>(std::move(scopes));
  _SupScopesGen = TypeStructureGeneration() == generation ? generation : 0;
  return SupScopeList(_SupScopesCache);
}

/// [CHECKED]
auto Scope::FindNsScope(
  Vec<IdentifierAst const*> const &parts) const -> Scope const* {
  // Starting from this scope (acting current scope), find each
  // part in the list, ["a", "b", "c"] from the below example,
  // moving towards the most nested scope.
  auto scope = this;
  for (const auto part : parts) {
    const auto sym = scope->FindNsSymbol(part);
    if (sym == nullptr) { return nullptr; }
    scope = sym->LinkedScope;
  }
  return scope;
}

/// [CHECKED]
auto Scope::FindNsScope(
  ExpressionAst const *postfix_ast) const -> Scope const* {
  // For the static member access like "a::b::c", this is stored
  // as "(a::b)::c", which is read as "c", "b", "a".
  auto parts = Vec<IdentifierAst const*>();
  auto lhs = postfix_ast;
  while (const auto postfix_lhs = lhs->To<PostfixExpressionAst>()) {
    parts.EmplaceBack(postfix_lhs->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>()->Name.get());
    lhs = postfix_lhs->Lhs.get();
  }

  // The final part isn't a postfix, but it is an identifier, so
  // add it to the end of the list ("a").
  if (const auto lhs_as_ident = lhs->To<IdentifierAst>(); lhs_as_ident != nullptr) {
    parts.EmplaceBack(lhs_as_ident);
  }

  // Reverse the list into the actual order "a", "b", "c", and then
  // find the namespace scope.
  parts |= genex::actions::reverse;
  return FindNsScope(parts);
}

/// [CHECKED]
auto Scope::NameAsString() const -> Str {
  // Identifier based scope name (get the "std::string"
  // field via the "ToString()" call).
  if (std::holds_alternative<ScopeIdentifierName>(Name)) {
    auto const name_as_id = std::get<ScopeIdentifierName>(Name).Name;
    return name_as_id->ToString();
  }

  // TypeIdentifier based scope name (get the "std::string"
  // creation from the "ToString()" call).
  if (std::holds_alternative<ScopeTypeIdentifierName>(Name)) {
    auto const name_as_type = std::get<ScopeTypeIdentifierName>(Name).Name;
    return name_as_type->ToString();
  }

  // Block name (already contains a std::string), so copy
  // it out.
  auto const name_as_block_name = std::get<ScopeBlockName>(Name).Name;
  return name_as_block_name;
}

/// [CHECKED]
auto Scope::FixChildrenToParentPointer() -> void {
  // Iterate all children, setting their parent pointer to
  // this scope. Recurse into them. This runs over a scope
  // tree that has already been read from, so anything cached
  // against the old shape of it has to go.
  BumpScopeLinkageGeneration();
  for (auto const &child : Children) {
    child->Parent = this;
    child->FixChildrenToParentPointer();
  }
}

/// [CHECKED]
auto Scope::ClearOpaqueCompValues() -> void {
  // Clear the opaque values from the static list.
  OpaqueValues().clear();
}

/// [CHECKED]
auto Scope::_TypeKey(TypeAst const &type) const -> TypeKey {
  // Create a raw instance key group for the type, and as this
  // isn't for variants specifically, just take the type key
  // of it.
  return RawTypeKey(&type, nullptr, *this).Key;
}

/// [CHECKED]
auto Scope::_CompKey(ExpressionAst const &expr) const -> CompKey {
  // Key the expression here, folded as it is built.
  return RawCompKey(expr, *this);
}

/// [CHECKED]
auto Scope::_CanonType(TypeSymbol &sym) const -> TypeSymbol* {
  // A generic parameter, or a binding of one, is the binding
  // of that parameter here.
  if (const auto param = sym.ParamId(); param != 0) {
    const auto binding = BindingOf(*this, sym, param, [](Scope const &scope, const std::uint64_t id) {
      return scope.InternalTable.TypeTable.FindByParam(id);
    });
    return binding;
  }

  // A closed class names the same type from anywhere; there are
  // no generics that can be canonical-ised.
  if (sym.Kind == TypeKind::Cls and sym.Alias == nullptr and sym.IsConcrete) { return &sym; }

  // An open instantiation is its identity read through this
  // scope's bindings ("ReadIn"): the instantiation filed under
  // that.
  if (sym.InstanceOf != nullptr and (sym.Kind == TypeKind::Cls or sym.Alias != nullptr)) {
    return sym.Id != nullptr ? FindTypeSymbolById(ReadIn(sym.Id)) : nullptr;
  }

  // An alias declared in a generic block has a copy per
  // instantiation of the block, its target read through that
  // instantiation's bindings ("CreateGnSupScope"): the copy
  // this scope reaches is the one it means. Copies share their
  // statement, which is how one is told from another alias of
  // that name (a "use" of it shares its name node, but is a
  // statement of its own).
  if (sym.InstanceOf == nullptr and sym.Alias != nullptr) {
    const auto here = FindHeadSymbol(*sym.Name);
    return here != nullptr and here->Alias != nullptr and here->Alias->Stmt == sym.Alias->Stmt ? here : &sym;
  }

  // Anything else names itself from anywhere: a template (only
  // its written identity, the stripped head of a name written
  // with arguments, brings one here), a mock, a class or alias
  // with nothing to re-read.
  return &sym;
}

/// [CHECKED]
auto Scope::_CanonVar(VariableSymbol &sym) const -> VariableSymbol* {
  // Like the canon type function, without the template, alias, and
  // open vs closed type logic.
  const auto param = sym.ParamId();
  if (param == 0) { return nullptr; }
  return BindingOf(*this, sym, param, [](Scope const &scope, const std::uint64_t id) {
    return scope.InternalTable.VarTable.FindByParam(id);
  });
}

SPP_MOD_END
