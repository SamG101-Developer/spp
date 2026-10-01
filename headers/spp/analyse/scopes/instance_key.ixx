module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.instance_key;
import spp.utils.interner;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS struct TypeIdHead;
  SPP_EXP_CLS struct TypeIdParams;

  /// The identity an instantiation is filed under: what each
  /// argument resolves to, as tagged words - the tag in the top
  /// byte and a payload below it, or a pointer in a word of its
  /// own after a "Sym" or "Id" word. Every tag is followed by a
  /// fixed shape, so two keys are equal exactly when their
  /// arguments are. A key is only compared and hashed, never
  /// printed.
  SPP_EXP_CLS struct InstanceKey {
    enum class Tag : std::uint8_t {
      Name, Pos, Conv, Unresolved, Bound, Param, Self, Sym, Comp, Inst, Variant, Len, Id
    };

    /// The interned parts of the instance information.
    std::vector<std::uint64_t> Words;

    /// The "intern" of this instantiation.
    std::uint64_t Hash = 0;

    /// Whether any part of the key names something that did not
    /// resolve, which is no type at all: two such keys spelled alike
    /// are equal as keys, never as types.
    bool HasUnresolved = false;

    /// Whether any part of the key is "Self", which is keyed by its
    /// spelling: what it means depends on the scope reading it.
    bool HasSelf = false;

    /// What "HeadOf", "ParamsOf" and "BareTypeId" read off an
    /// interned key, kept on it: an interned key never changes, so
    /// each is worked out once. Not part of the key.
    mutable TypeIdHead const *CachedHead = nullptr;
    mutable TypeIdParams const *CachedParams = nullptr;
    mutable InstanceKey const *CachedBare = nullptr;

    /// Append a tag with a small payload (an id, a position, a
    /// convention).
    auto Push(const Tag tag, const std::uint64_t payload = 0) -> void {
      HasUnresolved = HasUnresolved or tag == Tag::Unresolved;
      HasSelf = HasSelf or tag == Tag::Self;
      Append(static_cast<std::uint64_t>(tag) << 56 | payload);
    }

    /// Append a tag with text that is keyed by its spelling,
    /// interned.
    auto PushText(const Tag tag, const StrView text) -> void {
      Push(tag, static_cast<std::uint64_t>(utils::Intern(text)));
    }

    /// Append a symbol's identity: its address.
    auto PushPtr(void const *const ptr) -> void {
      Push(Tag::Sym);
      Append(reinterpret_cast<std::uintptr_t>(ptr));
    }

    /// Append an interned type ("TypeId") as one part of this key:
    /// the one word stands for its whole key, as interning makes
    /// equal keys one address.
    auto PushId(InstanceKey const *const id) -> void {
      HasSelf = HasSelf or id->HasSelf;
      Push(Tag::Id);
      Append(reinterpret_cast<std::uintptr_t>(id));
    }

    /// Append words already laid out as a key's, one part of another
    /// key copied as it stands: "unresolved" says whether they name
    /// something that did not resolve.
    auto AppendWords(std::span<std::uint64_t const> words, const bool unresolved, const bool self = false) -> void {
      HasUnresolved = HasUnresolved or unresolved;
      HasSelf = HasSelf or self;
      for (const auto word : words) { Append(word); }
    }

    /// Continue this key with another's words, as though they had
    /// been pushed here: the key of what a type stands for.
    auto AppendKey(InstanceKey const &that) -> void {
      HasUnresolved = HasUnresolved or that.HasUnresolved;
      HasSelf = HasSelf or that.HasSelf;
      for (const auto word : that.Words) { Append(word); }
    }

    /// Append a whole key, as one part of this one: its length
    /// first, so where it ends is part of the key.
    auto PushKey(InstanceKey const &that) -> void {
      Push(Tag::Len, that.Words.size());
      AppendKey(that);
    }

    auto operator==(InstanceKey const &that) const -> bool {
      return Hash == that.Hash and Words == that.Words;
    }

    /// An order over keys, so a set of them can be put in one
    /// canonical order within a compilation.
    auto operator<(InstanceKey const &that) const -> bool {
      return Words < that.Words;
    }

  private:
    auto Append(const std::uint64_t word) -> void {
      Words.push_back(word);
      Hash = (Hash ^ word) * 0x9E3779B97F4A7C15ull;
      Hash ^= Hash >> 29;
    }
  };

  /// A key hashes as the hash it accumulated while it was built.
  SPP_EXP_CLS struct InstanceKeyHash {
    auto operator()(InstanceKey const &key) const noexcept -> std::uint64_t { return key.Hash; }
  };

  /// A resolved type, interned: one per distinct key ("Scope::TypeKey"), so two types are one type exactly when their
  /// ids are equal. Only meaningful within one compilation, as keys hold symbol addresses.
  SPP_EXP_CLS using TypeId = InstanceKey const*;

  /// The one "TypeId" for a key.
  SPP_EXP_FUN inline auto InternTypeKey(InstanceKey &&key) -> TypeId {
    static auto interned = std::unordered_set<InstanceKey, InstanceKeyHash>();
    return &*interned.insert(std::move(key)).first;
  }

  /// Append "part" to "into" as one part: by its "TypeId" when it is a type, whole when it has an unresolved part
  /// (which is no type, and has to keep marking "into" as none).
  SPP_EXP_FUN inline auto PushTypePart(InstanceKey &into, InstanceKey &&part) -> void {
    if (part.HasUnresolved) { into.PushKey(part); }
    else { into.PushId(InternTypeKey(std::move(part))); }
  }

  /// What a substitution rewrites in a "TypeId" ("SubstituteTypeId"): type parameters by "ParamId" - 0 standing for
  /// "Self", which a key spells rather than identifies - and comp parameters by the interned text of their identity
  /// ("C<ParamId>", "comp_generics::CompExprIdentity"), each to what it is bound to.
  SPP_EXP_CLS struct TypeSubst {
    std::vector<std::pair<std::uint64_t, TypeId>> Types;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> Comps;

    /// The substituted parameters that are packs, of either kind (as "Types" and "Comps" name them): one standing as an
    /// element among others ("Tup[F, R]", "(f, r)") is spread into the elements it is bound to.
    std::vector<std::uint64_t> TypePacks;
    std::vector<std::uint64_t> CompPacks;

    SPP_ATTR_NODISCARD auto IsEmpty() const -> bool { return Types.empty() and Comps.empty(); }
  };

  /// "id" with "subst" applied, rebuilt as keying the substituted type would build it: an instantiation's arguments
  /// rewritten in place, a variant's members re-flattened, deduplicated and put back in order, and a convention kept
  /// over the one a substituted type carries. Memoised per ("id", "subst"). Null when the result is no type (a part
  /// did not resolve), or when a comp expression names a substituted parameter inside an operation, which only
  /// folding the expression can rewrite.
  SPP_EXP_FUN auto SubstituteTypeId(TypeId id, TypeSubst const &subst) -> TypeId;

  /// The outermost part of a "TypeId": what kind of type it is and what it is headed by.
  SPP_EXP_CLS struct TypeIdHead {
    /// "Self", "Param", "Bound", "Sym" (a closed class), "Inst", "Variant" or "Unresolved".
    InstanceKey::Tag Kind = InstanceKey::Tag::Unresolved;

    /// The convention tag, 0 for none.
    std::uint64_t Conv = 0;

    /// "Sym": the class; "Inst": the template; "Variant": the variant template.
    void const *Ptr = nullptr;

    /// "Param" and "Bound": the "ParamId".
    std::uint64_t ParamId = 0;

    /// "Inst": the arguments' key, interned as the template's "Instances" files it.
    TypeId Args = nullptr;

    /// "Variant": the members, in their canonical order.
    std::vector<TypeId> Members;
  };

  /// Read the outermost part of "id" ("TypeIdHead"). Memoised.
  SPP_EXP_FUN auto HeadOf(TypeId id) -> TypeIdHead const&;

  /// The parameter a written identity names ("Param", a type or comp
  /// parameter's "ParamId"), or 0.
  SPP_EXP_FUN auto ParamIdOf(TypeId id) -> std::uint64_t;

  /// The comp parameter a written identity names ("Comp", its identity
  /// "C<ParamId>"), or 0: "ParamIdOf" for a comp parameter.
  SPP_EXP_FUN auto CompParamIdOf(TypeId id) -> std::uint64_t;

  /// "CompParamIdOf" for a comp identity's interned text ("C<ParamId>"), as "TypeIdParams::Comps" lists them.
  SPP_EXP_FUN auto CompParamIdOfText(std::uint64_t text) -> std::uint64_t;

  /// A comp parameter's identity's interned text ("C<ParamId>"): the inverse of "CompParamIdOfText".
  SPP_EXP_FUN auto CompParamText(std::uint64_t param_id) -> std::uint64_t;

  /// "id" without its convention: the type a "TypeRef" holds under
  /// its own "Conv". Memoised.
  SPP_EXP_FUN auto BareTypeId(TypeId id) -> TypeId;

  /// The type parameters ("ParamId"s) "id" names anywhere, nested
  /// arguments and variant members included, and the comp
  /// parameters its comp arguments name (as "C<ParamId>" text ids).
  SPP_EXP_CLS struct TypeIdParams {
    std::vector<std::uint64_t> Types;
    std::vector<std::uint64_t> Comps;
  };

  /// "TypeIdParams" for "id". Memoised.
  SPP_EXP_FUN auto ParamsOf(TypeId id) -> TypeIdParams const&;

  /// One argument of an instantiation's identity ("TypeIdHead::Args"): named (its interned name) or positional (its
  /// index), then a type or a comp value's interned identity text.
  SPP_EXP_CLS struct TypeIdArg {
    bool Named = false;
    std::uint64_t Name = 0;
    TypeId Type = nullptr;
    std::uint64_t Comp = 0;
  };

  /// The elements' identities of a comp pack's identity ("P(C12, V1_uz)",
  /// "comp_generics::CompExprIdentity"), in order; nothing when it is not
  /// a pack's.
  SPP_EXP_FUN auto CompPackElements(StrView identity) -> std::optional<std::vector<StrView>>;

  /// The arguments an instantiation's arguments key lists, in order.
  SPP_EXP_FUN auto ArgsOf(TypeId args) -> std::vector<TypeIdArg>;
}
