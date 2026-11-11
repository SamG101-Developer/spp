module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.compiler.cfg;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.asts.string_literal_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_statement_ast;
import spp.asts.use_statement_ast;
import spp.asts.use_statement_variable_ast;
import spp.codegen.llvm_target;
import spp.compiler.out_layout;
import genex;
import std;

namespace spp::compiler::cfg {
  namespace {
    /// [CHECKED]
    /// The annotations an item carries, if it is one that can. These are then
    /// filtered for "!cfg" and checked if they meet the system filter or not.
    auto AnnotationsOf(asts::Ast &item) -> Vec<Unique<asts::AnnotationAst>>* {
      using namespace spp::asts;
      if (const auto f = item.To<FunctionPrototypeAst>()) {
        return &f->Annotations;
      }
      if (const auto c = item.To<ClassPrototypeAst>()) {
        return &c->Annotations;
      }
      if (const auto a = item.To<ClassAttributeAst>()) {
        return &a->Annotations;
      }
      if (const auto t = item.To<TypeStatementAst>()) {
        return &t->Annotations;
      }
      if (const auto k = item.To<CmpStatementAst>()) {
        return &k->Annotations;
      }
      if (const auto u = item.To<UseStatementAst>()) {
        return &u->Annotations;
      }
      if (const auto v = item.To<UseStatementVariableAst>()) {
        return &v->Annotations;
      }
      return nullptr;
    }

    /// [CHECKED]
    /// Whether an annotation is "!cfg", by the name it is written with: it is
    /// decided before any name can be looked up. Todo: This will need to change
    /// for custom annotations.
    auto IsCfg(asts::AnnotationAst const &annotation) -> bool {
      // Filter by the name on the ast, either imported or qualified from the
      // full "std::annotations::cfg" name.
      const auto name = annotation.Name->ToString();
      return name == "cfg" or name.ends_with("::cfg");
    }

    /// [CHECKED]
    /// Whether the target and build mode meet one "!cfg" condition: every key
    /// it gives. Raises for an unknown key or value.
    auto Meets(
      asts::AnnotationAst const &annotation, const StrView mode) -> bool {
      using namespace spp;
      using namespace spp::analyse::errors;
      auto met = true;
      for (auto const &arg : annotation.FnArgGroup->Args) {
        // Get the name on the parameter. As this is done before the call can be
        // analysed, we need a manual key check on the parameter. Todo: what if
        // we just ignore bad keys here and remove the key error, letting the
        // normal annotation analysis pick up the bad key?
        const auto keyword = arg->To<asts::FunctionCallArgumentKeywordAst>();
        if (keyword == nullptr) {
          Raise<SppCfgUnknownKeyError>(ERR_ARGS(*arg, StrView()));
        }

        // cppcheck-suppress nullPointerRedundantCheck
        const auto key = keyword->Name->Val;
        const auto is_key = genex::contains(CfgKeys(), StrView(key));
        if (not is_key) { Raise<SppCfgUnknownKeyError>(ERR_ARGS(*arg, StrView(key))); }

        // Todo: Again this is wrong once we switch to the enum based values,
        // which compose mathematically with "&", "|", "~".
        const auto literal = arg->Val->To<asts::StringLiteralAst>();
        RaiseIf<SppCfgInvalidValueError>(
          literal == nullptr,
          ERR_ARGS(*arg->Val, StrView(key), "must be a string"));

        // Every alternative named must be a value the key can have.
        // cppcheck-suppress nullPointerRedundantCheck
        const auto value = literal->CppVal();
        auto rest = StrView(value);
        if (rest.starts_with('!')) { rest.remove_prefix(1); }
        while (true) {
          const auto bar = rest.find('|');
          const auto alternative = rest.substr(0, bar);

          // Check the values passed into the parameters are known. Todo: This
          // can all be removed once the enums are in places (guaranteed to be
          // legitimate values).
          const auto known = key == "mode"
            ? genex::contains(ModeNames(), alternative) // Todo: Move the mode check into the "IsCfgValueValid"?
            : codegen::IsCfgValueValid(key, alternative);

          if (not known) {
            const auto why = "names '" + Str(alternative) + "', which no build has";
            Raise<SppCfgInvalidValueError>(ERR_ARGS(*arg->Val, StrView(key), StrView(why)));
          }
          if (bar == StrView::npos) { break; }
          rest.remove_prefix(bar + 1);
        }

        const auto actual = key == "mode"
          ? Str(mode)
          : codegen::TargetCfgValue(key);

        // If the "!cfg" filter is correct to match the system and build, then
        // it is "met". Todo: Short circuit?
        met = met and CfgValueMatches(value, actual);
      }
      return met;
    }

    /// [CHECKED]
    /// Whether every "!cfg" an item carries is met. Gets all the annotations,
    /// filters to "!cfg", and then checks against the system filter.
    auto Kept(asts::Ast &item, const StrView mode) -> bool {
      const auto annotations = AnnotationsOf(item);
      if (annotations == nullptr) { return true; }
      auto kept = true;
      for (auto const &annotation : *annotations) {
        if (IsCfg(*annotation)) { kept = Meets(*annotation, mode) and kept; }
      }
      return kept;
    }

    /// [CHECKED]
    /// The span of an ast (including its annotations) that has been dropped
    /// because of a cfg mismatch. This is used for the IDEA plugin, to mark
    /// dead zones from "!cfg".
    auto SpanOfDropped(asts::Ast &item) -> Pair<std::size_t, std::size_t> {
      using namespace spp::asts;
      // Default the span to the ast itself, start and end. These two positions
      // will then be updated for some ast-specific tuning.
      auto start = item.PosStart();
      auto end = item.PosEnd();

      // The start is updated to the first annotation over the ast, should any
      // asts be present ("!cfg" will).
      if (
        const auto annotations = AnnotationsOf(item);
        annotations != nullptr and not annotations->empty()) {
        start = std::min(start, annotations->front()->PosStart());
      }

      // A function's end position is updated to the end position of the
      // implementation, which sometimes changes (can't remember why; this was a
      // fix).
      if (
        const auto f = item.To<FunctionPrototypeAst>();
        f != nullptr and f->Impl != nullptr) {
        end = std::max(end, f->Impl->PosEnd());
      }

      // A class's end position is updated to the end position of the
      // implementation, which sometimes changes (can't remember why; this was a
      // fix).
      else if (
        const auto c = item.To<ClassPrototypeAst>();
        c != nullptr and c->Impl != nullptr) {
        end = std::max(end, c->Impl->PosEnd());
      }

      // A class's field's end position is updated to the end position of the
      // field's default value, which sometimes changes (can't remember why;
      // this was a fix).
      else if (
        const auto a = item.To<ClassAttributeAst>();
        a != nullptr and a->DefaultVal != nullptr) {
        end = std::max(end, a->DefaultVal->PosEnd());
      }

      return {start, end};
    }

    /// [CHECKED]
    /// Drop what is not kept from a body, recording where it was written, and
    /// filter what is kept that has a body of its own.
    auto Filter(
      Vec<Unique<asts::Ast>> &members, const StrView mode,
      Vec<Pair<std::size_t, std::size_t>> &dropped) -> void {
      using namespace spp::asts;
      // Skip any members that meet the system values for the "!cfg" specs, and
      // then record the token spans of member being dropped.
      std::erase_if(members, [&](Unique<Ast> const &member) {
        if (Kept(*member, mode)) { return false; }
        dropped.emplace_back(SpanOfDropped(*member));
        return true;
      });

      // Recursively apply the "!cfg" to the child asts for certain ast types,
      // to ensure for example a class's fields are not present, if a class is
      // not present itself.
      for (auto const &member : members) {
        if (
          const auto c = member->To<ClassPrototypeAst>();
          c != nullptr and c->Impl != nullptr) {
          Filter(c->Impl->Members, mode, dropped);
        }
        else if (
          const auto f = member->To<SupPrototypeFunctionsAst>();
          f != nullptr and f->Impl != nullptr) {
          Filter(f->Impl->Members, mode, dropped);
        }
        else if (
          const auto e = member->To<SupPrototypeExtensionAst>();
          e != nullptr and e->Impl != nullptr) {
          Filter(e->Impl->Members, mode, dropped);
        }
      }
    }
  }
}

/// [CHECKED]
auto spp::compiler::cfg::CfgKeys() -> Vec<StrView> const& {
  // Hold a static list of the keys available for the "cfg" attribute, where
  // some special validation needs to happen,
  static const auto keys = Vec<StrView>{
    "mode", "family", "os", "arch", "env", "pointer_width", "endian"};
  return keys;
}

/// [CHECKED]
auto spp::compiler::cfg::CfgValueMatches(
  const StrView condition, const StrView actual) -> bool {
  // Simple parse for "!" and "|", until enums are ready. The first thing to do
  // is detect a "!" for negation.
  auto rest = condition;
  const auto negated = rest.starts_with('!');
  if (negated) { rest.remove_prefix(1); }
  auto named = false;

  // Keep checking for "|" being present in the remaining string. If it is, then
  // check if the part of the condition matches the actual. Todo: Short circuit?
  while (true) {
    const auto bar = rest.find('|');
    named = named or rest.substr(0, bar) == actual;
    if (bar == StrView::npos) { break; }
    rest.remove_prefix(bar + 1);
  }

  // Flip the condition based on the negation.
  return named != negated;
}

/// [CHECKED]
auto spp::compiler::cfg::ApplyCfg(
  asts::ModulePrototypeAst &mod,
  const BuildMode mode) -> Vec<Pair<std::size_t, std::size_t>> {
  // Call the filter method to remove ASTs that do not match the "!cfg" filter,
  // removing functions/classes etc.
  auto dropped = Vec<Pair<std::size_t, std::size_t>>();
  if (mod.Impl != nullptr) {
    Filter(mod.Impl->Members, ModeName(mode), dropped);
  }
  return dropped;
}
