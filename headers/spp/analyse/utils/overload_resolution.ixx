module;
#include <spp/macros.hpp>

export module spp.analyse.utils.overload_resolution;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionOperatorFunctionCallAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::overload_resolution {
  /// A function overload struct contains information about
  /// the scope/proto of the function, generics being inherited
  /// into it, and a potential type-forwarding  type too.
  SPP_EXP_CLS struct FnOverload {
    Scope const *FnScope;
    FunctionPrototypeAst *Proto;
    Unique<GenericArgumentGroupAst> SupGns;
    Shared<TypeAst> FwdType;
  };

  /// Where one parameter's value comes from at a call, as the
  /// check of an overload decided it: arguments of the call (one,
  /// or a variadic parameter's pack of them), or a default made
  /// for it. Checking reads the call's arguments and never edits
  /// them; only the chosen overload's slots are applied to them
  /// ("ElaborateCall").
  SPP_EXP_CLS struct ParamSlot {
    /// The parameter's name, placed where its argument was written.
    Shared<IdentifierAst> Name = nullptr;

    /// The indices of the arguments taken, in the call's group.
    Vec<std::size_t> Args = {};

    /// Whether this is the variadic parameter, taking a tuple of
    /// the arguments.
    bool IsPack = false;

    /// The default value, for a parameter the call left out.
    Unique<ExpressionAst> Default;

    /// The type of what the parameter is given.
    Shared<TypeAst> Type = nullptr;

    /// Whether this is "self", whose argument takes the
    /// convention the prototype declares ("SelfConv").
    bool IsSelf = false;
    Unique<ConventionAst> SelfConv;

    /// Whether the value is handed over through its forwarding
    /// type ("&Vec[T]" to a "&View[T]" parameter).
    bool IsForwarded = false;
  };

  SPP_EXP_CLS struct PassedOverload {
    Scope const *FnScope;
    FunctionPrototypeAst *Proto;
    Vec<ParamSlot> Slots;

    /// What "Self" in the prototype's signature stands for at this
    /// call: the receiver ("Type::method(..)", which "a.method(..)"
    /// is rewritten to), unless the method was reached by
    /// forwarding or there is none, when it is the type owning the
    /// method. Decided once, for every reader of the call. Null for
    /// a call with no owning type (a closure).
    Shared<TypeAst> SelfType;
  };

  SPP_EXP_CLS struct FailedOverload {
    FunctionPrototypeAst *Proto;
    Str Error;
    Str Reason;
  };

  /// Given a function call, perform a complex set of steps
  /// to determine the overload being used. This requires
  /// method-to-function propagation, heavy generic inference,
  /// type forwarding, argument validation, and candidate
  /// disambiguation.
  SPP_EXP_FUN auto DetermineOverload(
    PostfixExpressionOperatorFunctionCallAst &fn_call,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Pair<PassedOverload, bool>;

  /// Rewrite a call's arguments into what the chosen overload
  /// takes: one keyword argument per parameter, in parameter
  /// order, with the defaults, packs, "self" convention and
  /// forwarding calls its check decided. The analysed values are
  /// moved, not copied.
  SPP_EXP_FUN auto ElaborateCall(
    FunctionCallArgumentGroupAst &args,
    PassedOverload &overload,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void;

  /// For each argument of a call, the type every overload the
  /// callee could resolve to expects there, or null. Read before
  /// the arguments are analysed, so an argument that is itself a
  /// call overloaded only on its return type is resolved against
  /// it ("h(g())"). Null where that would be circular: where the
  /// candidates disagree, or the parameter is generic, the type
  /// the argument needs depends on the argument itself.
  SPP_EXP_FUN auto ExpectedArgTypes(
    PostfixExpressionOperatorFunctionCallAst const &fn_call,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Vec<Shared<TypeRef>>;

  /// Given a function name and a scope, find all the function
  /// overloads that can be reached. This includes searching
  /// through scopes, handling generics, etc. All overloads
  /// are then processed for eligibility when calling.
  SPP_EXP_FUN auto GetAllFnScopes(
    IdentifierAst const &target_fn_name,
    Scope const *target_scope,
    ScopeManager &sm,
    meta::CompilerMetaData *meta)
    -> Vec<FnOverload>;

}
