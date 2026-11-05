module;
#include <spp/macros.hpp>

module spp.analyse.scopes.substitution;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.utils.ice;
import spp.utils.types;
import genex;
import std;

namespace spp::analyse::scopes {
  namespace {
    /// A memo entry's key: an interned identity and the
    /// interned substitution ("SubstId"), a pair of pointers.
    struct MemoHash {
      using is_avalanching = void;

      template <typename Id>
      auto operator()(Pair<Id, TypeId> const &p) const noexcept -> std::uint64_t {
        const auto first = reinterpret_cast<std::uintptr_t>(p.first);
        const auto second = reinterpret_cast<std::uintptr_t>(p.second);
        return Hash<std::uint64_t>()(first * 0x9E3779B97F4A7C15ull ^ second);
      }
    };

    /// [CHECKED]
    /// The cache for generic substitution for types.
    auto TypeMemo() -> Map<Pair<TypeId, TypeId>, TypeId, MemoHash>& {
      static auto memo = Map<Pair<TypeId, TypeId>, TypeId, MemoHash>();
      return memo;
    }

    /// [CHECKED]
    /// The cache for generic substitution for comps.
    auto CompMemo() -> Map<Pair<CompId, TypeId>, CompId, MemoHash>& {
      static auto memo = Map<Pair<CompId, TypeId>, CompId, MemoHash>();
      return memo;
    }

    /// [CHECKED]
    /// Find the TypeId from the type map within the "subst",
    /// indexed by "param" (if one exists).
    auto BoundTypeOf(GenericSubst const &subst, const std::uint64_t param) -> TypeId {
      const auto it = genex::find(subst.TypeParams, param, &Pair<std::uint64_t, TypeId>::first);
      return it != subst.TypeParams.end() ? it->second : nullptr;
    }

    /// [CHECKED]
    /// Find the TypeId from the comp map within the "subst",
    /// indexed by "param" (if one exists).
    auto BoundCompOf(GenericSubst const &subst, const std::uint64_t param) -> CompId {
      const auto it = genex::find(subst.CompParams, param, &Pair<std::uint64_t, CompId>::first);
      return it != subst.CompParams.end() ? it->second : nullptr;
    }

    auto RewriteTypeId(TypeId id, GenericSubst const &subst, TypeId subst_id) -> TypeId;
    auto RewriteCompId(CompId id, GenericSubst const &subst, TypeId subst_id) -> CompId;

    /// [CHECKED]
    /// An instantiation's arguments ("ArgsOf") rewritten, keyed as "Scope::ArgsIdOf" keys them: each named or
    /// positional, a type part or a comp identity ("RewriteCompId"). A type pack's argument is its list, which splices
    /// the packs bound in it ("RewriteTypeId").
    auto RewriteArgsId(const TypeId args, GenericSubst const &subst, const TypeId subst_id) -> TypeId {
      auto out = TypeKey();
      // From the interned type id of the args, pull the type
      // id args out and iterate them.
      for (auto const &arg : ArgsOf(args)) {
        const auto tag = arg.Named ? TypeKey::Tag::Arg : TypeKey::Tag::Pos;
        const auto at = arg.Slot;

        // For a generic comp arg, rewrite the comp id, and
        // push the tag+name and value. Handles positional,
        // keyword+name, and the value.
        if (arg.CompVal != nullptr) {
          const auto value = RewriteCompId(arg.CompVal, subst, subst_id);
          if (value == nullptr) { return nullptr; }
          out.Push(tag, at);
          out.PushCompPart(value);
          continue;
        }

        // A type argument: a standard rewrite, and tag+name and
        // part push.
        const auto part = RewriteTypeId(arg.TypeVal, subst, subst_id);
        if (part == nullptr) { return nullptr; }
        out.Push(tag, at);
        out.PushTypePart(part);
      }
      return InternTypeKey(std::move(out));
    }

    /// [CHECKED]
    /// A type identity rewritten part by part under "subst"
    /// (the type twin of "RewriteCompId"), memoised per ("id",
    /// "subst") once nothing in it is unresolved. Branches on
    /// the kind of its head ("HeaderOf").
    auto RewriteTypeId(const TypeId id, GenericSubst const &subst, const TypeId subst_id) -> TypeId {
      auto &memo = TypeMemo();
      if (const auto hit = memo.find({id, subst_id}); hit != memo.end()) { return hit->second; }

      const auto rewritten = [&]() -> TypeId {
        auto const &head = HeaderOf(id);
        switch (head.Kind) {
          // For type parameters ("Self" is 0), use the bound type
          // function so that the cache is reused (indexed with
          // interning), otherwise return the id back. Handle the
          // convention if there is one.
          case TypeKey::Tag::TypeParam:
          case TypeKey::Tag::TypeBound: {
            const auto bound = BoundTypeOf(subst, head.TypeParamId);
            if (bound == nullptr) { return id; }
            if (head.Conv == 0) { return bound; }

            // A convention written on the parameter ("&T") is kept
            // over the one the bound type carries.
            auto key = TypeKey();
            key.Push(TypeKey::Tag::Conv, head.Conv);
            key.PushSpliced(*BareOf(bound));
            return InternTypeKey(std::move(key));
          }

          // An empty key is no type, as a real unresolved one marks
          // itself.
          case TypeKey::Tag::Unresolved:
            return id->HasUnresolved ? id : nullptr;

          case TypeKey::Tag::Symbol:
            return id;

          // For an instantiation, push the convention, the tag,
          // the template symbol (off the head), and finally the
          // args, themselves re-written. Intern the result.
          case TypeKey::Tag::Inst: {
            if (head.Symbol() == nullptr or head.Args == nullptr) {
              Ice("instantiation identity has no template or arguments");
            }
            const auto args = RewriteArgsId(head.Args, subst, subst_id);
            if (args == nullptr) { return nullptr; }
            const auto inst = InstanceIdOfArgs(*head.Symbol(), args);
            if (head.Conv == 0) { return inst; }
            auto key = TypeKey();
            key.Push(TypeKey::Tag::Conv, head.Conv);
            key.PushSpliced(*inst);
            return InternTypeKey(std::move(key));
          }

          // For variants, push the tag, the convention, the symbol,
          // and finally the members. Intern the result. The members
          // are flattened, deduplicated and stable-sorted.
          case TypeKey::Tag::Variant: {
            if (head.Symbol() == nullptr) { Ice("variant identity has no template symbol"); }
            auto members = Vec<TypeId>();
            for (const auto written : head.VariantMembers) {
              const auto member = RewriteTypeId(written, subst, subst_id);
              if (member == nullptr) { return nullptr; }
              auto const &member_head = HeaderOf(member);

              // A pack member ("Var[Variants]") bound to its list is the list's elements.
              const auto flat = member_head.Kind == TypeKey::Tag::Pack
                ? member_head.PackElements
                : member_head.Kind == TypeKey::Tag::Variant and member_head.Conv == 0
                ? member_head.VariantMembers
                : Vec{member};
              for (const auto m : flat) {
                if (not genex::contains(members, m)) { members.push_back(m); }
              }
            }
            members |= genex::actions::sort([](const TypeId a, const TypeId b) { return StableKeyLess(*a, *b); });
            auto key = TypeKey();
            if (head.Conv != 0) { key.Push(TypeKey::Tag::Conv, head.Conv); }
            key.Push(TypeKey::Tag::Variant);
            key.PushSymbol(head.Symbol());
            for (const auto m : members) { key.PushTypePart(m); }
            return InternTypeKey(std::move(key));
          }

          // A type pack's list: each element rewritten, one bound to
          // a list of its own ("Tup[A, Ts]" with "Ts" bound to "B, C")
          // spliced in.
          case TypeKey::Tag::Pack: {
            auto elems = Vec<TypeId>();
            for (const auto written : head.PackElements) {
              const auto elem = RewriteTypeId(written, subst, subst_id);
              if (elem == nullptr) { return nullptr; }
              if (HeaderOf(elem).Kind == TypeKey::Tag::Pack) { elems.append_range(HeaderOf(elem).PackElements); }
              else { elems.push_back(elem); }
            }
            return PackIdOf(elems);
          }
          default:
            return nullptr;
        }
      }();

      // A part that cannot be read yet (a constant whose "sup"
      // block is not attached marks the key unresolved), or
      // rewritten at all, is rewritten again next time, as the
      // comp side's is.
      if (rewritten != nullptr and not rewritten->HasUnresolved) {
        memo.emplace(Pair<TypeId, TypeId>{id, subst_id}, rewritten);
      }
      return rewritten;
    }

    /// [CHECKED]
    /// A comp identity rewritten part by part under "subst"
    /// (the comp twin of "RewriteTypeId"), memoised per ("id",
    /// "subst") once nothing in it is unresolved. Include
    /// folding, and binary operator collapsing.
    auto RewriteCompId(const CompId id, GenericSubst const &subst, const TypeId subst_id) -> CompId {
      auto &memo = CompMemo();
      if (const auto hit = memo.find({id, subst_id}); hit != memo.end()) { return hit->second; }

      const auto rewritten = [&]() -> CompId {
        switch (id->Kind) {
          // A value (literal) or opaque expression cannot be
          // substituted, so return the id that was sent in.
          case CompKey::Part::Value:
          case CompKey::Part::Opaque:
            return id;

          // For members of a type, substitute the type with
          // the generics. If the type is now closed, get the
          // cmp member off of it. Otherwise, intern the member
          // access.
          case CompKey::Part::Member: {
            const auto owner = SubstituteTypeId(TypeIdOfWord(id->Type), subst);
            if (owner == nullptr) { return nullptr; }
            if (IsClosedTypeId(owner)) {
              if (const auto read = utils::comp_generics::FindCompMemberId(owner, id->Text); read != nullptr) {
                return read;
              }
            }
            return InternCompKey(CompKey::OfMember(TypeIdWord(owner), id->Text));
          }

          // For a comp param part, call the bound function to
          // pull it out the cache map.
          case CompKey::Part::Param: {
            const auto bound = BoundCompOf(subst, id->ParamId);
            return bound != nullptr ? bound : id;
          }

          // For a comp pack's list, or a tuple, rewrite each
          // element; one bound to a pack's list is spliced in, as
          // a type pack's is ("RewriteTypeId").
          case CompKey::Part::Pack:
          case CompKey::Part::Tuple: {
            auto elems = Vec<CompId>();
            for (const auto elem : id->Kids) {
              const auto value = RewriteCompId(elem, subst, subst_id);
              if (value == nullptr) { return nullptr; }
              if (value->Kind == CompKey::Part::Pack) {
                elems.insert(elems.end(), value->Kids.begin(), value->Kids.end());
                continue;
              }
              elems.push_back(value);
            }
            return InternCompKey(id->Kind == CompKey::Part::Pack
              ? CompKey::OfPack(std::move(elems))
              : CompKey::OfTuple(std::move(elems)));
          }

          // For the binary operator, rewrite the left- and
          // right-hand-side operands, and fold them with the
          // operator. Intern the result.
          case CompKey::Part::Op: {
            const auto lhs = RewriteCompId(id->Kids[0], subst, subst_id);
            const auto rhs = RewriteCompId(id->Kids[1], subst, subst_id);
            if (lhs == nullptr or rhs == nullptr) { return nullptr; }
            if (lhs->Kind == CompKey::Part::Value and rhs->Kind == CompKey::Part::Value) {
              if (auto folded = FoldCompValues(id->Text, *lhs, *rhs); folded.has_value()) {
                return InternCompKey(std::move(*folded));
              }
            }
            return InternCompKey(CompKey::OfOp(id->Text, lhs, rhs));
          }

          default:
            return nullptr;
        }
      }();

      // A constant that cannot be read yet is read again next
      // time: its "sup" block may be attached by then.
      if (IsReadableCompId(rewritten)) { memo.emplace(Pair<CompId, TypeId>{id, subst_id}, rewritten); }
      return rewritten;
    }

    /// [CHECKED]
    /// The "subst" as one interned key, so a memo entry is a
    /// pair of pointers.
    auto SubstId(GenericSubst const &subst) -> TypeId {
      // Extract the type and comp parameters from the
      // substitution object, and sort them.
      auto types = subst.TypeParams;
      auto comps = subst.CompParams;
      types |= genex::actions::sort;
      comps |= genex::actions::sort;

      // Create an empty type key, and push the type
      // parameters, and then the comp parameters, into it.
      auto key = TypeKey();
      for (auto const &[param, type] : types) {
        key.Push(TypeKey::Tag::TypeParam, param);
        key.PushTypeId(type);
      }
      for (auto const &[param, value] : comps) {
        key.Push(TypeKey::Tag::CompParam, param);
        key.PushCompId(value);
      }

      // Finally, push the type and comp pack parameters
      // into it, for the variadic management. Intern
      // the result.
      return InternTypeKey(std::move(key));
    }
  }
}

/// [CHECKED]
auto spp::analyse::scopes::SubstituteTypeId(
  const TypeId id, GenericSubst const &subst) -> TypeId {
  // Rewrite the substitution into the "id".
  if (id == nullptr) { return nullptr; }
  if (subst.IsEmpty()) { return id->HasUnresolved ? nullptr : id; }
  const auto result = RewriteTypeId(id, subst, SubstId(subst));
  return result == nullptr or result->HasUnresolved ? nullptr : result;
}

/// [CHECKED]
auto spp::analyse::scopes::SubstituteCompId(
  const CompId id, GenericSubst const &subst) -> CompId {
  // Rewrite the substitution into the "id".
  if (id == nullptr) { return nullptr; }
  return RewriteCompId(id, subst, SubstId(subst));
}

auto spp::analyse::scopes::ClearSubstitutionMemos() -> void {
  TypeMemo().clear();
  CompMemo().clear();
}

SPP_MOD_BEGIN
auto spp::analyse::scopes::ExprSubst::In(
  Scope const &scope, GenericSubst bindings) -> ExprSubst {
  return {.Bindings = std::move(bindings), .Written = &scope, .Reading = &scope};
}

auto spp::analyse::scopes::ExprSubst::Across(
  Scope const &written, GenericSubst bindings, Scope const &reading) -> ExprSubst {
  return {.Bindings = std::move(bindings), .Written = &written, .Reading = &reading};
}

SPP_MOD_END
