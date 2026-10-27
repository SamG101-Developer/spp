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
import genex;
import std;

namespace {
  /// The annotations an item carries, if it is one that can.
  auto AnnotationsOf(spp::asts::Ast &item) -> spp::Vec<spp::Unique<spp::asts::AnnotationAst>>* {
    using namespace spp::asts;
    if (auto *const f = item.To<FunctionPrototypeAst>()) { return &f->Annotations; }
    if (auto *const c = item.To<ClassPrototypeAst>()) { return &c->Annotations; }
    if (auto *const a = item.To<ClassAttributeAst>()) { return &a->Annotations; }
    if (auto *const t = item.To<TypeStatementAst>()) { return &t->Annotations; }
    if (auto *const k = item.To<CmpStatementAst>()) { return &k->Annotations; }
    if (auto *const u = item.To<UseStatementAst>()) { return &u->Annotations; }
    if (auto *const v = item.To<UseStatementVariableAst>()) { return &v->Annotations; }
    return nullptr;
  }

  /// Whether an annotation is "!cfg", by the name it is written
  /// with: it is decided before any name can be looked up.
  auto IsCfg(spp::asts::AnnotationAst const &annotation) -> bool {
    const auto name = annotation.Name->ToString();
    return name == "cfg" or name.ends_with("::cfg");
  }

  /// Whether the target and build mode meet one "!cfg" condition:
  /// every key it gives. Raises for an unknown key or value.
  auto Meets(spp::asts::AnnotationAst const &annotation, const spp::StrView mode) -> bool {
    using namespace spp;
    using namespace spp::analyse::errors;
    auto met = true;
    for (auto const &arg : annotation.FnArgGroup->Args) {
      auto const *const keyword = arg->To<asts::FunctionCallArgumentKeywordAst>();
      if (keyword == nullptr) { Raise<SppCfgUnknownKeyError>(ERR_ARGS(*arg, StrView())); }

      // cppcheck-suppress nullPointerRedundantCheck
      const auto key = keyword->Name->Val;
      const auto is_key = genex::contains(spp::compiler::cfg::CfgKeys(), StrView(key));
      if (not is_key) { Raise<SppCfgUnknownKeyError>(ERR_ARGS(*arg, StrView(key))); }

      auto const *const literal = arg->Val->To<asts::StringLiteralAst>();
      if (literal == nullptr) { Raise<SppCfgInvalidValueError>(ERR_ARGS(*arg->Val, StrView(key), "must be a string")); }

      // Every alternative named must be a value the key can have.
      // cppcheck-suppress nullPointerRedundantCheck
      const auto value = literal->CppVal();
      auto rest = StrView(value);
      if (rest.starts_with('!')) { rest.remove_prefix(1); }
      while (true) {
        const auto bar = rest.find('|');
        const auto alternative = rest.substr(0, bar);
        const auto known = key == "mode"
          ? alternative == "dev" or alternative == "rel"
          : codegen::IsCfgValueValid(key, alternative);
        if (not known) {
          const auto why = "names '" + Str(alternative) + "', which no build has";
          Raise<SppCfgInvalidValueError>(ERR_ARGS(*arg->Val, StrView(key), StrView(why)));
        }
        if (bar == StrView::npos) { break; }
        rest.remove_prefix(bar + 1);
      }

      const auto actual = key == "mode" ? Str(mode) : codegen::TargetCfgValue(key);
      met = met and spp::compiler::cfg::CfgValueMatches(value, actual);
    }
    return met;
  }

  /// Whether every "!cfg" an item carries is met.
  auto Kept(spp::asts::Ast &item, const spp::StrView mode) -> bool {
    auto *const annotations = AnnotationsOf(item);
    if (annotations == nullptr) { return true; }
    auto kept = true;
    for (auto const &annotation : *annotations) {
      if (IsCfg(*annotation)) { kept = Meets(*annotation, mode) and kept; }
    }
    return kept;
  }

  /// Where an item was written, all of it: its annotations, and
  /// the body or default value its own position stops short of.
  auto Extent(spp::asts::Ast &item) -> spp::Pair<std::size_t, std::size_t> {
    using namespace spp::asts;
    auto start = item.PosStart();
    auto end = item.PosEnd();
    if (auto const *const annotations = AnnotationsOf(item); annotations != nullptr and not annotations->empty()) {
      start = std::min(start, annotations->front()->PosStart());
    }
    if (auto const *const f = item.To<FunctionPrototypeAst>(); f != nullptr and f->Impl != nullptr) {
      end = std::max(end, f->Impl->PosEnd());
    }
    else if (auto const *const c = item.To<ClassPrototypeAst>(); c != nullptr and c->Impl != nullptr) {
      end = std::max(end, c->Impl->PosEnd());
    }
    else if (auto const *const a = item.To<ClassAttributeAst>(); a != nullptr and a->DefaultVal != nullptr) {
      end = std::max(end, a->DefaultVal->PosEnd());
    }
    return {start, end};
  }

  /// Drop what is not kept from a body, recording where it was
  /// written, and filter what is kept that has a body of its own.
  auto Filter(
    spp::Vec<spp::Unique<spp::asts::Ast>> &members, const spp::StrView mode,
    spp::Vec<spp::Pair<std::size_t, std::size_t>> &dropped) -> void {
    using namespace spp::asts;
    std::erase_if(members, [&](spp::Unique<Ast> &member) {
      if (Kept(*member, mode)) { return false; }
      dropped.emplace_back(Extent(*member));
      return true;
    });
    for (auto const &member : members) {
      if (auto *const c = member->To<ClassPrototypeAst>(); c != nullptr and c->Impl != nullptr) {
        Filter(c->Impl->Members, mode, dropped);
      }
      else if (auto *const f = member->To<SupPrototypeFunctionsAst>(); f != nullptr and f->Impl != nullptr) {
        Filter(f->Impl->Members, mode, dropped);
      }
      else if (auto *const e = member->To<SupPrototypeExtensionAst>(); e != nullptr and e->Impl != nullptr) {
        Filter(e->Impl->Members, mode, dropped);
      }
    }
  }
}

auto spp::compiler::cfg::CfgKeys() -> Vec<StrView> const& {
  static const auto keys = Vec<StrView>{"mode", "family", "os", "arch", "env", "pointer_width", "endian"};
  return keys;
}

auto spp::compiler::cfg::CfgValueMatches(
  const StrView condition, const StrView actual) -> bool {
  auto rest = condition;
  const auto negated = rest.starts_with('!');
  if (negated) { rest.remove_prefix(1); }
  auto named = false;
  while (true) {
    const auto bar = rest.find('|');
    named = named or rest.substr(0, bar) == actual;
    if (bar == StrView::npos) { break; }
    rest.remove_prefix(bar + 1);
  }
  return named != negated;
}

auto spp::compiler::cfg::ApplyCfg(
  asts::ModulePrototypeAst &mod, const StrView mode) -> Vec<Pair<std::size_t, std::size_t>> {
  auto dropped = Vec<Pair<std::size_t, std::size_t>>();
  if (mod.Impl != nullptr) { Filter(mod.Impl->Members, mode, dropped); }
  return dropped;
}
