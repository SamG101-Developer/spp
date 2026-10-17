module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.case_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.aliases;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
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
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.ptr;
import spp.utils.uid;
import genex;
import std;

namespace spp::analyse::utils::case_utils {
  namespace {
    /// A pattern as coverage reads it, in the terms of the column it
    /// tests: anything ("Wild"); a test nothing is known to pass
    /// ("Opaque"); one constructor of the column's type with a pattern
    /// per field ("Ctor": a variant member by index, "false"/"true" as
    /// 0/1, or a product type's one constructor 0); or a literal of a
    /// type with too many values to list ("Lit", by its value's
    /// interned identity, so "16" and "0x10" are one value).
    struct Pat {
      enum class Kind { Wild, Opaque, Ctor, Lit } K = Kind::Wild;

      std::size_t Ctor = 0;
      scopes::CompId Lit = nullptr;
      Vec<Pat> Fields;
    };

    /// A position a pattern tests: its type, where that type reads,
    /// and the constructors the type is made of.
    struct Column {
      /// The type at this position.
      TypeRef Ref;

      /// Where that type reads (field types resolve relative to it).
      Scope const *Where = nullptr;

      /// If it's a variant, the members that form the variant.
      Vec<TypeRef> VariantMembers;

      /// If it's a bool.
      bool IsBool = false;
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
      // type reference, and bind to the column too.
      col.VariantMembers = type_members::VariantMemberRefs(col.Ref, where);
      auto through = col.Ref;
      const auto is_alias = [](TypeRef const &r) { return r.Symbol != nullptr and r.Symbol->Alias != nullptr; };

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
      col.IsBool = col.VariantMembers.empty() and type_predicates::IsTypeBool(col.Ref, where);
      return col;
    }

    /// [CHECKED]
    /// The fields of constructor "ctor" of "col", as columns of
    /// their own.
    auto FieldsOf(Column const &col, const std::size_t ctor) -> Vec<Column> {
      // Create the output column list. If the column is a bool or
      // non-symbolic, return the empty list. No fields.
      auto out = Vec<Column>();
      if (col.IsBool or col.Ref.Symbol == nullptr) { return out; }

      // If there are no variants, then use the column's type ref,
      // otherwise the nth variant member for n=ctor.
      const auto &of = col.VariantMembers.empty()
        ? col.Ref
        : col.VariantMembers[ctor];

      // Create a column for each of the fields.
      for (auto const &part : type_members::GetAllParts(of, *col.Where)) {
        out.emplace_back(ColumnOf(part.Ref, part.Where != nullptr ? *part.Where : *col.Where));
      }
      return out;
    }

    /// [CHECKED]
    /// Given a column and a ctor index, get the nth variant
    /// member if we are handling variants; otherwise the col
    /// type. Emplace every field name into the "out" vector.
    auto FieldNamesOf(
      Column const &col, const std::size_t ctor) -> Vec<Str> {
      // Build the "out" vector, initially empty. If the col
      // is considering a boolean match, return the vector.
      auto out = Vec<Str>();
      if (col.IsBool or col.Ref.Symbol == nullptr) { return out; }

      // If we are considering variant members, get the nth
      // variant member, otherwise use the overall ref type.
      const auto &of = col.VariantMembers.empty()
        ? col.Ref
        : col.VariantMembers[ctor];

      // For each part in the selected type, add the
      // stringified field name into the list and return it.
      for (auto const &part : type_members::GetAllParts(of, *col.Where)) {
        out.emplace_back(part.Step->Val);
      }
      return out;
    }

    auto CtorPat(Column const &col, const std::size_t ctor) -> Pat {
      auto p = Pat();
      p.K = Pat::Kind::Ctor;
      p.Ctor = ctor;
      p.Fields = Vec<Pat>(FieldsOf(col, ctor).size(), Pat());
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

    /// [CHECKED]
    /// The pattern a destructure naming "type" is, before its
    /// elements: anything, for the column's own type; a member,
    /// for one of the column's members; and for a type refining
    /// a member (one more exact in its fields' types, "Some[Some[T]]"
    /// for "Some[Opt[T]]"), that member with each differing
    /// field refined in turn.
    auto Refine(
      TypeRef const &type, Scope const &type_scope, Column const &col) -> Pat {
      // A destructure type that does not resolve (a user error its
      // own analysis reports) is a pattern coverage gives up on.
      if (type.Symbol == nullptr or col.Ref.Symbol == nullptr) { return OpaquePat(); }

      // If the two types are directly equal, ie Str vs Str, then
      // as long as the subject doesn't have variant members, return
      // a ctor pattern over the type.
      if (type_compare::TypeEq(col.Ref, type, *col.Where, type_scope)) {
        return col.VariantMembers.empty() and not col.IsBool ? CtorPat(col, 0) : Pat();
      }

      // Check each variant member, and return the match if there
      // is a direct one, in a ctor pattern again. This constructs
      // for the inner type.
      for (auto const &[m, member] : col.VariantMembers | genex::views::enumerate) {
        if (type_compare::TypeEq(member, type, *col.Where, type_scope)) { return CtorPat(col, m); }
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
        // coverage cannot line up, so it gives up on the pattern.
        const auto type_parts = type_members::GetAllParts(type, type_scope);
        const auto member_cols = FieldsOf(col, m);
        if (type_parts.size() != member_cols.size()) { return OpaquePat(); }

        // Create the ctor pattern, with "m" fields, "m" being the
        // index of the variant member within the variant type
        // itself.
        auto out = CtorPat(col, m);
        for (auto const &[f, part] : type_parts | genex::views::enumerate) {
          auto const &part_scope = part.Where != nullptr ? *part.Where : type_scope;
          auto const &field = member_cols[f];

          // Allow for the recursive refinement to generate the
          // patterns.
          if (type_compare::TypeEq(field.Ref, part.Ref.WithoutConvention(), *field.Where, part_scope)) { continue; }
          out.Fields[f] = Refine(part.Ref.WithoutConvention(), part_scope, field);
          if (out.Fields[f].K == Pat::Kind::Opaque) { return OpaquePat(); }
        }
        return out;
      }
      return OpaquePat();
    }

    /// Two patterns for one field, as the one matching what both match:
    /// the other, when either is anything; one constructor with its
    /// fields met in turn, when both name it ("is Some[Some[S32]]" and a
    /// binding "val=Some(v)" on the same field); one literal, when both
    /// are it. Anything else - an opaque test, or two that cannot both
    /// hold - is nothing known.
    auto Meet(Pat a, Pat b) -> Pat {
      if (a.K == Pat::Kind::Wild) { return b; }
      if (b.K == Pat::Kind::Wild) { return a; }
      if (a.K == Pat::Kind::Ctor and b.K == Pat::Kind::Ctor and a.Ctor == b.Ctor
        and a.Fields.size() == b.Fields.size()) {
        for (auto f = 0uz; f < a.Fields.size(); ++f) { a.Fields[f] = Meet(std::move(a.Fields[f]), std::move(b.Fields[f])); }
        return a;
      }
      if (a.K == Pat::Kind::Lit and b.K == Pat::Kind::Lit and a.Lit == b.Lit) { return a; }
      return OpaquePat();
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

    auto ToPat(CasePatternVariantAst const &pattern, Column const &col, Scope const &scope) -> Pat;

    /// [CHECKED]
    /// A positional destructure ("(a, b)", "[a, .., z]") of a tuple or
    /// array column: each element against the field in its position,
    /// and a ".." as however many fields it stands for.
    auto PositionalPat(
      Vec<Unique<CasePatternVariantAst>> const &elems, Column const &col, Scope const &scope) -> Pat {
      // A ()/[] destructure of a column that is not comp-time
      // indexable is a user error the destructure's own analysis
      // reports; coverage gives up on it.
      if (col.Ref.Symbol == nullptr or not type_predicates::IsTypeCompTimeIndexable(col.Ref, *col.Where)) {
        return OpaquePat();
      }

      // Build a "ctor" kind of pattern. It has 0 constructors, unlike
      // bool (true/false) and an object type (its variant members).
      // Then get the fields on the column as columns themselves.
      auto out = CtorPat(col, 0);
      const auto fields = FieldsOf(col, 0);
      const auto skip = genex::position(
        elems, [](auto const &e) { return e->template To<CasePatternVariantDestructureSkipMultipleArgumentsAst>(); });

      // Loop up to the number of elements within the pattern, skipping
      // a skip pattern, creating inner patterns for the elements.
      const auto n = elems.size();
      for (auto i = 0uz; i < n; ++i) {
        if (i == skip) { continue; }
        const auto at = skip.has_value() and i > *skip ? fields.size() - (n - i) : i;
        if (at >= fields.size()) { return OpaquePat(); }
        out.Fields[at] = ToPat(*elems[i], fields[at], scope);
      }
      return out;
    }

    /// [CHECKED]
    /// "pattern" in the terms of the column it tests.
    auto ToPat(
      CasePatternVariantAst const &pattern, Column const &col, Scope const &scope) -> Pat {
      // For an attribute binding pattern, we need to recurse
      // the function into the value on the binding, which might
      // be a pattern itself.
      if (const auto binding = pattern.To<CasePatternVariantDestructureAttributeBindingAst>()) {
        return binding->Val != nullptr ? ToPat(*binding->Val, col, scope) : Pat();
      }

      // A literal, by its value ("LiteralPat").
      if (const auto lit = pattern.To<CasePatternVariantLiteralAst>()) {
        return LiteralPat(*lit->Literal, col, scope);
      }

      // An expression tests something only coverage cannot read,
      // except a comparing branch's operand ("BranchPat").
      if (pattern.To<CasePatternVariantExpressionAst>() != nullptr) { return OpaquePat(); }

      // For tuples, use the positional pattern, with the tuple
      // elements.
      if (const auto tup = pattern.To<CasePatternVariantDestructureTupleAst>()) {
        return PositionalPat(tup->Elems, col, scope);
      }

      // For arrays, use the positional pattern, with the array
      // elements.
      if (const auto arr = pattern.To<CasePatternVariantDestructureArrayAst>()) {
        return PositionalPat(arr->Elems, col, scope);
      }

      // For object destructures, use the "Refine" helper to
      // create the applicable pattern. This manages variant
      // matching and inner refinement etc.
      if (const auto obj = pattern.To<CasePatternVariantDestructureObjectAst>()) {
        // We either bind an element of an object by a field
        // name (potentially aliased), or by a value match.
        // The name is a wildcard, the value is opaque.
        auto out = Refine(TypeRef::Of(*obj->Type, scope), scope, col);
        if (out.K == Pat::Kind::Opaque) { return out; }
        if (out.K == Pat::Kind::Wild) { return out; }

        // Get the fields and the fields' names from the col
        const auto names = FieldNamesOf(col, out.Ctor);
        const auto fields = FieldsOf(col, out.Ctor);

        // For each element of the object, check if that we
        // are considering a binding, ie "Type(a=...)".
        for (auto const &elem : obj->Elems) {
          const auto binding = elem->To<CasePatternVariantDestructureAttributeBindingAst>();
          if (binding == nullptr or binding->Val == nullptr) { continue; }
          const auto f = genex::position(names, [&](auto const &n) { return n == binding->Name->Val; });
          if (not f.has_value()) { return OpaquePat(); }
          out.Fields[*f] = Meet(std::move(out.Fields[*f]), ToPat(*binding->Val, fields[*f], scope));
          if (out.Fields[*f].K == Pat::Kind::Opaque) { return OpaquePat(); }
        }
        return out;
      }
      // A name, a skip, an "else": anything.
      return Pat();
    }

    /// A branch's pattern, as "ToPat" reads it, except that the
    /// operand of a comparing branch is an expression: under "==" a
    /// literal operand is the value it names ("== 1" twice repeats
    /// a value, as "is (1, _)" twice does); anything else it compares
    /// with is nothing known.
    auto BranchPat(
      CasePatternVariantAst const &pattern, Column const &col, Scope const &scope, const bool eq_op) -> Pat {
      if (const auto expr = pattern.To<CasePatternVariantExpressionAst>()) {
        const auto lit = expr->Expr->To<LiteralAst>();
        return eq_op and lit != nullptr ? LiteralPat(*lit, col, scope) : OpaquePat();
      }
      return ToPat(pattern, col, scope);
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

        // If the first pattern is a wildcard, the next pattern

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

    /// The value of one field of an already-generated aggregate,
    /// indexed rather than rebuilt.
    auto NarrowOntoField(
      TypeAst const &base_type, IdentifierAst const &field_name,
      llvm::Value *const llvm_base, ScopeManager const &sm,
      LlvmCtx *const ctx) -> llvm::Value* {
      using type_members::GetFieldIndexInType;
      using type_predicates::IsTypeArray;

      const auto uid = "." + spp::utils::Uid();
      const auto bare_type = base_type.WithoutConvention();
      const auto base_type_sym = sm.CurrentScope->FindTypeSymbol(bare_type.get());
      if (base_type_sym == nullptr or base_type_sym->LlvmInfo->LlvmType == nullptr) { return nullptr; }
      const auto llvm_base_ty = base_type_sym->LlvmInfo->LlvmType;

      // Indexing needs an address. A borrowed subject already
      // is one; anything else is a value, and gets a slot to
      // be indexed through - the same materialisation the member
      // access does for a non-symbolic base.
      auto base_ptr = llvm_base;
      if (not llvm_base->getType()->isPointerTy()) {
        base_ptr = LlvmEntryAlloca(llvm_base_ty, "case.pattern.subject" + uid, ctx);
        ctx->Builder.CreateStore(llvm_base, base_ptr);
      }

      // Determine the field pointer which can be set via the
      // runtime member access field (numeric or identifier).
      auto field_ptr = static_cast<llvm::Value*>(nullptr);

      // For the numeric case, we need to either GEP into the
      // array target, or struct GEP into the tuple target.
      if (std::isdigit(static_cast<unsigned char>(field_name.Val[0]))) {
        const auto index = static_cast<std::uint32_t>(std::stoul(field_name.Val));

        // An array lowers to "[n x T]" rather than to a struct,
        // so it is indexed through the array itself.
        if (IsTypeArray(TypeRef::ForKindCheck(*base_type_sym, *sm.CurrentScope), *sm.CurrentScope)) {
          const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
          field_ptr = ctx->Builder.CreateGEP(
            llvm_base_ty, base_ptr, {llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, index)},
            "case.pattern.elem_ptr" + uid);
        }

        // Tuples use the normal struct GEP function, which
        // gets the target field by internal pointer math. No
        // mapping because tuples' field order matches the
        // type arguments' order.
        else {
          field_ptr = ctx->Builder.CreateStructGEP(
            llvm_base_ty, base_ptr, index,
            "case.pattern.elem_ptr" + uid);
        }
      }

      // The layout re-orders fields to minimize padding, so a
      // named attribute's declaration index has to be resolved
      // through the type's own field index map.
      else {
        const auto decl_index = GetFieldIndexInType(*base_type_sym, field_name);
        const auto field_index = GetPhysicalFieldIndex(*base_type_sym->LlvmInfo, decl_index);
        field_ptr = ctx->Builder.CreateStructGEP(
          llvm_base_ty, base_ptr, field_index, "case.pattern.field_ptr" + uid);
      }

      return field_ptr;
    }

    /// Build the "cond.<field>.eq(&literal)" for a literal
    /// element of a pattern, analyse it where the pattern is
    /// (and walk back the scope as the condition might
    /// introduce scopes).
    template <typename T>
    auto MapFieldEqLiteral(
      Unique<PostfixExpressionOperatorRuntimeMemberAccessAst> &&field,
      ExpressionAst const &literal, ScopeManager *sm, CompilerMetaData *meta,
      FuncRef<T(Ast *)> mapper) -> T {
      // Build the asts up.
      auto pf_expr = MakeUnique<PostfixExpressionAst>(
        AstClone(meta->CaseCondition), std::move(field));
      auto eq_arg = MakeUnique<FunctionCallArgumentPositionalAst>(
        MakeUnique<ConventionRefAst>(nullptr), nullptr, AstClone(&literal));
      auto eq_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
        nullptr, MakeShared<IdentifierAst>(0uz, "eq"));
      auto eq_pf_expr = MakeUnique<PostfixExpressionAst>(
        std::move(pf_expr), std::move(eq_field));
      auto eq_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
        nullptr, nullptr, nullptr);
      eq_call->FnArgGroup->Args.emplace_back(std::move(eq_arg));
      const auto eq_call_expr = MakeUnique<PostfixExpressionAst>(
        std::move(eq_pf_expr), std::move(eq_call));

      // Analyse and walk back the scope.
      {
        const auto _back = ScopePositionGuard(*sm);
        eq_call_expr->Stage7_AnalyseSemantics(sm, meta);
      }
      return mapper(eq_call_expr.get());
    }

    template <typename T>
    auto CreateAndAnalysePatternEqFnsCore(
      Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
      CompilerMetaData *meta, FuncRef<T(Ast *)> mapper,
      Func<void(ExpressionAst *)> &&on_nested_subject = {})
      -> Vec<T> {
      auto transformed = Vec<T>();
      transformed.reserve(elems.size());

      if (not elems.empty() and elems[0]->To<CasePatternVariantExpressionAst>()) {
        // For expression patterns, just generate the expression once.
        const auto expr_part = elems[0]->To<CasePatternVariantExpressionAst>();
        auto transform = mapper(expr_part->Expr.get());
        transformed.emplace_back(std::move(transform));
        return transformed;
      }

      // A bound-or-unbound multi-argument skip ("..") absorbs a number
      // of real array/tuple slots that isn't known until the real (rhs)
      // length is known, so any positionally-addressed element after it
      // ("cond.<i>") cannot use its raw position within "elems" - that
      // would under-count by the width of the skip and read one slot too
      // early.
      auto skip_index = Opt<std::size_t>{};
      for (auto const &[i, part] : elems | genex::views::enumerate) {
        if (part->To<CasePatternVariantDestructureSkipMultipleArgumentsAst>() != nullptr) {
          skip_index = i;
          break;
        }
      }

      // Todo: move "max length" into type_resolution function, and route the
      //  "is in bounds" through that too.
      auto num_rhs_elems = Opt<std::size_t>{};
      const auto real_index = [&](const std::size_t i) -> std::size_t {
        if (not skip_index.has_value() or i <= *skip_index) { return i; }
        if (not num_rhs_elems.has_value()) {
          const auto cond_ref = meta->CaseCondition->InferTypeRef(sm, meta).WithoutConvention();
          num_rhs_elems = type_predicates::IsTypeArray(cond_ref, *sm->CurrentScope)
            ? static_cast<std::size_t>(U64Of(cond_ref.Symbol->CompArgId("n")).value())
            : cond_ref.Symbol->TypeArgRefs().size();
        }
        return *num_rhs_elems - (elems.size() - i);
      };

      for (auto const &[i, part] : elems | genex::views::enumerate) {
        // For literals and expressions, generate the equality checks.
        if (auto const *const literal = part->To<CasePatternVariantLiteralAst>(); literal != nullptr) {
          // The element by its position: "cond.<i>.eq(&literal)".
          auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
            nullptr, MakeShared<IdentifierAst>(0uz, std::to_string(real_index(i))));
          transformed.emplace_back(MapFieldEqLiteral<T>(
            std::move(field), *literal->Literal->To<ExpressionAst>(), sm, meta, mapper));
        }

        // For named attribute bindings whose value is a literal, like "field=literal".
        else if (const auto cast_attr = part->To<CasePatternVariantDestructureAttributeBindingAst>();
          cast_attr != nullptr and cast_attr->Val->To<CasePatternVariantLiteralAst>() != nullptr) {
          // The attribute by its name: "cond.field.eq(&literal)".
          const auto literal_part = cast_attr->Val->To<CasePatternVariantLiteralAst>();
          auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
            nullptr, AstCloneShared(cast_attr->Name));
          transformed.emplace_back(MapFieldEqLiteral<T>(
            std::move(field), *literal_part->Literal->To<ExpressionAst>(), sm, meta, mapper));
        }

        // For nested objects (array, tuple, object)
        else if (
          part->To<CasePatternVariantDestructureArrayAst>() != nullptr or
          part->To<CasePatternVariantDestructureTupleAst>() != nullptr or
          part->To<CasePatternVariantDestructureObjectAst>() != nullptr) {
          // Generate the extraction on the condition for this part, like "cond.0".
          auto field_name = MakeShared<IdentifierAst>(0uz, std::to_string(real_index(i)));
          auto field = MakeUnique<
            PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
          auto pf_expr = MakeUnique<PostfixExpressionAst>(AstClone(meta->CaseCondition), std::move(field));

          // Update the "meta->cond" with the "pf_expr", and analyse against the inner part.
          const auto _meta_guard = MetaGuard(meta);
          meta->CaseCondition = pf_expr.get();
          if (on_nested_subject) { on_nested_subject(pf_expr.get()); }

          // Combine the result.
          auto transform = mapper(part);
          transformed.emplace_back(std::move(transform));
        }
      }

      return transformed;
    }
  }
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqFnsLlvm(
  Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
  CompilerMetaData *meta, LlvmCtx *ctx) -> Vec<llvm::Value*> {
  // Get the expression and map then to LLVM values.
  const auto map = [&](Ast *x) {
    return x->Stage11_CodeGen(sm, meta, ctx);
  };

  // Narrow the subject's llvm value onto each element alongside
  // its ast. The element access is built fresh here, so it has
  // to be analysed before it can be generated - the literal
  // branches above do the same, saving and restoring the scope
  // around it because analysis walks into the condition's own scope.
  Func<void(ExpressionAst *)> on_nested_subject = [&](ExpressionAst *subject) {
    // Analyse the subject, and then walk back the scope iterator,
    // as the value itself might have introduced new scopes.
    {
      const auto _back = ScopePositionGuard(*sm);
      subject->Stage7_AnalyseSemantics(sm, meta);
    }

    // If the subject is a postfix expression, extract the runtime
    // member access as the operator (otherwise nullptr).
    const auto access = subject->To<PostfixExpressionAst>();
    const auto field = access != nullptr
      ? access->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>()
      : nullptr;

    // Anything that is not the plain field access requires code
    // generation. No more work needs to be done after that.
    if (field == nullptr or meta->LlvmCaseCondition == nullptr) {
      meta->LlvmCaseCondition = subject->Stage11_CodeGen(sm, meta, ctx);
      return;
    }

    // For nested variant destructures, we need to narrow the
    // value into the target type field, providing access onto
    // the narrowed-type value.
    const auto base_type = access->Lhs->InferType(sm, meta);
    const auto field_ptr = NarrowOntoField(
      *base_type, *field->Name, meta->LlvmCaseCondition, *sm, ctx);
    if (field_ptr == nullptr) {
      meta->LlvmCaseCondition = subject->Stage11_CodeGen(sm, meta, ctx);
      return;
    }

    // A field carrying no value is not laid out, so there is
    // nothing to read and "load void" is not valid ir.
    const auto field_llvm_ty = GetLlvmTypeOf(subject->InferTypeRef(sm, meta).WithoutConvention(), ctx);
    meta->LlvmCaseCondition = IsValuelessType(field_llvm_ty)
      ? nullptr
      : ctx->Builder.CreateLoad(field_llvm_ty, field_ptr, "case.pattern.subject.value");
  };

  // Forward the nested analysis lambda into the core checker
  // to propagate the nested checks properly.
  auto asts = CreateAndAnalysePatternEqFnsCore<llvm::Value*>(
    elems, sm, meta, map, std::move(on_nested_subject));
  return asts;
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqCompTime(
  Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
  CompilerMetaData *meta) -> Vec<Unique<ExpressionAst>> {
  // Get the expression and map then to Comptime values.
  const auto map = [&](Ast *x) -> Unique<ExpressionAst> {
    x->Stage9_CompTimeResolve(sm, meta);
    return std::move(meta->CompTimeResult);
  };

  auto asts = CreateAndAnalysePatternEqFnsCore<Unique<ExpressionAst>>(elems, sm, meta, map);
  return asts;
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqFnsDummyCore(
  Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
  CompilerMetaData *meta) -> void {
  //
  const auto noop = [](Ast *) { return Monostate{}; };
  CreateAndAnalysePatternEqFnsCore<Monostate>(elems, sm, meta, noop);
}

auto spp::analyse::utils::case_utils::ValidateInconsistentTypes(
  Vec<CaseExpressionBranchAst*> const &branches, ScopeManager &sm,
  CompilerMetaData *meta) -> Tup<Pair<Ast*, Shared<TypeAst>>, Vec<Pair<Ast*, Shared<TypeAst>>>> {
  //
  using errors::SppTypeMismatchError;

  // Collect type information for each branch, pairing the
  // branch with its inferred type.
  auto branches_type_info = branches
    | genex::views::transform([&sm, meta](auto *x) { return MakePair(x, x->InferType(&sm, meta)); })
    | genex::to<Vec>();

  // The valued branches are branches that are non-terminating.
  // This is because "ret" from a branch doesn't pass a value
  // back to the binding, so shouldn't be considered for type
  // checking.
  auto valued_branches_type_info = branches_type_info
    | genex::views::remove_if([&sm, meta](auto const &x) { return expr_utils::Diverges(*x.first->Body, &sm, meta); })
    | genex::to<Vec>();

  // When every branch diverges, the ones typed "!" stand for the
  // case (it fits whatever it is assigned to); a "ret" branch's
  // own type is "Void", which the case never produces. No value
  // leaves such a case, so there is nothing for its branches to
  // agree on with what it is assigned to.
  const auto every_branch_diverges = valued_branches_type_info.empty();
  if (valued_branches_type_info.empty()) {
    valued_branches_type_info = branches_type_info
      | genex::views::filter([](auto const &x) { return x.second->IsNeverType(); })
      | genex::to<Vec>();
  }
  if (valued_branches_type_info.empty()) { valued_branches_type_info = branches_type_info; }

  // Filter the branch types down to variant types for custom
  // analysis.
  auto variant_branches_type_info = valued_branches_type_info
    | genex::views::filter([&sm](auto &&x) {
      return type_predicates::IsTypeVariant(TypeRef::ForKindCheck(*x.second, *sm.CurrentScope), *sm.CurrentScope);
    })
    | genex::to<Vec>();

  // Set the master branch type to the first branch that has a
  // value: a branch that diverges ("!") has none for the others
  // to agree with, and only a case whose every branch diverges
  // is "!" itself. This is the default and may be subsequently
  // changed. Override it if an assignment type is given.
  auto master_branch_type_info = MakePair<CaseExpressionBranchAst*, Shared<TypeAst>>(nullptr, nullptr);
  for (auto const &[branch, type] : valued_branches_type_info) {
    if (not type->IsNeverType()) {
      master_branch_type_info = MakePair(branch, type);
      break;
    }
  }
  if (master_branch_type_info.first == nullptr and not valued_branches_type_info.empty()) {
    master_branch_type_info = MakePair(valued_branches_type_info[0].first, valued_branches_type_info[0].second);
  }
  if (meta->AssignmentTargetType != nullptr) {
    master_branch_type_info = MakePair(nullptr, meta->AssignmentTargetType);
  }

  // Otherwise, if there are variant branches, use the most
  // variant type as the master branch type.
  else if (not variant_branches_type_info.empty()) {
    auto most_inner_types = 0uz;
    for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
      const auto variant_size = type_members::VariantMemberRefs(
        TypeRef::Of(*variant_type, *sm.CurrentScope), *sm.CurrentScope).size();
      if (variant_size > most_inner_types) {
        master_branch_type_info = {variant_branch, variant_type};
        most_inner_types = variant_size;
      }
    }
  }

  // Remove the master branch pointer from the list of remaining
  // branch types and check all types match. A "!" branch fits
  // the master type like any value would, so "TypeEq" drops it.
  auto mismatch_branches_type_info = valued_branches_type_info
    | genex::views::remove_if([&](auto const &x) {
      return x.first == master_branch_type_info.first;
    })
    | genex::views::remove_if([&](auto const &x) {
      return type_compare::Assignable(
        TypeRef::Of(*master_branch_type_info.second, *sm.CurrentScope),
        TypeRef::Of(*x.second, *sm.CurrentScope),
        *sm.CurrentScope, *sm.CurrentScope);
    })
    | genex::to<Vec>();

  if (not every_branch_diverges and not mismatch_branches_type_info.empty()) {
    const auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
    const auto [master_branch, master_branch_type] = master_branch_type_info;
    const auto final_member = master_branch != nullptr
      ? master_branch->Body->FinalMember()
      : meta->AssignmentTarget != nullptr
      ? static_cast<Ast*>(meta->AssignmentTarget.get())
      : mismatch_branch->Body->FinalMember();
    Raise<SppTypeMismatchError>(
      {sm.CurrentScope},
      ERR_ARGS(*final_member, *master_branch_type, *mismatch_branch->Body->FinalMember(), *mismatch_branch_type));
  }

  // The `master_branch_type_info.first` is deliberately null when an
  // assignment target type drove the master type (see above); calling
  // `To<>()` through that null pointer is UB, so guard it and keep
  // the null.
  const auto cast_master_branch_type_info = MakePair(
    master_branch_type_info.first ? master_branch_type_info.first->template ToUnchecked<Ast>() : nullptr,
    master_branch_type_info.second);

  // Cast to common AST nodes and return with the types.
  const auto cast_branches_type_info = branches_type_info
    | genex::views::transform([](auto &&x) {
      return MakePair(x.first->template ToUnchecked<Ast>(), x.second);
    })
    | genex::to<Vec>();
  return {cast_master_branch_type_info, cast_branches_type_info};
}

auto spp::analyse::utils::case_utils::ValidateInconsistentMemory(
  Ast *parent, Vec<CaseExpressionBranchAst*> const &branches,
  CaseCoverage const &coverage, VariableSymbol *subject,
  SubjectEdges const *subject_edges, ScopeManager *sm,
  CompilerMetaData *meta) -> void {
  // The symbols whose states are joined: those visible here,
  // and (inside a body) every one its flow has touched, so a
  // symbol hidden by a shadowing "let" is joined too. The
  // lookup can reach one symbol by more than one route.
  auto var_syms = Vec<VariableSymbol*>();
  auto var_syms_seen = Set<VariableSymbol const*>();

  // Add every symbol into the "seen" set. If it was inserted
  // into the set, ie not a duplicate, insert it into the main
  // vector too.
  for (auto sym : sm->CurrentScope->GetAllVarSymbols()) {
    if (var_syms_seen.insert(sym).second) { var_syms.emplace_back(sym); }
  }
  if (const auto flow = memory_state::FlowOf(*sm->CurrentScope); flow != nullptr) {
    for (auto const &[sym, _] : flow->States) {
      if (var_syms_seen.insert(sym).second) {
        var_syms.emplace_back(const_cast<VariableSymbol*>(sym));
      }
    }
  }

  // The state before any branch has run. Each branch starts
  // from it, and it stands in as the path that falls through
  // every branch, where there is one. Each branch's state
  // is kept to be joined.
  const auto pre_analysis = memory_state::SnapshotSymbols(var_syms, *sm->CurrentScope);
  auto paths = Vec<Pair<CaseExpressionBranchAst*, memory_state::ScopeSnapshot>>();
  for (auto const &[i, branch] : branches | genex::views::enumerate) {
    // The branch applies its match edge to the subject.
    meta->CaseSubjectOnMatch = subject_edges != nullptr
      ? subject_edges->OnMatch[i]
      : meta::SubjectOnMatch::kKeep;
    branch->Stage8_CheckMemory(sm, meta);
    paths.emplace_back(branch, memory_state::SnapshotSymbols(var_syms, *sm->CurrentScope));
    memory_state::RestoreSnapshot(pre_analysis);
  }

  // On the fall-through path, the subject holds whichever
  // members no branch matched.
  const auto falls_through = coverage.FallsThrough;
  paths.emplace_back(nullptr, pre_analysis);
  const auto discharged = subject_edges != nullptr
    and subject_edges->OnFallThrough == meta::SubjectOnMatch::kDischarge;

  // A branch that never finishes leaves no state behind to
  // agree with. Worked out once per branch, not per symbol:
  // it infers the body's type.
  auto diverging = Set<CaseExpressionBranchAst const*>();
  for (auto branch : branches) {
    if (expr_utils::Diverges(*branch->Body, sm, meta)) { diverging.insert(branch); }
  }

  // A branch diverges if it in the set of diverging case
  // expression asts.
  const auto diverges = [&](CaseExpressionBranchAst const *branch) {
    return diverging.contains(branch);
  };

  // A branch after every value was matched, is never taken.
  const auto reachable = [&](CaseExpressionBranchAst const *branch) {
    const auto at = genex::position(branches, genex::operations::eq_fixed{branch});
    return not at.has_value() or coverage.Reachable[*at];
  };

  // The first branch that finishes is the path carried on.
  const auto non_terminating_branch = genex::find_if(
    branches, [&](auto const &x) { return not diverges(x); });

  // A path is named by its branch, or by the "case" itself
  // where the branch was generated (a lowered "is" builds an
  // "else" with no source of its own) and so has nowhere to
  // point - or, the lowered "case" having no "case" token
  // either, by its condition, which is the "is" expression's
  // subject as written.
  auto case_named = parent->PosStart() != 0 or meta->CaseCondition == nullptr
    ? parent
    : static_cast<Ast*>(meta->CaseCondition);
  const auto path_of = [case_named](CaseExpressionBranchAst *branch) -> Ast* {
    return branch != nullptr and branch->PosStart() != 0 ? static_cast<Ast*>(branch) : case_named;
  };
  const auto first_branch = non_terminating_branch == branches.end() ? case_named : path_of(*non_terminating_branch);
  auto const &first_path = non_terminating_branch != branches.end()
    ? paths.at(static_cast<std::size_t>(genex::iterators::distance(branches.begin(), non_terminating_branch))).second
    : paths.back().second;

  const auto has_else_branch = not branches.empty()
    ? branches.back()->Patterns[0]->To<CasePatternVariantElseAst>()
    : nullptr;
  const auto skip_else = has_else_branch and has_else_branch->MarkedForIterLoopExit();

  // The state after the "case" is every path's state out of
  // it, joined: the branches that finish, and without an
  // "else" (when no branch may run at all) the state the
  // "case" was entered with (the null entry).
  const auto leaves = [&](CaseExpressionBranchAst const *branch) {
    return not((branch == nullptr and not falls_through) or (branch != nullptr and diverges(branch))
      or (branch != nullptr and not reachable(branch))
      or (skip_else and not branches.empty() and branch == branches.back()));
  };
  const auto state_on = [&](memory_state::ScopeSnapshot const &path, VariableSymbol const &sym) {
    auto state = memory_state::StateIn(path, sym);
    if (discharged and &sym == subject and &path == &paths.back().second) { state.OwesNothing = true; }
    return memory_state::SettleFullyTaken(state, sym, *sm->CurrentScope);
  };
  for (auto sym : var_syms) {
    auto joined = state_on(first_path, *sym);
    for (auto const &[branch_or_null, path] : paths) {
      if (leaves(branch_or_null)) {
        memory_state::Join(joined, state_on(path, *sym), first_branch, path_of(branch_or_null));
      }
    }
    memory_state::StateOf(*sm->CurrentScope, *sym) = joined;
  }

  // The loans held after the "case", likewise.
  auto &loans = memory_state::LoansOf(*sm->CurrentScope);
  const auto loans_on = [](memory_state::ScopeSnapshot const &path) { return path.Loans; };
  loans = loans_on(first_path);
  for (auto const &[branch_or_null, path] : paths) {
    if (leaves(branch_or_null)) {
      memory_state::JoinLoans(loans, loans_on(path), first_branch, path_of(branch_or_null));
    }
  }
}

/// [CHECKED]

auto spp::analyse::utils::case_utils::IsElseBranch(
  CaseExpressionBranchAst const &branch) -> bool {
  // Check if either of the two "else" variations of case
  // patterns are being used.
  auto const &patterns = branch.Patterns;
  return not patterns.empty()
    and (patterns[0]->To<CasePatternVariantElseAst>() != nullptr
      or patterns[0]->To<CasePatternVariantElseCaseAst>() != nullptr);
}

auto spp::analyse::utils::case_utils::CoverageOf(
  Vec<CaseExpressionBranchAst*> const &branches, TypeRef const &subject,
  Scope const &scope) -> CaseCoverage {
  // Generate the column for the subject (type) in the scope,
  // and put it into a list. Copy the variant members into the
  // output coverage struct.
  const auto col = ColumnOf(subject, scope);
  const auto cols = Vec{col};
  auto out = CaseCoverage();
  out.VariantMembers = col.VariantMembers;

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
  for (auto const &branch : branches) {
    // Each pattern in the column's terms, once: as a query for
    // what it can match, and below as the row it covers. A pattern
    // naming one of the subject's members by constructor selects
    // that member.
    const auto eq_op = branch->Op != nullptr and branch->Op->TokenType == lex::SppTokenType::TK_EQ;
    const auto branch_pats = branch->Patterns
      | genex::views::transform([&](auto const &p) { return BranchPat(*p, col, scope, eq_op); })
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

    // Get the branches that are actually useful if a branch is
    // not reachable, then dont include it here.
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

auto spp::analyse::utils::case_utils::PlanSubjectEdges(
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

auto spp::analyse::utils::case_utils::ApplySubjectOnMatch(
  CaseExpressionBranchAst const &branch,
  ScopeManager &sm,
  CompilerMetaData const &meta)
  -> void {
  // "case x of" takes the symbol itself; "case x.inner of"
  // takes one region of it, a partial move like any other.
  // Only a whole subject can be found to owe nothing: the
  // fact is held per symbol.
  const auto subject = meta.CaseSubject;
  auto const *const cond = meta.CaseCondition;
  if (subject == nullptr or cond == nullptr) { return; }
  const auto whole = cond->To<IdentifierAst>() != nullptr;

  // The patterns took it apart, so a part they left unbound
  // would be left with no owner once it is taken.
  if (meta.CaseSubjectOnMatch == meta::SubjectOnMatch::kTake) {
    const auto taken = regions::PlaceOf(*cond, *sm.CurrentScope);
    if (const auto skipped = taken.has_value() ? regions::FirstUnaccountedPart(*subject, *taken, sm) : Str();
      not skipped.empty()) {
      auto const *const blamed = branch.Patterns.empty()
        ? static_cast<Ast const*>(&branch)
        : static_cast<Ast const*>(branch.Patterns[0].get());
      Raise<errors::SppDestructureSkipsOwnedPartError>(
        {sm.CurrentScope}, ERR_ARGS(*blamed, *subject->Name, StrView(skipped)));
    }
  }

  // If we are moving an identifier part, then mark the state
  // as "moved by", so it cannot be used until initialized
  // again.
  if (meta.CaseSubjectOnMatch == meta::SubjectOnMatch::kTake and whole) {
    memory_state::StateOf(*sm.CurrentScope, *subject).MovedBy(*cond);
  }

  // If we are moving a non-identifier part, then this is a
  // partial move, so get its place, and mark the partial
  // move.
  else if (meta.CaseSubjectOnMatch == meta::SubjectOnMatch::kTake) {
    memory_state::StateOf(*sm.CurrentScope, *subject).PartMovedBy(regions::PlaceAtOf(*cond, *sm.CurrentScope));
  }

  // If the discharge flag has been set and it is a identifier
  // ast, then mark the "owes nothing" as true.
  else if (meta.CaseSubjectOnMatch == meta::SubjectOnMatch::kDischarge and whole) {
    memory_state::StateOf(*sm.CurrentScope, *subject).OwesNothing = true;
  }
}
