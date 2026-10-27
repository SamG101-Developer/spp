module;
#include <spp/macros.hpp>

export module spp.analyse.utils.fn_values;
import spp.analyse.scopes.substitution;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct SupPrototypeExtensionAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::fn_values {
  /// A function value match struct contains information about
  /// the function prototype, the scope it is in, and the
  /// generic arguments being used.
  SPP_EXP_CLS struct FnValueMatch {
    FunctionPrototypeAst *Proto;
    Scope const *FnScope;
    scopes::GenericSubst Bindings;
  };

  /// The "sup $F ext FunXXX { ... }" block an overload was
  /// lowered to, and the overload; or nullptrs.
  SPP_EXP_FUN auto FnBlockOf(
    Scope const &scope) -> Pair<SupPrototypeExtensionAst*, FunctionPrototypeAst*>;

  /// Get the actual name of a function hidden by a $type. For
  /// regular functions and methods, this is the name of the
  /// prototype the mock extends. For closures, there is no name,
  /// as a closure is an unnamed function by definition, so
  /// "nullptr". The scope returned is the parent of the overload.
  SPP_EXP_FUN auto GetFnValueName(
    TypeRef const &type) -> Pair<Shared<IdentifierAst>, Scope const*>;

  /// Given a mock function type like $Type, and a genuine
  /// functional type like FunMov[(), Void], extract the match
  /// information based on what $Type superimposes. Prefer
  /// non-generic overloads, and handle generic functions by
  /// inferring of the "func_type".
  SPP_EXP_FUN auto MatchFnValue(
    TypeRef const &mock, TypeRef const &func,
    Scope const &func_scope) -> Opt<FnValueMatch>;

  /// When we pass a function value into a functional type,
  /// ie "$Type" into "FunMov[(), Void]" - "codegen::CoerceToSlot"
  /// handles this at the codegen level. But if the overload
  /// it stands for is generic, it must be instantiated in
  /// the analysis engine earlier, so the overload is ready
  /// by codegen-time. Called at every slot "CoerceToSlot" is,
  /// and through a tuple or an array element by element.
  SPP_EXP_FUN auto InstantiateFnValue(
    TypeRef const &value, TypeRef const &target,
    ScopeManager *sm, meta::CompilerMetaData *meta) -> void;

  /// Lookup for a generic instantiation of a function based
  /// on the function's "$Type" and the function "FunMov"
  /// type.
  SPP_EXP_FUN auto FindFnValue(
    TypeRef const &value, TypeRef const &target,
    ScopeManager const &sm) -> FunctionPrototypeAst*;

  /// The function type an expression is called indirectly
  /// through: the "Fun*" its type is or superimposes, or the
  /// one a generic constraint promised ("IndirectFnType" on
  /// its symbol); null for anything not callable. Todo: Do we
  /// want to return TypeAst here, or a TypeRef?
  SPP_EXP_FUN auto IndirectFnTypeOf(
    ExpressionAst &expr, ScopeManager &sm,
    meta::CompilerMetaData *meta) -> Shared<const TypeAst>;
}
