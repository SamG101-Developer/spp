module;
#include <spp/macros.hpp>

export module spp.analyse.utils.marker_sups;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::marker_sups {
  /// Check the type and search the supertypes to identify a
  /// functional superimposition. Retrieve it. There is a hack
  /// here where generic constraints are considered beforehand,
  /// because if we have a FunRef that must be a FunMov by
  /// constraint, for behaviour to be consistent, it must be
  /// treated like the FunMov would be.
  SPP_EXP_FUN auto GetFunctionalType(
    TypeAst const &type,
    Scope const &scope)
    -> Shared<const TypeAst>;

  /// Check the type and search the supertypes to identifier a
  /// generator superimposition. Retrieve its symbol along with the
  /// Yield type in the generator's generics, and if its Gen or
  /// GenOnce. Fallible with >1 generator candidates. The errors
  /// are placed on "expr" and print the type "spell" gives, which
  /// is only asked for when one is raised.
  SPP_EXP_FUN auto GetGenAndYieldTypes(
    TypeRef const &ref,
    Scope const &scope,
    ExpressionAst const &expr,
    std::function<Shared<TypeAst>()> const &spell,
    StrView what,
    bool raise = true)
    -> Tup<TypeSymbol*, Shared<TypeAst>, bool>;

  /// Resuming a finished "Gen" answers "GenDone", so a yield type
  /// holding it would be indistinguishable from that. Raised on
  /// "expr" when it does. A "GenOnce" has no sentinel, so is
  /// never checked. Basically prevent "GenDone" from manually
  /// appearing - always injected.
  SPP_EXP_FUN auto EnforceYieldTypeWithoutGenDone(
    TypeAst const *yield_type,
    bool is_once,
    Scope const &scope,
    Ast const &expr,
    StrView what)
    -> void;

  /// Check the type and search the supertypes to identifier a
  /// try-type superimposition. Retrieve its symbol, whose "Value"
  /// and "Residual" arguments are what an early return reads. The
  /// errors print the type "spell" gives, as above.
  SPP_EXP_FUN auto GetTryType(
    TypeRef const &ref,
    ExpressionAst const &expr,
    std::function<Shared<TypeAst>()> const &spell,
    ScopeManager const &sm,
    StrView what,
    bool raise = true)
    -> TypeSymbol*;

  /// Check the type and search the supertypes to identify a
  /// forwarding superimposition (pair). Retrieve the ref/mut
  /// forwarding super classes ("FwdRef[T=StrView]" for "Str"),
  /// whose "T" is what is forwarded to.
  SPP_EXP_FUN auto GetFwdTypes(
    TypeSymbol const &sym,
    Scope const &scope)
    -> Pair<TypeSymbol*, TypeSymbol*>;

  /// Manually build the hidden forwarding call that is
  /// abstracted over for things like member access, assignment,
  /// returning etc. This is needed to the llvm codegen can
  /// access the forwarded object.
  SPP_EXP_FUN auto BuildFwdCall(
    ExpressionAst const &receiver,
    TypeRef const &receiver_ref,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Unique<PostfixExpressionAst>;

}
