module;
#include <spp/macros.hpp>

module spp.analyse.scopes.type_key;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.asts.type_identifier_ast;
import spp.utils.interner;
import spp.utils.types;
import genex;
import std;

namespace spp::analyse::scopes {
  namespace {
    using Words = std::span<std::uint64_t const>;

    /// [CHECKED]
    auto TagOf(const std::uint64_t word) -> TypeKey::Tag {
      return static_cast<TypeKey::Tag>(word >> 56);
    }

    /// [CHECKED]
    auto PayloadOf(const std::uint64_t word) -> std::uint64_t {
      return word & ((std::uint64_t{1} << 56) - 1);
    }

    /// [CHECKED]
    auto WordsOf(const TypeId id) -> Words {
      return {id->Words.data(), id->Words.size()};
    }

    /// [CHECKED]
    /// Whether raw key words name a "Self" anywhere. Check
    /// the different type key tags, and perform the required
    /// behaviour.
    auto WordsNameSelf(const Words words) -> bool {
      for (auto i = 0uz; i < words.size(); ++i) {
        switch (TagOf(words[i])) {
          // "Self" is parameter 0: the root truth.
          case TypeKey::Tag::TypeParam:
            if (PayloadOf(words[i]) == 0) { return true; }
            break;

          // For a type id, use the "HasSelf" flag to check
          // as its been set during construction.
          case TypeKey::Tag::TypeId:
            if (i + 1 < words.size() and TypeIdOfWord(words[i + 1])->HasSelf) { return true; }
            ++i;
            break;

          // For a comp id, check its members (static access)
          // for a "Self" owner type (ie Self::n).
          case TypeKey::Tag::CompId:
            if (i + 1 < words.size()) {
              if (const auto comp = CompIdOfWord(words[i + 1]); comp != nullptr) {
                for (auto const &[type, name] : MembersNamedBy(comp)) {
                  if (TypeIdOfWord(type)->HasSelf) { return true; }
                }
              }
            }
            ++i;
            break;

          // No work needed for a symbol.
          case TypeKey::Tag::Symbol:
            ++i;
            break;
          default:
            break;
        }
      }
      return false;
    }

    /// [CHECKED]
    auto StableCompare(
      Words lhs, Words rhs) -> std::strong_ordering;

    /// [CHECKED]
    /// "StableKeyLess" for two interned types, and for two type
    /// words.
    auto StableCompareIds(
      const TypeId lhs, const TypeId rhs) -> std::strong_ordering {
      return lhs == rhs
        ? std::strong_ordering::equal
        : StableCompare(WordsOf(lhs), WordsOf(rhs));
    }

    /// [CHECKED]
    /// "StableKeyLess" for two comp identities: part by part,
    /// a value by its value and a constant's type by its own
    /// order. Interned, so two that are alike in all of that
    /// are one node, unless a type in them ties on its name.
    auto StableCompareNodes(
      CompKey const &lhs, CompKey const &rhs) -> std::strong_ordering {
      if (&lhs == &rhs) { return std::strong_ordering::equal; }
      if (const auto c = lhs.Kind <=> rhs.Kind; c != 0) { return c; }
      if (const auto c = lhs.Text <=> rhs.Text; c != 0) { return c; }
      if (const auto c = lhs.ParamId <=> rhs.ParamId; c != 0) { return c; }
      if (lhs.Type != rhs.Type) {
        if (const auto c = StableCompareIds(TypeIdOfWord(lhs.Type), TypeIdOfWord(rhs.Type)); c != 0) { return c; }
      }
      if (const auto c = lhs.Val.index() <=> rhs.Val.index(); c != 0) { return c; }
      if (const auto l = lhs.AsInt(); l != nullptr) {
        if (const auto c = *l <=> *rhs.AsInt(); c != 0) { return c; }
      }
      if (const auto l = lhs.AsFloat(); l != nullptr) {
        if (const auto c = *l <=> *rhs.AsFloat(); c != 0) { return c; }
      }
      if (const auto l = lhs.AsBool(); l != nullptr) {
        if (const auto c = *l <=> *rhs.AsBool(); c != 0) { return c; }
      }
      for (auto i = 0uz; i < lhs.Kids.size() and i < rhs.Kids.size(); ++i) {
        if (const auto c = StableCompareNodes(*lhs.Kids[i], *rhs.Kids[i]); c != 0) { return c; }
      }
      return lhs.Kids.size() <=> rhs.Kids.size();
    }

    /// [CHECKED]
    /// Two symbol words by what is fixed when the symbols are
    /// made: the name, then which was made first. Not the
    /// qualified name, which changes when a scope is re-parented:
    /// a variant keyed before and after would split into two
    /// identities. Every pointer a key holds after "Symbol" is
    /// a type symbol's.
    auto StableCompareSymbols(
      const std::uint64_t lhs, const std::uint64_t rhs) -> std::strong_ordering {
      const auto l = reinterpret_cast<TypeSymbol const*>(static_cast<std::uintptr_t>(lhs));
      const auto r = reinterpret_cast<TypeSymbol const*>(static_cast<std::uintptr_t>(rhs));
      const auto l_name = l->Name != nullptr ? l->Name->ToView() : StrView();
      const auto r_name = r->Name != nullptr ? r->Name->ToView() : StrView();
      if (const auto c = l_name <=> r_name; c != 0) { return c; }
      return l->Serial <=> r->Serial;
    }

    /// [CHECKED]
    /// Stable comparison for the words (list of interned words),
    /// which maps into the other two stable sorting functions.
    auto StableCompare(const Words lhs, const Words rhs) -> std::strong_ordering {
      // The two keys are walked together while they are alike,
      // so their raw words line up: a pointer follows only a
      // "Symbol", "TypeId" or "CompId" tag.
      for (auto i = 0uz; i < lhs.size() and i < rhs.size(); ++i) {
        const auto tag = TagOf(lhs[i]);
        // When the two numbers don't line up, then compare the
        // text if they are both spellable, otherwise the original
        // number comparison.
        if (lhs[i] != rhs[i]) {
          const auto spelled = tag == TypeKey::Tag::Name or tag == TypeKey::Tag::Unresolved or tag == TypeKey::Tag::TypeBound;
          if (spelled and TagOf(rhs[i]) == tag) {
            return TextOfWord(PayloadOf(lhs[i])) <=> TextOfWord(PayloadOf(rhs[i]));
          }
          // An argument's parameter is ordered by its name, as it was when keys spelled it: a "ParamId" is minted in
          // the order things were analysed, which is no order to give a variant's members.
          if (tag == TypeKey::Tag::Arg and TagOf(rhs[i]) == tag) {
            return spp::analyse::scopes::ParamNameOf(PayloadOf(lhs[i])) <=> spp::analyse::scopes::ParamNameOf(PayloadOf(rhs[i]));
          }
          return lhs[i] <=> rhs[i];
        }

        // Only symbols, type ids and comp ids are comparable, so
        // if the tags aren't one of those, then continue to the
        // next part.
        if (tag != TypeKey::Tag::Symbol and tag != TypeKey::Tag::TypeId and tag != TypeKey::Tag::CompId) { continue; }
        if (++i == lhs.size() or i == rhs.size() or lhs[i] == rhs[i]) { continue; }
        const auto c = tag == TypeKey::Tag::Symbol
          ? StableCompareSymbols(lhs[i], rhs[i])
          : tag == TypeKey::Tag::TypeId
          ? StableCompareIds(TypeIdOfWord(lhs[i]), TypeIdOfWord(rhs[i]))
          : StableCompareNodes(*CompIdOfWord(lhs[i]), *CompIdOfWord(rhs[i]));
        if (c != 0) { return c; }
      }

      // The shorter first, and then, as a last resort, the raw
      // words.
      if (const auto c = lhs.size() <=> rhs.size(); c != 0) { return c; }
      return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }
  }
}

SPP_MOD_BEGIN
/// [CHECKED]
auto spp::analyse::scopes::HeadOf(
  const TypeId id) -> TypeIdHead const& {
  // Use the internally cached head on the type id, if it
  // exists, to get the head. Define the "remember" lambda
  // that memoises the head for the type id.
  static auto memo = StableMap<TypeId, TypeIdHead>();
  if (id != nullptr and id->_CachedHead != nullptr) { return *id->_CachedHead; }
  const auto remember = [id](TypeIdHead &&answer) -> TypeIdHead const& {
    auto const &kept = memo.emplace(id, std::move(answer)).first->second;
    if (id != nullptr) { id->_CachedHead = &kept; }
    return kept;
  };

  // Define an empty head to fill, and the "words" which is
  // the pair [id-words-data, id-words-size].
  auto head = TypeIdHead();
  auto words = WordsOf(id);

  // If there is a convention present, extract it from the
  // words and copy it into the head. The convention is part
  // of the head: "&Vec[Str]" heads as an "&" "Inst" of "Vec".
  if (not words.empty() and TagOf(words[0]) == TypeKey::Tag::Conv) {
    head.Conv = PayloadOf(words[0]);
    words = words.subspan(1);
  }

  // If there are no words, then return the empty head at
  // this point. Still "remember" it, for the symbol.
  if (words.empty()) { return remember(std::move(head)); }

  // The potential convention has been removed, so the 0th
  // words value is the "kind". Save and bind it, then switch
  // on it.
  head.Kind = TagOf(words[0]);
  switch (head.Kind) {
    // For a type parameter, just get the payload of the word
    // and assign it as the type param id property on the
    // head.
    case TypeKey::Tag::TypeParam:
    case TypeKey::Tag::TypeBound:
      head.TypeParamId = PayloadOf(words[0]);
      break;

    // If a symbol was interned into the slot, then assign to
    // the pointer field, via some reinterpret casts. The two
    // fields are the tag and the symbol pointer following it.
    case TypeKey::Tag::Symbol:
      if (words.size() >= 2) { head.Ptr = reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[1])); }
      break;

    // For an instantiation, we need 4+ words, for the inst
    // tag, the symbol tag, the symbol pointer, the length
    // tag, followed by the arguments (can be none for len=0).
    case TypeKey::Tag::Inst:
      if (words.size() >= 4 and TagOf(words[3]) == TypeKey::Tag::Len) {
        head.Ptr = reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[2]));
        auto args = TypeKey();
        args._AppendWords(words.subspan(4, PayloadOf(words[3])), id->HasUnresolved, id->HasSelf);
        head.Args = InternTypeKey(std::move(args));
      }
      break;

    // For a variant, we need 3+ words, for the variant tag,
    // the symbol tag, the variant's template pointer, followed
    // by 0+ members. Each variant member is a "type part".
    case TypeKey::Tag::Variant:
      if (words.size() >= 3) {
        head.Ptr = reinterpret_cast<void const*>(static_cast<std::uintptr_t>(words[2]));
        for (auto rest = words.subspan(3); not rest.empty();) {
          auto taken = 0uz;
          head.Members.push_back(TypeKey::_DecodePart(rest, taken));
          rest = rest.subspan(taken);
        }
      }
      break;

    // No behaviour for anything else (no possibilities).
    default:
      break;
  }

  // Cache the value and return it.
  return remember(std::move(head));
}

/// [CHECKED]
auto spp::analyse::scopes::BareOf(
  const TypeId id) -> TypeId {
  // For a type id that hasn't got a convention already, no
  // work needs to be done, so return the id as it is.
  if (id == nullptr or id->Words.empty()) { return id; }
  if (TagOf(id->Words[0]) != TypeKey::Tag::Conv) { return id; }

  // Use the internally cached "bare" on the type id, if it
  // exists, to get the bare. Define the "remember" lambda
  // that memoises the bare for the type id.
  if (id->_CachedBare != nullptr) { return id->_CachedBare; }

  // Define an empty bare to fill, and the "words" which is
  // the pair [id-words-data, id-words-size] (parts taken
  // after the convention).
  auto key = TypeKey();
  key._AppendWords(
    WordsOf(id).subspan(1), id->HasUnresolved, id->HasSelf);
  id->_CachedBare = InternTypeKey(std::move(key));
  return id->_CachedBare;
}

SPP_MOD_END

/// [CHECKED]
auto spp::analyse::scopes::ParamsNamedBy(const TypeId id) -> TypeIdParams const& {
  // For a type id that hasn't been param-extracted already,
  // no work needs to be done, so return the ids as they are.
  static auto memo = StableMap<TypeId, TypeIdParams>();
  if (id != nullptr and id->_CachedParams != nullptr) { return *id->_CachedParams; }
  if (const auto hit = memo.find(id); hit != memo.end()) { return hit->second; }

  // Define an empty bare to fill, and the "words" which is
  // the pair [id-words-data, id-words-size].
  auto out = TypeIdParams();
  const auto add = [](std::vector<std::uint64_t> &into, const std::uint64_t x) {
    if (not genex::contains(into, x)) { into.push_back(x); }
  };

  // Iterate through each part of the word set. We can then
  // check the tag of each part. Example: Type[T, Str, n, 1]
  // where T and n are generic parameters from the enclosing
  // context (function).
  const auto words = WordsOf(id);
  for (auto i = 0uz; i < words.size(); ++i) {
    switch (TagOf(words[i])) {
      // For a type parameter, add the payload of the current
      // word. This would be if "T" was seen in the example,
      // via the TypeId for "T" (see the recursive call to the
      // ParamsNamedBy function).
      case TypeKey::Tag::TypeParam:
        if (PayloadOf(words[i]) != 0) { add(out.TypeParams, PayloadOf(words[i])); }
        break;

      // For type bound, do the same as a type param, but skip
      // the second part of it, which is not needed (spelling).
      case TypeKey::Tag::TypeBound:
        add(out.TypeParams, PayloadOf(words[i]));
        ++i;
        break;

      // Skip the symbol parts, and the symbol itself, as this
      // is not needed for getting the params.
      case TypeKey::Tag::Symbol:
        ++i;
        break;

      // For a TypeId, either T or Str, look into the generic
      // params represented by it. For T, it'll find TypeParam;
      // and for Str, a symbol => nothing to record.
      case TypeKey::Tag::TypeId: {
        auto const &inner = ParamsNamedBy(TypeIdOfWord(words[i + 1]));
        for (const auto t : inner.TypeParams) { add(out.TypeParams, t); }
        for (const auto c : inner.CompParams) { add(out.CompParams, c); }
        ++i;
        break;
      }

      // Slightly special because this is why we don't need to
      // have CompParam here. Both "n" and "1" come into here,
      // like "T" and "Str" go to TypeId, but the ParamsNamedBy
      // overload for the comp id, extracts the actual params.
      case TypeKey::Tag::CompId: {
        const auto node = CompIdOfWord(words[i + 1]);
        ++i;
        if (node == nullptr) { break; }

        // Get the comp params being named by the node, which
        // will be "n".
        for (const auto param_id : ParamsNamedBy(node)) { add(out.CompParams, param_id); }

        for (auto const &[type, _] : MembersNamedBy(node)) {
          auto const &inner = ParamsNamedBy(TypeIdOfWord(type));
          for (const auto t : inner.TypeParams) { add(out.TypeParams, t); }
          for (const auto comp : inner.CompParams) { add(out.CompParams, comp); }
        }
        break;
      }
      default:
        break;
    }
  }

  // Cache the result, and return it.
  auto const &kept = memo.emplace(id, std::move(out)).first->second;
  if (id != nullptr) { id->_CachedParams = &kept; }
  return kept;
}

/// [CHECKED]
auto spp::analyse::scopes::DoesTypeIdNameAnyGnParams(
  const TypeId id) -> bool {
  // Get the params under the type id, and check if
  // there are any present.
  auto const &params = ParamsNamedBy(id);
  return not params.TypeParams.empty() or not params.CompParams.empty();
}

/// [CHECKED]
auto spp::analyse::scopes::StableKeyLess(
  TypeKey const &lhs, TypeKey const &rhs) -> bool {
  // The comparison uses the "stable compare" algorithm,
  // checking against "0".
  return StableCompare(
    {lhs.Words.data(), lhs.Words.size()},
    {rhs.Words.data(), rhs.Words.size()}) < 0;
}

/// [CHECKED]
auto spp::analyse::scopes::ParamTypeId(
  const std::uint64_t param_id) -> TypeId {
  // Create a type key, push the param id into it under
  // the "type parameter" tag. Intern the key for caching.
  auto key = TypeKey();
  key.Push(TypeKey::Tag::TypeParam, param_id);
  return InternTypeKey(std::move(key));
}

auto spp::analyse::scopes::IsSelfTypeId(
  const TypeId id) -> bool {
  if (id == nullptr) { return false; }
  auto const &head = spp::analyse::scopes::HeadOf(id);
  return head.Kind == TypeKey::Tag::TypeParam and head.TypeParamId == 0;
}

/// [CHECKED]
auto spp::analyse::scopes::ParamCompId(
  const std::uint64_t param_id) -> CompId {
  // Create comp key from the param id and intern it for
  // caching.
  return InternCompKey(CompKey::OfParam(param_id));
}

/// [CHECKED]
auto spp::analyse::scopes::IsConcreteTypeId(
  const TypeId id) -> bool {
  // A type is "concrete" if it has no generic parameters
  // ("Self" treated as generic). It can have unresolved
  // parts however.
  return id != nullptr and not id->HasSelf and not DoesTypeIdNameAnyGnParams(id);
}

/// [CHECKED]
auto spp::analyse::scopes::IsClosedTypeId(
  const TypeId id) -> bool {
  // A closed type is a concrete type that doesn't have any
  // unresolved parts.
  return IsConcreteTypeId(id) and not id->HasUnresolved;
}

SPP_MOD_BEGIN
/// [CHECKED]
auto spp::analyse::scopes::ArgsOf(
  const TypeId args) -> std::vector<TypeIdArg> {
  // Create the empty list of what the arg ids will fill
  // up, then decode each argument in turn.
  auto out = std::vector<TypeIdArg>();
  if (args == nullptr) { return out; }
  auto words = WordsOf(args);
  for (auto arg = TypeIdArg(); TypeKey::_DecodeArg(words, arg); arg = TypeIdArg()) {
    out.push_back(arg);
  }
  return out;
}

/// [CHECKED]
auto spp::analyse::scopes::FindArgOf(
  const TypeId args, const std::uint64_t param_id) -> std::optional<TypeIdArg> {
  // Decode the arguments one at a time, stopping at the first
  // one for the parameter.
  if (args == nullptr) { return std::nullopt; }
  auto words = WordsOf(args);
  for (auto arg = TypeIdArg(); TypeKey::_DecodeArg(words, arg); arg = TypeIdArg()) {
    if (arg.Named and not arg.Spelled and arg.Slot == param_id) { return arg; }
  }
  return std::nullopt;
}

/// [CHECKED]
auto spp::analyse::scopes::TypeKey::_DecodeArg(
  std::span<const Word> &words, TypeIdArg &arg) -> bool {
  // The parameter (or spelled name, or position) word, then the
  // value. Cut short after the name, nothing is decoded.
  if (words.empty()) { return false; }
  arg.Named = TagOf(words[0]) == Tag::Arg or TagOf(words[0]) == Tag::Name;
  arg.Spelled = TagOf(words[0]) == Tag::Name;
  arg.Slot = PayloadOf(words[0]);
  words = words.subspan(1);
  if (words.empty()) { return false; }

  // Comp arg.
  if (TagOf(words[0]) == Tag::CompId) {
    if (words.size() < 2) { return false; }
    arg.CompVal = CompIdOfWord(words[1]);
    words = words.subspan(2);
    return true;
  }

  // Type arg.
  auto taken = 0uz;
  arg.TypeVal = _DecodePart(words, taken);
  words = words.subspan(taken);
  return true;
}

/// [CHECKED]
auto spp::analyse::scopes::TypeKey::PushTypePart(
  const TypeId id)
  -> void {
  // If the part is not resolved, push its key inline, otherwise
  // push its id.
  if (id->HasUnresolved) { PushKey(*id); }
  else { PushTypeId(id); }
}

/// [CHECKED]
auto spp::analyse::scopes::TypeKey::PushCompPart(
  const CompId id) -> void {
  // Push the comp id in, and then do a recursive "self" and
  // "unresolved" flag check.
  PushCompId(id);
  if (id == nullptr) { return; }
  for (auto const &member : MembersNamedBy(id)) { HasSelf = HasSelf or TypeIdOfWord(member.first)->HasSelf; }
  HasUnresolved = HasUnresolved or not IsReadableCompId(id);
}

/// [CHECKED]
auto spp::analyse::scopes::TypeKey::_DecodePart(
  const std::span<const Word> words,
  std::size_t &taken) -> TypeKey const* {
  // If we are looking at a type id, update the mutable
  // reference and return the type key found there.
  if (TagOf(words[0]) == TypeKey::Tag::TypeId) {
    taken = 2;
    return TypeIdOfWord(words[1]);
  }

  // Otherwise, get the length of the payload, set the
  // "taken" to that + 1 (cover the tag).
  const auto len = PayloadOf(words[0]);
  taken = 1 + len;

  // Create a key and append all the leftover words
  // into it, with a "Self" check. Intern the result.
  auto key = TypeKey();
  key._AppendWords(words.subspan(1, len), true, WordsNameSelf(words.subspan(1, len)));
  return InternTypeKey(std::move(key));
}

/// [CHECKED]
auto spp::analyse::scopes::TypeIdHead::Symbol() const -> TypeSymbol* {
  // If this is a symbol or instantiation or variant,
  // then return from the ptr (cast).
  const auto names_sym = Kind == TypeKey::Tag::Symbol or IsInstance();
  return names_sym
    ? const_cast<TypeSymbol*>(static_cast<TypeSymbol const*>(Ptr))
    : nullptr;
}

/// [CHECKED]
auto spp::analyse::scopes::TypeIdHead::IsInstance() const -> bool {
  // An instantiation or variant is an instance.
  return Kind == TypeKey::Tag::Inst or Kind == TypeKey::Tag::Variant;
}

SPP_MOD_END
