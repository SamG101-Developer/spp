module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.type_key;
import spp.utils.interner;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct CompKey);
use(spp::analyse::scopes, struct TypeSymbol);

namespace spp::analyse::scopes {
  SPP_EXP_CLS struct TypeIdHead;
  SPP_EXP_CLS struct TypeIdParams;
  using Word = std::uint64_t;

  /// [CHECKED]
  /// A type key holds a vector of words. This function allows
  /// a "word" to be produced from a string, for the creation
  /// of the type key.
  SPP_EXP_FUN inline auto TextWord(const StrView text) -> Word {
    // Call the low level intern function and cast the result
    // from an "InternId" into a "Word" (raw, hashable number).
    return static_cast<Word>(utils::Intern(text));
  }

  /// [CHECKED]
  /// The inverse of the string-to-word interner. This allows
  /// for strings to be re-created off of interning, once all
  /// processing with interning has been completed.
  SPP_EXP_FUN inline auto TextOfWord(const Word word) -> StrView {
    // Call the low level reverse intern function with a word
    // cast to an InternId type, to recreate the string (view).
    return utils::InternedText(static_cast<utils::InternedId>(word));
  }

  SPP_EXP_CLS struct TypeKey;

  /// A "type id" is a pointer to an instance key, representing
  /// an interned type. They are the same id for the same type,
  /// meaningful within one compilation.
  SPP_EXP_CLS using TypeId = TypeKey const*;

  /// A "comp id" is a pointer to a comp node ("CompKey"),
  /// representing an interned comp value, as a "type id" is a
  /// pointer to an interned key: the same id for the same value,
  /// meaningful within one compilation. (Forward declared).
  SPP_EXP_CLS using CompId = CompKey const*;
  SPP_EXP_CLS struct TypeIdArg;

  /// Read the outermost part of "id" ("TypeIdHead"). Memoised.
  SPP_EXP_CLS SPP_ATTR_HOT auto HeadOf(
    TypeKey const *id) -> TypeIdHead const&;

  /// "id" without its convention: the type a "TypeRef" holds
  /// under its own "Conv". Memoised.
  SPP_EXP_CLS SPP_ATTR_HOT auto BareOf(
    TypeKey const *id) -> TypeKey const*;

  /// The arguments an instantiation's arguments key lists,
  /// in order.
  SPP_EXP_CLS auto ArgsOf(
    TypeKey const *args) -> std::vector<TypeIdArg>;

  /// The argument an instantiation's arguments key holds for the
  /// parameter "param_id", decoded as "ArgsOf" decodes it; read in
  /// place, stopping at the match. Only an argument for a parameter
  /// matches: a positional one (a pack's element, or one past the
  /// parameters) or a spelled one has no parameter, so none of its
  /// words is mistaken for one. Its kind is as found: "TypeVal" or
  /// "CompVal" is set, whichever the caller wants.
  SPP_EXP_CLS auto FindArgOf(
    TypeKey const *args, std::uint64_t param_id) -> std::optional<TypeIdArg>;

  /// The "type key" is an identity that a type instantiation
  /// is tagged as. It holds what each argument resolved to
  /// (as ids themselves) and their shapes.
  SPP_EXP_CLS struct TypeKey {
    /// A large set of possible types of a type, which might
    /// require certain behaviour and handling.
    enum class Tag : std::uint8_t {
      Name, Pos, Conv, Unresolved, TypeBound, TypeParam,
      Symbol, CompId, Inst, Variant, Len, TypeId, CompParam,
      TypePack, CompPack, Arg
    };

    /// The interned parts of the instance information. This
    /// might include things like generic argument interned
    /// information.
    std::vector<Word> Words;

    /// The "intern" of this entire instantiation. This is
    /// the master value used for things like comparisons.
    std::uint64_t Hash = 0;

    /// Whether any part of the key names something that did
    /// not resolve, which is no type at all: two such keys
    /// spelled alike are equal as keys, never as types.
    bool HasUnresolved = false;

    /// Whether any part of the key is "Self", parameter 0 ("TypeParam 0"): what it means depends on the scope reading
    /// it (its enclosing type) or a binding of 0. Not one of the parameters a key names ("ParamsNamedBy").
    bool HasSelf = false;

    /// What "HeadOf", "ParamsNamedBy" and "BareOf" read off an interned key, kept on it: an interned
    /// key never changes, so each is worked out once. Not part of the key; written only by those functions.
    ///   - the "head" of this type key (the template for this type, "Vec" for "Vec[S32]" etc);
    ///   - the generic params it names;
    ///   - the "bare" version of this type (with the convention removed).
    mutable TypeIdHead const *_CachedHead = nullptr;
    mutable TypeIdParams const *_CachedParams = nullptr;
    mutable TypeKey const *_CachedBare = nullptr;

    /// Push a tag with a small payload (an id, a position, a
    /// convention). This is the root payload push algorithm
    /// that should be wrapped. It itself uses "_Append" (raw).
    auto Push(const Tag tag, const std::uint64_t payload = 0) -> void {
      HasUnresolved = HasUnresolved or tag == Tag::Unresolved;
      HasSelf = HasSelf or (tag == Tag::TypeParam and payload == 0);
      _Append(static_cast<std::uint64_t>(tag) << 56 | payload);
    }

    /// Push a tag with text that is keyed by its spelling, by
    /// first interning the text and then pushing the id.
    auto PushText(const Tag tag, const StrView text) -> void {
      Push(tag, TextWord(text));
    }

    /// Append a type symbol's identity: its address, under the
    /// "Symbol" tag, as a number.
    auto PushSymbol(TypeSymbol const *sym) -> void {
      Push(Tag::Symbol);
      _Append(reinterpret_cast<std::uintptr_t>(sym));
    }

    /// Append an interned type ("TypeId") as one part of this
    /// key: the one word stands for its whole key, as interning
    /// makes equal keys one address.
    auto PushTypeId(const TypeId id) -> void {
      HasSelf = HasSelf or id->HasSelf;
      Push(Tag::TypeId);
      _Append(reinterpret_cast<std::uintptr_t>(id));
    }

    /// Append an interned comp ("CompId") as one part of this
    /// key: its tag, then its address, as "PushTypeId" does for
    /// a type; interning makes equal values one address.
    auto PushCompId(const CompId id) -> void {
      Push(Tag::CompId);
      _Append(reinterpret_cast<std::uintptr_t>(id));
    }

    /// Append a whole key, as one part of this one: its length
    /// first, so where it ends is part of the key.
    auto PushKey(TypeKey const &that) -> void {
      Push(Tag::Len, that.Words.size());
      _AppendKey(that);
    }

    /// Push another key spliced in: its words in place, with no
    /// tag or length, as though they had been pushed here, so
    /// this key continues as that one ("&" then the type it
    /// borrows; a key that is an existing type's, then more).
    auto PushSpliced(TypeKey const &that) -> void {
      _AppendKey(that);
    }

    /// Use push key for an unresolved part, or push type id
    /// if the part is resolved.
    auto PushTypePart(TypeId id) -> void;

    /// Use push comp id, and then update the self/resolved
    /// flags based on the member parts of the comp value.
    auto PushCompPart(CompId id) -> void;

    /// Equality is based on the hash and the words (numeric
    /// vector so not a heavy operation).
    auto operator==(TypeKey const &that) const -> bool {
      return Hash == that.Hash and Words == that.Words;
    }

    /// An order over keys, so a set of them can be put in one
    /// canonical order within a compilation.
    auto operator<(TypeKey const &that) const -> bool {
      return Words < that.Words;
    }

  private:
    friend auto HeadOf(TypeKey const *id) -> TypeIdHead const&;
    friend auto BareOf(TypeKey const *id) -> TypeKey const*;
    friend auto ArgsOf(TypeKey const *args) -> std::vector<TypeIdArg>;
    friend auto FindArgOf(TypeKey const *args, std::uint64_t param_id) -> std::optional<TypeIdArg>;

    /// The raw appending function, which takes the word, adds
    /// it into the vector, and updates the hash.
    auto _Append(const Word word) -> void {
      Words.push_back(word);
      Hash = (Hash ^ word) * 0x9E3779B97F4A7C15ull;
      Hash ^= Hash >> 29;
    }

    /// Append words already laid out as a key's, one part of
    /// another key copied as it stands: "unresolved" says whether
    /// they name something that did not resolve. Loop and append.
    auto _AppendWords(const std::span<const Word> words, const bool unresolved, const bool self = false) -> void {
      HasUnresolved = HasUnresolved or unresolved;
      HasSelf = HasSelf or self;
      for (const auto word : words) { _Append(word); }
    }

    /// Continue this key with another's words, as though they
    /// had been pushed here: the key of what a type stands for.
    auto _AppendKey(TypeKey const &that) -> void {
      HasUnresolved = HasUnresolved or that.HasUnresolved;
      HasSelf = HasSelf or that.HasSelf;
      for (const auto word : that.Words) { _Append(word); }
    }

    /// Extract some part of this type key's "words" into its
    /// own type key. Effectively the inverse of "PushTypePart".
    static auto _DecodePart(std::span<const Word> words, std::size_t &taken) -> TypeKey const*;

    /// Decode the argument at the front of an arguments key's
    /// "words" into "arg", and move "words" past it; false at
    /// the end, or on words cut short.
    static auto _DecodeArg(std::span<const Word> &words, TypeIdArg &arg) -> bool;
  };

  /// A key hashes as the hash it accumulated while it was built.
  /// No need to recalculate, and stable.
  SPP_EXP_CLS struct TypeKeyHash {
    auto operator()(TypeKey const &key) const noexcept -> std::uint64_t { return key.Hash; }
  };

  /// Convert a "type id" into a word, just by casting the
  /// address into a number. It is unique, so fine.
  SPP_EXP_FUN inline auto TypeIdWord(TypeId id) -> Word {
    return reinterpret_cast<std::uintptr_t>(id);
  }

  /// Convert a "word" into a "type id", by inverting the
  /// cast, reinterpreting it as the "type id" type.
  SPP_EXP_FUN inline auto TypeIdOfWord(const Word word) -> TypeId {
    return reinterpret_cast<TypeId>(static_cast<std::uintptr_t>(word));
  }

  /// Convert a "comp id" into a word, just by casting the
  /// address into a number, as for a "type id".
  SPP_EXP_FUN inline auto CompIdWord(const CompId id) -> Word {
    return reinterpret_cast<std::uintptr_t>(id);
  }

  /// Convert a "word" into a "comp id", by inverting the cast.
  SPP_EXP_FUN inline auto CompIdOfWord(const Word word) -> CompId {
    return reinterpret_cast<CompId>(static_cast<std::uintptr_t>(word));
  }

  /// Produce a "type id" from an "instance key".
  SPP_EXP_FUN SPP_ATTR_HOT inline auto InternTypeKey(TypeKey &&key) -> TypeId {
    static auto interned = StableSet<TypeKey, TypeKeyHash>();
    if (const auto hit = interned.find(key); hit != interned.end()) { return &*hit; }
    return &*interned.emplace(std::move(key)).first;
  }

  /// An order over keys that reads no address where anything
  /// else tells two keys apart: a symbol by its name and then
  /// the order it was made in ("TypeSymbol::Serial"), a nested
  /// type or comp identity by this same order, a spelling by
  /// its text, anything else by its value. Only keys alike in
  /// all of that fall back to their raw words, which keeps the
  /// order total. A variant's members are put in this order
  /// ("VariantKey"), so its tags do not follow where things
  /// happened to be allocated, and nothing it reads changes once
  /// made, so one set of members has one order.
  SPP_EXP_FUN auto StableKeyLess(
    TypeKey const &lhs, TypeKey const &rhs) -> bool;

  /// Given a generic parameter's id (on the symbol), get the
  /// type id being represented by it. 0 is "Self".
  SPP_EXP_FUN auto ParamTypeId(
    std::uint64_t param_id) -> TypeId;

  /// Whether "id" is "Self" itself (parameter 0), with or without a convention ("&Self").
  SPP_EXP_FUN auto IsSelfTypeId(
    TypeId id) -> bool;

  /// Given a generic parameter's id (on the symbol), get the
  /// comp id being represented by it.
  SPP_EXP_FUN auto ParamCompId(
    std::uint64_t param_id) -> CompId;

  /// The identity of the instantiation of "tmpl" whose arguments
  /// have the identity "args" ("Scope::ArgsIdOf"): an "Inst"
  /// head, then the arguments as one part.
  SPP_EXP_FUN inline auto InstanceIdOfArgs(
    TypeSymbol const &tmpl, const TypeId args) -> TypeId {
    auto key = TypeKey();
    key.Push(TypeKey::Tag::Inst);
    key.PushSymbol(&tmpl);
    key.PushKey(*args);
    return InternTypeKey(std::move(key));
  }

  /// The outermost part of a "TypeId", decoded: what kind of
  /// type it is, its convention, and that kind's payload (the
  /// class, the template and arguments, the variant members, or
  /// the parameter). Not the template itself: "&Vec[Str]" heads
  /// as an "&" "Inst" of "Vec" with the arguments "[Str]", and
  /// "Str" as a "Symbol" of "Str".
  SPP_EXP_CLS struct TypeIdHead {
    /// "Self", "TypeParam", "TypeBound", "Symbol" (a closed
    /// class), "Inst", "Variant" or "Unresolved".
    TypeKey::Tag Kind = TypeKey::Tag::Unresolved;

    /// The convention tag, 0 for none.
    std::uint64_t Conv = 0;

    /// "Symbol": the class; "Inst": the template; "Variant":
    /// the variant template.
    void const *Ptr = nullptr;

    /// "TypeParam" and "TypeBound": the type parameter's
    /// "ParamId".
    std::uint64_t TypeParamId = 0;

    /// "Inst": the arguments' key, interned as the template's
    /// "Instances" files it.
    TypeId Args = nullptr;

    /// "Variant": the members, in their canonical order.
    std::vector<TypeId> Members;

    /// "Ptr" as the symbol it is: the class ("Symbol"), the
    /// template ("Inst") or the variant template ("Variant").
    /// Null for any other kind.
    SPP_ATTR_NODISCARD auto Symbol() const -> TypeSymbol*;

    /// Whether this heads an instantiation: of a class template
    /// ("Inst"), or of the variant template ("Variant").
    SPP_ATTR_NODISCARD auto IsInstance() const -> bool;
  };

  /// The type and comp params interned to their symbols' param
  /// ids.
  SPP_EXP_CLS struct TypeIdParams {
    std::vector<std::uint64_t> TypeParams;
    std::vector<std::uint64_t> CompParams;
  };

  /// The type and comp params, as their param ids from their
  /// symbols, that are hidden inside the [type id]'s
  /// representative type. Memoised.
  SPP_EXP_FUN SPP_ATTR_HOT auto ParamsNamedBy(
    TypeId id) -> TypeIdParams const&;

  /// Whether "id" names a type or comp parameter anywhere
  /// in it ("ParamsNamedBy").
  SPP_EXP_FUN SPP_ATTR_HOT auto DoesTypeIdNameAnyGnParams(
    TypeId id) -> bool;

  /// Whether "id" is a concrete type: it names no parameter
  /// and no "Self", so nothing a binding gives it changes it.
  /// It may contain unresolved parts however, such as Vec[Foo]
  /// where Foo isn't a generic, but doesn't resolve.
  SPP_EXP_FUN auto IsConcreteTypeId(
    TypeId id) -> bool;

  /// A closed type is a concrete type with no unresolved
  /// parts left.
  SPP_EXP_FUN auto IsClosedTypeId(
    TypeId id) -> bool;

  /// One argument of an instantiation's identity
  /// ("TypeIdHead::Args"): for a parameter (by its "ParamId",
  /// "Arg"), or positional (by its index, "Pos": a pack's element,
  /// or one past the parameters), then a type or a comp value's
  /// interned identity. A keyword naming no parameter of the group
  /// is the one argument spelled ("Name"), an error the analysis
  /// reports.
  SPP_EXP_CLS struct TypeIdArg {
    /// For a parameter, or spelled: not positional.
    bool Named = false;

    /// Named by spelling rather than by a parameter.
    bool Spelled = false;

    /// The "ParamId" (for a parameter), the interned name (spelled), or the index (positional).
    std::uint64_t Slot = 0;

    TypeId TypeVal = nullptr;
    CompId CompVal = nullptr;
  };
}
