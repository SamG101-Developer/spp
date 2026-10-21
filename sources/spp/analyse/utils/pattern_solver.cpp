module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.pattern_solver;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.aliases;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.float_literal_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.uid;
import genex;
import std;

namespace spp::analyse::utils::pattern_solver {
  namespace {
    /// A position a pattern tests: its type, where that type reads,
    /// and the constructors the type is made of: a variant's members
    /// (read through an alias, "Opt[T]", to the variant it stands for),
    /// "Bool"'s "false" and "true", or the one constructor of any other
    /// type.
    struct Column {
      /// The type at this position, as a value of it (no convention).
      TypeRef Ref;

      /// Where that type reads (field types resolve relative to it).
      Scope const *Where = nullptr;

      /// If it's a variant, the members that form the variant.
      Vec<TypeRef> VariantMembers;

      /// If it's a bool.
      bool IsBool = false;
    };

    /// One pattern, at the position it tests, in the terms of that
    /// position's type: anything ("Any", which a name binds); a literal;
    /// an expression; one constructor with a node per field ("Ctor": a
    /// variant member by index, "false"/"true" as 0/1, or a product
    /// type's one constructor 0); several members of a variant at once,
    /// by naming a variant made of them ("Members": "is Opt[S32](..)" on
    /// "Some[S32] or None or GenDone"); or a pattern whose shape is not
    /// known here ("Unknown": an unresolved type, an arity that does not
    /// line up). Two patterns on one position ("is Some[Some[S32]]" and
    /// "val=Some(v)") meet: their fields in turn when both name one
    /// constructor, "Also" otherwise.
    struct PatternNode {
      enum class Kind { Any, Literal, Expression, Ctor, Members, Unknown } K = Kind::Any;

      /// The pattern written at this position, if any.
      CasePatternVariantAst const *Source = nullptr;

      /// The type tested here.
      Column Col;

      /// Where this position is, from the subject.
      Vec<PathStep> Path;

      /// The constructor, for "Ctor".
      std::size_t Ctor = 0;

      /// The variant's members, for "Members".
      Vec<std::size_t> Members;

      /// For "Ctor": an object destructure, which coverage gives up on
      /// as a whole when it gives up on one of its fields.
      bool Whole = false;

      /// For "Ctor": one node per field of the constructor, "Any" where
      /// nothing is written.
      Vec<PatternNode> Fields;

      /// Further patterns on this position that do not meet this one.
      Vec<PatternNode> Also;
    };

    /// [CHECKED]
    /// The column for a value of type "ref": a variant's members (read
    /// through an alias, "Opt[T]", to the variant it stands for), or
    /// "Bool"'s two values; any other type has the one constructor.
    /// Effectively: what constructors can form this pattern? For Opt,
    /// that's "Some[T]" and "None". For bool, its "true" and "false".
    auto ColumnOf(TypeRef const &ref, Scope const &where) -> Column {
      // Build an empty column, and bind the type reference and the
      // scope to it. This forms the basis of the column.
      auto col = Column();
      col.Ref = ref.WithoutConvention();
      col.Where = &where;
      if (col.Ref.Symbol == nullptr) { return col; }

      // Extract the variant members from the column's newly bound
      // type reference, and bind to the column too. Will be {} for
      // a non-variant type.
      col.VariantMembers = type_members::VariantMemberRefs(col.Ref, where);
      auto through = col.Ref;
      const auto is_alias = [](TypeRef const &r) {
        return r.Symbol != nullptr and r.Symbol->Alias != nullptr;
      };

      // Move through aliases by identity to the variant behind them
      // ("Opt[T]" is "Var[..]"). A chain ends: cycles are rejected
      // where an alias is declared ("aliases::StatementTarget").
      while (col.VariantMembers.empty() and is_alias(through)) {
        const auto target = aliases::InstanceTargetIdOf(*through.Symbol, through.Id);
        if (target == nullptr or target == through.Id) { break; }
        through = TypeRef::Of(target, where);
        col.VariantMembers = type_members::VariantMemberRefs(through, where);
      }

      // If there were no variant members, then check if the column
      // is a bool comparison.
      col.IsBool = col.VariantMembers.empty()
        and type_predicates::IsTypeBool(col.Ref, where);
      return col;
    }

    /// The parts of constructor "ctor" of "col": none for a "Bool" or
    /// an unresolved type; the "ctor"th member's, for a variant; the
    /// type's own otherwise.
    auto PartsOf(Column const &col, const std::size_t ctor) -> Vec<type_members::TypePart> {
      if (col.IsBool or col.Ref.Symbol == nullptr) { return {}; }
      const auto &of = col.VariantMembers.empty() ? col.Ref : col.VariantMembers[ctor];
      return type_members::GetAllParts(of, *col.Where);
    }

    /// [CHECKED]
    /// The fields of constructor "ctor" of "col", as columns of
    /// their own.
    auto FieldsOf(Column const &col, const std::size_t ctor) -> Vec<Column> {
      // Create a column for each of the fields.
      auto out = Vec<Column>();
      for (auto const &part : PartsOf(col, ctor)) {
        out.emplace_back(ColumnOf(part.Ref, part.Where != nullptr ? *part.Where : *col.Where));
      }
      return out;
    }

    // -- Nodes --------------------------------------------------------------------------------------------------------

    /// The "any" node is a pattern node that cannot, by shape,
    /// fail. This is applicable for literals and expressions.
    auto AnyNode(
      Column col, Vec<PathStep> path,
      CasePatternVariantAst const *source = nullptr) -> PatternNode {
      auto n = PatternNode();
      n.Source = source;
      n.Col = std::move(col);
      n.Path = std::move(path);
      return n;
    }

    auto UnknownNode(
      Column col, Vec<PathStep> path,
      CasePatternVariantAst const *source) -> PatternNode {
      auto n = AnyNode(std::move(col), std::move(path), source);
      n.K = PatternNode::Kind::Unknown;
      return n;
    }

    /// A "ctor node" is a pattern that commits to exactly one
    /// constructor of its position's type. The "ctor" parameter
    /// is n for a variant destructure, otherwise 0 (tuple/array
    /// etc). For example, if we have:
    ///
    ///   is Some[S32](val=5) on a case subject of Opt[S32]
    ///   Ctor(Some) path []
    ///   └─ val: literal 5; path [payload Some, field 0 "val"]
    ///   checks: tag {Some} at []; literal 5 at [payload Some, field "val"]
    ///   Pat: Some(Lit 5)
    ///
    ///   is (0, _) on a case subject of (S32, Bool)
    ///   Ctor(0) path []
    ///   ├─ 0: literal 0; path [field 0]
    ///   └─ 1: any; path [field 1]
    ///   checks: literal 0 at [field 0]
    ///   pat: T(Lit 0, _)
    auto CtorNode(
      Column const &col, const std::size_t ctor,
      Vec<PathStep> const &path) -> PatternNode {
      auto n = AnyNode(col, path);
      n.K = PatternNode::Kind::Ctor;
      n.Ctor = ctor;

      auto into = path;
      if (not col.VariantMembers.empty()) {
        into.emplace_back(PathStep{.K = PathStep::Kind::Payload, .Index = ctor, .Name = nullptr, .Of = col.Ref});
      }
      const auto &holder = col.VariantMembers.empty() ? col.Ref : col.VariantMembers[ctor];
      for (auto const &[f, part] : PartsOf(col, ctor) | genex::views::enumerate) {
        auto field_path = into;
        field_path.emplace_back(PathStep{
          .K = PathStep::Kind::Field, .Index = f, .Name = part.Step, .Of = holder.WithoutConvention(),
          .Through = part.Ref.IsBorrowed()
        });
        n.Fields.emplace_back(AnyNode(
          ColumnOf(part.Ref, part.Where != nullptr ? *part.Where : *col.Where), std::move(field_path)));
      }
      return n;
    }

    /// Two patterns on one position, as one node: the other, when either
    /// is "Any"; one constructor with its fields met in turn, when both
    /// name it. Anything else keeps both, the second under "Also".
    auto Meet(PatternNode a, PatternNode b) -> PatternNode {
      const auto is_any = [](PatternNode const &n) { return n.K == PatternNode::Kind::Any and n.Also.empty(); };
      if (is_any(b)) { return a; }
      if (is_any(a)) {
        if (b.Source == nullptr) { b.Source = a.Source; }
        return b;
      }
      if (a.K == PatternNode::Kind::Ctor and b.K == PatternNode::Kind::Ctor and a.Ctor == b.Ctor
        and a.Fields.size() == b.Fields.size()) {
        for (auto f = 0uz; f < a.Fields.size(); ++f) {
          a.Fields[f] = Meet(std::move(a.Fields[f]), std::move(b.Fields[f]));
        }
        a.Whole = a.Whole or b.Whole;
        return a;
      }
      a.Also.emplace_back(std::move(b));
      return a;
    }

    /// The node a destructure naming "type" is, before its elements:
    /// "Any", for the column's own type when that is a variant or a
    /// "Bool"; the type's one constructor, for any other own type; a
    /// member, for one of the column's members; and for a type refining
    /// a member (one more exact in its fields' types, "Some[Some[T]]"
    /// for "Some[Opt[T]]"), that member with each differing field
    /// refined in turn.
    auto Refine(
      TypeRef const &type, Scope const &type_scope, Column const &col,
      Vec<PathStep> const &path, CasePatternVariantAst const *source) -> PatternNode {
      // A destructure type that does not resolve (a user error its
      // own analysis reports) is a shape not known here.
      if (type.Symbol == nullptr or col.Ref.Symbol == nullptr) { return UnknownNode(col, path, source); }

      // If the two types are directly equal, ie Str vs Str, then
      // as long as the subject doesn't have variant members, the
      // type's one constructor.
      if (type_compare::TypeEq(col.Ref, type, *col.Where, type_scope)) {
        return col.VariantMembers.empty() and not col.IsBool ? CtorNode(col, 0, path) : AnyNode(col, path);
      }

      // Check each variant member, and return the match if there
      // is a direct one.
      for (auto const &[m, member] : col.VariantMembers | genex::views::enumerate) {
        if (type_compare::TypeEq(member, type, *col.Where, type_scope)) { return CtorNode(col, m, path); }
      }

      // Otherwise, we haven't had a direct match, or a member
      // match, but we need to consider a refined match. Ie, does
      // Some[Some[Str]] match Opt[Opt[Str]]?
      for (auto const &[m, member] : col.VariantMembers | genex::views::enumerate) {
        // Non-matching base templates are not refinements, so
        // skip them.
        if (member.Template() != type.Template()) { continue; }

        // One template can still give two arities (a tuple, or a
        // variadic member against a pattern type of another), which
        // cannot be lined up.
        const auto type_parts = type_members::GetAllParts(type, type_scope);
        auto out = CtorNode(col, m, path);
        if (type_parts.size() != out.Fields.size()) { return UnknownNode(col, path, source); }

        for (auto const &[f, part] : type_parts | genex::views::enumerate) {
          auto const &part_scope = part.Where != nullptr ? *part.Where : type_scope;
          auto &field = out.Fields[f];
          const auto part_ref = part.Ref.WithoutConvention();
          if (type_compare::TypeEq(field.Col.Ref, part_ref, *field.Col.Where, part_scope)) { continue; }
          field = Refine(part_ref, part_scope, field.Col, field.Path, nullptr);
          if (field.K == PatternNode::Kind::Unknown) { return UnknownNode(col, path, source); }
        }
        return out;
      }

      // A variant made of some of the column's members selects any of
      // them ("is Opt[S32](..)" on "Some[S32] or None or GenDone").
      auto members = Vec<std::size_t>();
      for (auto const &member : type_members::VariantMemberRefs(type, type_scope)) {
        const auto m = genex::position(col.VariantMembers, [&](auto const &c) {
          return type_compare::TypeEq(c, member, *col.Where, type_scope);
        });
        if (not m.has_value()) { return UnknownNode(col, path, source); }
        members.emplace_back(*m);
      }
      if (members.empty()) { return UnknownNode(col, path, source); }
      auto out = AnyNode(col, path, source);
      out.K = PatternNode::Kind::Members;
      out.Members = std::move(members);
      return out;
    }

    auto NodeOf(
      CasePatternVariantAst const &pattern, Column const &col, Vec<PathStep> const &path,
      Scope const &scope) -> PatternNode;

    /// A positional destructure ("(a, b)", "[a, .., z]") of a tuple or
    /// array column: each element against the field in its position,
    /// and a ".." as however many fields it stands for.
    auto PositionalNode(
      CasePatternVariantAst const &pattern, Vec<Unique<CasePatternVariantAst>> const &elems,
      Column const &col, Vec<PathStep> const &path, Scope const &scope) -> PatternNode {
      // A ()/[] destructure of a column that is not comp-time
      // indexable is a user error the destructure's own analysis
      // reports.
      if (col.Ref.Symbol == nullptr
        or not type_predicates::IsTypeCompTimeIndexable(col.Ref, *col.Where)) {
        return UnknownNode(col, path, &pattern);
      }

      // The column's one constructor, then each element in the
      // field it lines up with: those after a ".." count back from
      // the end.
      auto out = CtorNode(col, 0, path);
      out.Source = &pattern;
      const auto skip = genex::position(
        elems, [](auto const &e) { return e->template To<CasePatternVariantDestructureSkipMultipleArgumentsAst>(); });

      const auto n = elems.size();
      for (auto i = 0uz; i < n; ++i) {
        if (i == skip) { continue; }
        const auto at = skip.has_value() and i > *skip ? out.Fields.size() - (n - i) : i;
        if (at >= out.Fields.size()) { return UnknownNode(col, path, &pattern); }
        auto &field = out.Fields[at];
        field = NodeOf(*elems[i], field.Col, field.Path, scope);
      }
      return out;
    }

    /// An object destructure ("Some(val=x)", "Point(x, ..)"): the type
    /// it names ("Refine"), with each attribute's pattern on its field.
    auto ObjectNode(
      CasePatternVariantDestructureObjectAst const &obj, Column const &col, Vec<PathStep> const &path,
      Scope const &scope) -> PatternNode {
      // A type that is the column's own variant (or "Bool") has no
      // fields to place elements on, and one not known has none
      // known.
      auto out = Refine(TypeRef::Of(*obj.Type, scope), scope, col, path, &obj);
      out.Source = &obj;
      if (out.K != PatternNode::Kind::Ctor) { return out; }
      out.Whole = true;

      // Each element by the field it names: "field=<pattern>" meets
      // what the type already says of that field.
      const auto names = PartsOf(col, out.Ctor)
        | genex::views::transform([](auto const &part) { return part.Step->Val; })
        | genex::to<Vec>();
      for (auto const &elem : obj.Elems) {
        const auto binding = elem->To<CasePatternVariantDestructureAttributeBindingAst>();
        if (binding == nullptr or binding->Val == nullptr) { continue; }
        const auto f = genex::position(names, [&](auto const &n) { return n == binding->Name->Val; });
        if (not f.has_value()) { return UnknownNode(col, path, &obj); }

        // Read before "field" is moved into "Meet" (c++ argument order
        // is unspecified, and clang moves it first).
        auto &field = out.Fields[*f];
        auto inner = NodeOf(*binding->Val, field.Col, field.Path, scope);
        field = Meet(std::move(field), std::move(inner));
      }
      return out;
    }

    /// Convert a pattern-based ast into a "pattern". Will
    /// recursively look into patterns for nested patterns at
    /// any depth.
    auto NodeOf(
      CasePatternVariantAst const &pattern, Column const &col,
      Vec<PathStep> const &path, Scope const &scope) -> PatternNode {
      // An attribute's binding is the pattern it holds, on the
      // same position. Look into the "Val" attribute (can be
      // another pattern).
      if (const auto binding = pattern.To<CasePatternVariantDestructureAttributeBindingAst>()) {
        return binding->Val != nullptr ? NodeOf(*binding->Val, col, path, scope) : AnyNode(col, path, &pattern);
      }

      // A case pattern comparing to a literal is an "any" node -
      // a node who can't fail, by shape. Type checking already
      // ensures semantically it is ok.
      if (pattern.To<CasePatternVariantLiteralAst>() != nullptr) {
        auto out = AnyNode(col, path, &pattern);
        out.K = PatternNode::Kind::Literal;
        return out;
      }

      // Similarly, a case pattern comparing an expression is also
      // an "any" node. "case my_bool_expr { ... }".
      if (pattern.To<CasePatternVariantExpressionAst>() != nullptr) {
        auto out = AnyNode(col, path, &pattern);
        out.K = PatternNode::Kind::Expression;
        return out;
      }

      // A tuple is a positional node, ie do the elements match by
      // position, as they are unnamed.
      if (const auto tup = pattern.To<CasePatternVariantDestructureTupleAst>()) {
        return PositionalNode(pattern, tup->Elems, col, path, scope);
      }

      // An array is a positional node, ie do the elements match by
      // position, as they are unnamed.
      if (const auto arr = pattern.To<CasePatternVariantDestructureArrayAst>()) {
        return PositionalNode(pattern, arr->Elems, col, path, scope);
      }

      // An object is a special object node, ie do the elements match
      // by keyword, as all object fields are named.
      if (const auto obj = pattern.To<CasePatternVariantDestructureObjectAst>()) {
        return ObjectNode(*obj, col, path, scope);
      }

      // A name, a skip, an "else": anything.
      return AnyNode(col, path, &pattern);
    }

    // -- Coverage -----------------------------------------------------------------------------------------------------

    /// A pattern as coverage reads it, in the terms of the column it
    /// tests: anything ("Wild"); a test nothing is known to pass
    /// ("Opaque"); one constructor of the column's type with a pattern
    /// per field ("Ctor"); or a literal of a type with too many values
    /// to list ("Lit", by its value's interned identity, so "16" and
    /// "0x10" are one value).
    struct Pat {
      enum class Kind { Wild, Opaque, Ctor, Lit } K = Kind::Wild;

      std::size_t Ctor = 0;
      scopes::CompId Lit = nullptr;
      Vec<Pat> Fields;
    };

    auto LitPat(const scopes::CompId value) -> Pat {
      auto p = Pat();
      p.K = Pat::Kind::Lit;
      p.Lit = value;
      return p;
    }

    auto OpaquePat() -> Pat {
      auto p = Pat();
      p.K = Pat::Kind::Opaque;
      return p;
    }

    auto CtorPat(Column const &col, const std::size_t ctor) -> Pat {
      auto p = Pat();
      p.K = Pat::Kind::Ctor;
      p.Ctor = ctor;
      p.Fields = Vec<Pat>(PartsOf(col, ctor).size(), Pat());
      return p;
    }

    /// The value a literal pattern tests. Every literal in one column
    /// has the column's type, so a number is its value alone: "5" and
    /// "5_s32" are one value in an "S32" column. Anything else is
    /// keyed as a comp value ("Scope::CompIdOf").
    auto LiteralValue(LiteralAst const &lit, Scope const &scope) -> scopes::CompId {
      if (const auto i = lit.To<IntegerLiteralAst>()) { return InternCompKey(scopes::CompKey::OfInt(i->BigVal(), "")); }
      if (const auto f = lit.To<FloatLiteralAst>()) { return InternCompKey(scopes::CompKey::OfFloat(f->BigVal(), "")); }
      return scope.CompIdOf(lit);
    }

    /// A literal tested against "col": "true" and "false" select a
    /// "Bool" column's constructor (and test nothing known of any
    /// other); any other literal is its value.
    auto LiteralPat(LiteralAst const &lit, Column const &col, Scope const &scope) -> Pat {
      if (const auto b = lit.To<BooleanLiteralAst>()) {
        return col.IsBool ? CtorPat(col, b->IsTrue() ? 1uz : 0uz) : OpaquePat();
      }
      return LitPat(LiteralValue(lit, scope));
    }

    /// Two patterns for one field, as the one matching what both match:
    /// the other, when either is anything; one constructor with its
    /// fields met in turn, when both name it; one literal, when both
    /// are it. Anything else - an opaque test, or two that cannot both
    /// hold - is nothing known.
    auto MeetPat(Pat a, Pat b) -> Pat {
      if (a.K == Pat::Kind::Wild) { return b; }
      if (b.K == Pat::Kind::Wild) { return a; }
      if (a.K == Pat::Kind::Ctor and b.K == Pat::Kind::Ctor and a.Ctor == b.Ctor
        and a.Fields.size() == b.Fields.size()) {
        for (auto f = 0uz; f < a.Fields.size(); ++f) {
          a.Fields[f] = MeetPat(std::move(a.Fields[f]), std::move(b.Fields[f]));
        }
        return a;
      }
      if (a.K == Pat::Kind::Lit and b.K == Pat::Kind::Lit and a.Lit == b.Lit) { return a; }
      return OpaquePat();
    }

    /// "node" as coverage reads it. An expression tests something
    /// coverage cannot read, except the operand of a comparing branch
    /// ("eq_op", at the top of a pattern only): under "==" a literal
    /// operand is the value it names ("== 1" twice repeats a value, as
    /// "is (1, _)" twice does).
    auto PatOf(PatternNode const &node, Scope const &scope, const bool eq_op = false) -> Pat {
      auto out = Pat();
      switch (node.K) {
        case PatternNode::Kind::Members:
        case PatternNode::Kind::Unknown:
          out = OpaquePat();
          break;
        case PatternNode::Kind::Literal:
          out = LiteralPat(*node.Source->To<CasePatternVariantLiteralAst>()->Literal, node.Col, scope);
          break;
        case PatternNode::Kind::Expression: {
          const auto lit = node.Source->To<CasePatternVariantExpressionAst>()->Expr->To<LiteralAst>();
          out = eq_op and lit != nullptr ? LiteralPat(*lit, node.Col, scope) : OpaquePat();
          break;
        }
        case PatternNode::Kind::Ctor:
          out = CtorPat(node.Col, node.Ctor);
          for (auto const &[f, field] : node.Fields | genex::views::enumerate) {
            out.Fields[f] = PatOf(field, scope);
            if (node.Whole and out.Fields[f].K == Pat::Kind::Opaque) { return OpaquePat(); }
          }
          break;
        default:
          break;
      }
      for (auto const &also : node.Also) { out = MeetPat(std::move(out), PatOf(also, scope)); }
      return out;
    }

    /// Converts each opaque part of a pattern into a wildcard
    /// to allow the coverage to see if there is a branch can be
    /// reached.
    auto AsQuery(Pat p) -> Pat {
      if (p.K == Pat::Kind::Opaque) { return Pat(); }
      for (auto &f : p.Fields) { f = AsQuery(std::move(f)); }
      return p;
    }

    using Row = Vec<Pat>;

    /// The rows of "rows" that match "head" in their first position,
    /// with its fields in its place.
    auto Specialise(Vec<Row> const &rows, Pat const &head, const std::size_t arity) -> Vec<Row> {
      auto out = Vec<Row>();
      for (auto const &row : rows) {
        auto const &first = row[0];
        auto next = Row();

        // A wildcard matches the head with a wildcard per field; a
        // pattern naming the same constructor or literal, with its
        // own fields; anything else, not at all.
        if (first.K == Pat::Kind::Wild) { next = Row(arity, Pat()); }
        else if (first.K == head.K and head.K == Pat::Kind::Ctor and first.Ctor == head.Ctor) { next = first.Fields; }
        else if (first.K == head.K and head.K == Pat::Kind::Lit and first.Lit == head.Lit) {}
        else { continue; }
        for (auto i = 1uz; i < row.size(); ++i) { next.emplace_back(row[i]); }
        out.emplace_back(std::move(next));
      }
      return out;
    }

    /// The implementation of Maranget's "warnings for pattern
    /// matching", and the core of the coverage checking. It
    /// answers: "is there a value that matches q, but none of
    /// the rows?". the rows are the matrix; one per earlier
    /// unguarded pattern, one per position. The query is "q"
    /// (one per position), and the "cols" provide the type at
    /// each position.
    auto Useful(Vec<Row> const &rows, Row const &q, Vec<Column> const &cols) -> bool {
      // Id "q" is empty, there is nothing left to test. Every
      // position has been dealt with. Any surviving row matches
      // with what "q" describes. Only "useful" when no rows
      // are left.
      if (q.empty()) { return rows.empty(); }

      auto const &head = q[0];
      const auto rest_cols = [&](Vec<Column> front) {
        for (auto i = 1uz; i < cols.size(); ++i) { front.emplace_back(cols[i]); }
        return front;
      };
      const auto rest_q = [&](Row front) {
        for (auto i = 1uz; i < q.size(); ++i) { front.emplace_back(q[i]); }
        return front;
      };

      // If the query names a constructor (true, Some(...), tuple/
      // array's 1 constructor), then only rows that can match the
      // constructor matter.
      if (head.K == Pat::Kind::Ctor) {
        const auto fields = FieldsOf(cols[0], head.Ctor);
        return Useful(Specialise(rows, head, head.Fields.size()), rest_q(head.Fields), rest_cols(fields));
      }

      // If the query names a literal (5, "abc"), then do a 0-field
      // specialisation,
      if (head.K == Pat::Kind::Lit) {
        return Useful(Specialise(rows, head, 0), rest_q({}), rest_cols({}));
      }

      // If the query uses "_": when the rows name every constructor
      // of the column, each constructor in turn; otherwise a value
      // of a constructor none of them name reaches only the rows
      // matching anything there.
      auto const &col = cols[0];
      auto named = Set<std::size_t>();
      for (auto const &row : rows) {
        if (row[0].K == Pat::Kind::Ctor) { named.insert(row[0].Ctor); }
      }
      const auto ctors = not col.VariantMembers.empty() ? col.VariantMembers.size() : col.IsBool ? 2uz : 1uz;
      const auto has_literals = genex::any_of(rows, [](auto const &row) { return row[0].K == Pat::Kind::Lit; });
      if (not has_literals and not named.empty() and named.size() == ctors) {
        for (auto c = 0uz; c < ctors; ++c) {
          const auto ctor = CtorPat(col, c);
          if (Useful(Specialise(rows, ctor, ctor.Fields.size()), rest_q(ctor.Fields), rest_cols(FieldsOf(col, c)))) {
            return true;
          }
        }
        return false;
      }
      auto rest = Vec<Row>();
      for (auto const &row : rows) {
        if (row[0].K != Pat::Kind::Wild) { continue; }
        rest.emplace_back(Row(row.begin() + 1, row.end()));
      }
      return Useful(rest, rest_q({}), rest_cols({}));
    }

    /// What "branches" cover, read from their nodes ("PatOf").
    auto CoverageOf(
      Vec<CaseExpressionBranchAst*> const &branches, Vec<Vec<PatternNode>> const &nodes, Column const &col,
      Scope const &scope) -> CaseCoverage {
      const auto cols = Vec{col};
      auto out = CaseCoverage();

      const auto remaining_of = [&](Vec<Row> const &rows) {
        auto remaining = Vec<TypeRef>();
        for (auto const &[m, member] : col.VariantMembers | genex::views::enumerate) {
          if (Useful(rows, Row{CtorPat(col, m)}, cols)) { remaining.emplace_back(member); }
        }
        return remaining;
      };

      // A branch is reachable when some value one of its patterns
      // matches is matched by no unguarded branch before it; one
      // that is not is shadowed by the earliest branch that leaves
      // it nothing.
      auto rows = Vec<Row>();
      auto rows_by = Vec<std::size_t>();
      for (auto const &[b, branch] : branches | genex::views::enumerate) {
        // Each pattern in the column's terms, once: as a query for
        // what it can match, and below as the row it covers. A pattern
        // naming one of the subject's members by constructor selects
        // that member.
        const auto eq_op = branch->Op != nullptr and branch->Op->TokenType == lex::SppTokenType::TK_EQ;
        const auto branch_pats = nodes[b]
          | genex::views::transform([&](auto const &n) { return PatOf(n, scope, eq_op); })
          | genex::to<Vec>();
        const auto pats = branch_pats
          | genex::views::transform([](auto const &p) { return AsQuery(p); })
          | genex::to<Vec>();
        out.SelectedMembers.emplace_back(branch_pats
          | genex::views::transform([&](auto const &p) -> Opt<TypeRef> {
            if (p.K != Pat::Kind::Ctor or col.VariantMembers.empty()) { return Nullopt; }
            return col.VariantMembers[p.Ctor];
          })
          | genex::to<Vec>());

        // Whether any of the branch's patterns matches a value none
        // of the rows "before" do.
        const auto reaches = [&](Vec<Row> const &before) {
          return genex::any_of(pats, [&](auto const &p) { return Useful(before, Row{p}, cols); });
        };

        // Update the "remaining before", "readable" and "shadowed"
        // parts based on the reachability results and columns.
        out.RemainingBefore.emplace_back(remaining_of(rows));
        out.Reachable.emplace_back(reaches(rows));
        out.ShadowedBy.emplace_back(Nullopt);
        for (auto j = 0uz; not out.Reachable.back() and j < rows_by.size(); ++j) {
          if (reaches(Vec<Row>(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(rows_by[j])))) { continue; }
          out.ShadowedBy.back() = j;
          break;
        }

        // A guard invalidates the completeness detection, because
        // its runtime evaluating as to that the condition is
        // preventing from making an exclusive match.
        if (branch->Guard == nullptr) {
          for (auto const &p : branch_pats) { rows.emplace_back(Row{p}); }
        }
        rows_by.emplace_back(rows.size());
      }

      // Falling through every branch leaves the members none of
      // them matched whole.
      out.FallsThrough = Useful(rows, Row{Pat()}, cols);
      out.Remaining = remaining_of(rows);
      return out;
    }

    /// The subject's edges through "branches", given what they cover
    /// ("SubjectEdges").
    auto EdgesOf(
      Vec<CaseExpressionBranchAst*> const &branches, CaseCoverage const &coverage) -> SubjectEdges {
      // Lambda to detect if every type ref in a list is copyable
      // or not. This dictates if a subject is moved or copied on
      // a match.
      using meta::SubjectOnMatch;
      const auto all_copy = [](Vec<TypeRef> const &types) {
        const auto copyable = [](auto const &t) { return t.Symbol != nullptr and t.Symbol->IsCopyable(); };
        return not types.empty() and genex::all_of(types, copyable);
      };

      // Each match edge: a binding by move takes the subject;
      // narrowing it to "Copy" members alone, or an "else" left with
      // only those, finds it owes nothing. A branch never reached
      // does nothing to it.
      auto out = SubjectEdges();
      for (auto const &[i, branch] : branches | genex::views::enumerate) {
        // If a branch is not reachable, then mark the subject as
        // being kept - this branch can never execute. Continue.
        auto const &patterns = branch->Patterns;
        if (not coverage.Reachable[i]) {
          out.OnMatch.emplace_back(SubjectOnMatch::kKeep);
          continue;
        }

        // The members the branch's patterns select, where each does.
        auto narrowed = Vec<TypeRef>();
        for (auto const &member : coverage.SelectedMembers[i]) {
          if (member.has_value()) { narrowed.emplace_back(*member); }
        }
        const auto is_else = IsElseBranch(*branch);
        const auto narrows_only_to_copy = narrowed.size() == patterns.size() and all_copy(narrowed);

        // Take a subject where any of the patterns move the subject,
        // discharge the subject if we are at the "else" block and
        // all the previous branches were copying; otherwise keep it.
        out.OnMatch.emplace_back(
          genex::any_of(patterns, [](auto const &p) { return p->BindsByMove(); })
          ? SubjectOnMatch::kTake
          : (is_else and all_copy(coverage.RemainingBefore[i])) or (not is_else and narrows_only_to_copy)
          ? SubjectOnMatch::kDischarge
          : SubjectOnMatch::kKeep);
      }

      // The fall-through holds the members none of the branches matched.
      out.OnFallThrough = coverage.FallsThrough and all_copy(coverage.Remaining)
        ? SubjectOnMatch::kDischarge
        : SubjectOnMatch::kKeep;
      return out;
    }

    // -- Checks -------------------------------------------------------------------------------------------------------

    /// Whether "pattern" holds a literal anywhere in it.
    auto HoldsLiteral(CasePatternVariantAst const &pattern) -> bool {
      if (pattern.To<CasePatternVariantLiteralAst>() != nullptr) { return true; }
      const auto any_of = [](Vec<Unique<CasePatternVariantAst>> const &elems) {
        return genex::any_of(elems, [](auto const &e) { return HoldsLiteral(*e); });
      };
      if (const auto binding = pattern.To<CasePatternVariantDestructureAttributeBindingAst>()) {
        return binding->Val != nullptr and HoldsLiteral(*binding->Val);
      }
      if (const auto tup = pattern.To<CasePatternVariantDestructureTupleAst>()) { return any_of(tup->Elems); }
      if (const auto arr = pattern.To<CasePatternVariantDestructureArrayAst>()) { return any_of(arr->Elems); }
      if (const auto obj = pattern.To<CasePatternVariantDestructureObjectAst>()) { return any_of(obj->Elems); }
      return false;
    }

    /// The check of the literal at "node": a hidden borrow of the value
    /// there, declared where "sm" stands, and "<value>.eq(&<literal>)"
    /// over it, analysed there.
    auto LiteralCheckOf(PatternNode const &node, ScopeManager &sm, CompilerMetaData &meta) -> PatternCheck {
      auto const &literal = *node.Source->To<CasePatternVariantLiteralAst>()->Literal;
      const auto pos = literal.PosStart();

      // The hidden borrow, a view that owes nothing.
      auto name = MakeShared<IdentifierAst>(pos, "$_pat_" + spp::utils::Uid());
      auto type = node.Col.Ref.AstIn(*sm.CurrentScope)->WithConvention(MakeUnique<ConventionRefAst>(nullptr));
      auto value = MakeShared<VariableSymbol>(name, std::move(type), sm.CurrentScope, VariableKind::Temporary);
      value->MemInfo->AstBorrowed = name.get();
      sm.CurrentScope->AddVarSymbol(value);

      // "<value>.eq(&<literal>)".
      auto eq_arg = MakeUnique<FunctionCallArgumentPositionalAst>(
        MakeUnique<ConventionRefAst>(nullptr), nullptr, AstClone(literal.To<ExpressionAst>()));
      auto eq_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
        nullptr, MakeShared<IdentifierAst>(pos, "eq"));
      auto eq_lhs = MakeUnique<PostfixExpressionAst>(AstClone(name.get()), std::move(eq_field));
      auto eq_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
      eq_call->FnArgGroup->Args.emplace_back(std::move(eq_arg));
      auto call = MakeUnique<PostfixExpressionAst>(std::move(eq_lhs), std::move(eq_call));

      // Analysed for what it is, not for whatever the "case" is
      // being assigned to.
      {
        const auto _meta_guard = MetaGuard(&meta);
        meta.AssignmentTarget = nullptr;
        meta.AssignmentTargetType = nullptr;
        call->Stage7_AnalyseSemantics(&sm, &meta);
      }
      auto out = PatternCheck();
      out.K = PatternCheck::Kind::Literal;
      out.Path = node.Path;
      out.Value = std::move(value);
      out.Call = std::move(call);
      return out;
    }

    /// Everything "node" checks, in order: a variant's tag (a member it
    /// names, or any of a variant's it names) before the fields of its
    /// payload, and each literal where it is written. A pattern whose
    /// shape is not known checks nothing: the walk has no position to
    /// check a literal at, so one there is a gap in the walk, not a
    /// pattern that matches more.
    auto CollectChecks(
      PatternNode const &node, Vec<PatternCheck> &out, ScopeManager &sm, CompilerMetaData &meta) -> void {
      if (node.K == PatternNode::Kind::Unknown and node.Source != nullptr and HoldsLiteral(*node.Source)) {
        Ice(*node.Source, *sm.CurrentScope, "a literal in a pattern whose shape the pattern solver does not know");
      }
      const auto tags_variant = not node.Col.VariantMembers.empty()
        and (node.K == PatternNode::Kind::Ctor or node.K == PatternNode::Kind::Members);
      if (tags_variant) {
        auto check = PatternCheck();
        check.Path = node.Path;
        check.Variant = node.Col.Ref;
        check.Members = node.K == PatternNode::Kind::Ctor ? Vec{node.Ctor} : node.Members;
        for (const auto m : check.Members) { check.MemberRefs.emplace_back(node.Col.VariantMembers[m]); }
        out.emplace_back(std::move(check));
      }
      if (node.K == PatternNode::Kind::Literal) { out.emplace_back(LiteralCheckOf(node, sm, meta)); }
      for (auto const &field : node.Fields) { CollectChecks(field, out, sm, meta); }
      for (auto const &also : node.Also) { CollectChecks(also, out, sm, meta); }
    }
  }
}

/// [CHECKED]
auto spp::analyse::utils::pattern_solver::SubjectValueName() -> Str {
  return "$_case_subject";
}

/// [CHECKED]
auto spp::analyse::utils::pattern_solver::DeclareSubjectValue(
  TypeRef const &subject, const std::size_t pos,
  ScopeManager &sm) -> Shared<VariableSymbol> {
  // A borrow of the subject, as the tests only read it: a view,
  // which owes nothing.
  auto name = MakeShared<IdentifierAst>(pos, SubjectValueName());
  auto type = subject.WithoutConvention().AstIn(*sm.CurrentScope)
                     ->WithConvention(MakeUnique<ConventionRefAst>(nullptr));

  // Build a symbol with the fixed known name, into the current
  // scope. Mark it as borrowed so that it doesn't get moved on
  // comparisons.
  auto value = MakeShared<VariableSymbol>(
    name, std::move(type), sm.CurrentScope, VariableKind::Temporary);
  value->MemInfo->AstBorrowed = name.get();
  sm.CurrentScope->AddVarSymbol(value);
  return value;
}

/// [CHECKED]
auto spp::analyse::utils::pattern_solver::IsElseBranch(
  CaseExpressionBranchAst const &branch) -> bool {
  // Check if either of the two "else" variations of case
  // patterns are being used.
  auto const &patterns = branch.Patterns;
  return not patterns.empty()
    and (patterns[0]->To<CasePatternVariantElseAst>() != nullptr
      or patterns[0]->To<CasePatternVariantElseCaseAst>() != nullptr);
}

SPP_MOD_BEGIN
/// [CHECKED]
PatternSolver::PatternSolver(
  Vec<CaseExpressionBranchAst*> branches, TypeRef const &subject,
  Shared<VariableSymbol> subject_value, Scope const &scope) :
  _Branches(std::move(branches)),
  _Subject(subject),
  _SubjectValue(std::move(subject_value)),
  _Scope(&scope) {
}

/// [CHECKED]
PatternSolver::~PatternSolver() = default;

auto PatternSolver::Solve(
  ScopeManager &sm, CompilerMetaData &meta) const -> MatchPlan {
  // Each pattern of each branch, once, at the subject's position,
  // and what each checks.

  // The column of the pattern, containing the type, scope,
  // any variant members, if its a bool check (not pattern)
  // etc. Make a new "match plan" and bind the subject info
  // to it.
  const auto col = ColumnOf(_Subject, *_Scope);
  auto out = MatchPlan();
  out.Subject = _SubjectValue;
  out.SubjectHeld = _Subject;

  // The nodes are a 2d list of patterns (multiple patterns
  // on a branch, and multiple branches).
  auto nodes = Vec<Vec<PatternNode>>();
  for (const auto branch : _Branches) {
    // Take the pattern asts on each branch and map them to
    // pattern nodes. Add this to the 2d nodes list.
    nodes.emplace_back(branch->Patterns
      | genex::views::transform([&](auto const &p) { return NodeOf(*p, col, {}, *_Scope); })
      | genex::to<Vec>());

    auto &checks = out.Branches.emplace_back().Checks;
    for (auto const &node : nodes.back()) {
      CollectChecks(node, checks.emplace_back(), sm, meta);
    }

    // A comparing branch ("== x") compares the subject itself.
    const auto compares = branch->Op != nullptr and branch->Op->TokenType != lex::SppTokenType::KW_IS;
    const auto checks_any = genex::any_of(checks, [](auto const &c) { return not c.empty(); });
    out.ReadsSubject = out.ReadsSubject or compares or checks_any;
  }

  // What the branches cover, from those nodes.
  out.Coverage = CoverageOf(_Branches, nodes, col, *_Scope);
  out.Edges = EdgesOf(_Branches, out.Coverage);
  return out;
}

SPP_MOD_END
