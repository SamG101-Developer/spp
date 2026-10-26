module;
#include <spp/macros.hpp>

export module spp.analyse.utils.overload_resolution;
import spp.analyse.scopes.type_key;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::utils::overload_resolution, class OverloadSolver);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionParameterAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionOperatorFunctionCallAst);
use(spp::asts, struct PostfixExpressionOperatorRuntimeMemberAccessAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::overload_resolution {
  /// A function overload struct contains information about
  /// the scope/proto of the function, the "sup" scope it was
  /// reached through ("ReachedThrough", whose bound generics it
  /// inherits; null for a free function or an indirect callee), and
  /// a potential type-forwarding type too.
  SPP_EXP_CLS struct FnOverload {
    Scope const *FnScope;
    FunctionPrototypeAst *Proto;
    Scope const *ReachedThrough;
    Shared<TypeAst> FwdType;
  };

  /// How well an argument matched its parameter, best first: the
  /// same type, a type it widens into ("Some" into "Opt", "!" into
  /// anything), a type the argument chose by binding one of the
  /// callee's generics ("f[T](x: T)"), or reached through forwarding.
  /// Overloads are chosen by it ("OverloadSolver::Resolve").
  SPP_EXP_CLS enum class MatchRank : std::uint8_t { Exact, Widened, Bound, Forwarded };

  /// Where one parameter's value comes from at a call, as the
  /// check of an overload decided it: arguments of the call (one,
  /// or a variadic parameter's pack of them), or a default made
  /// for it. Checking reads the call's arguments and never edits
  /// them; only the chosen overload's slots are applied to them
  /// ("ElaborateCall").
  SPP_EXP_CLS struct ParamSlot {
    /// The parameter this is for, in the prototype the call's
    /// arguments were bound against (a generic one's template).
    FunctionParameterAst const *Param = nullptr;

    /// The parameter's name, placed where its argument was written:
    /// the keyword the chosen overload's argument is given under.
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

    /// How well what it is given matched. "self", and a default the
    /// call left to the callee, are no argument the call chose, so
    /// rank as exact.
    MatchRank Rank = MatchRank::Exact;
  };

  SPP_EXP_CLS struct PassedOverload {
    Scope const *FnScope;
    FunctionPrototypeAst *Proto;
    Vec<ParamSlot> Slots;

    /// What "Self" in the prototype's signature stands for at this
    /// call, by identity: the type the call was made on, unless the
    /// method was reached by forwarding or there is none, when it is
    /// the type owning the method. Decided once, for every reader of
    /// the call ("SubstituteSelfId"). Null for a call with no owning
    /// type (a closure).
    TypeId SelfId = nullptr;

    /// Whether the call was made on a value ("obj.method()"), whose
    /// receiver is the "self" argument resolution injected first.
    bool HasReceiver = false;
  };

  SPP_EXP_CLS struct FailedOverload {
    FunctionPrototypeAst *Proto;
    Str Error;
    Str Reason;
  };

  /// One call as resolution reads it, decided once from its
  /// left-hand side: what is called, where its overloads are
  /// looked up, and the type the call was made on.
  SPP_EXP_CLS struct CallSite {
    /// The left-hand side as written ("f", "x.m", "T::m", "ns::f").
    ExpressionAst *Written = nullptr;

    /// The type owning the function ("x"'s type, "T"), the scope
    /// its overloads are found in, and its name.
    Shared<TypeAst> OwnerType;
    Scope const *OwnerScope = nullptr;
    Shared<IdentifierAst> FnName;

    /// The type the call was made on, if any: "Vec[Str]::new()" has
    /// "Vec[Str]", "obj.method()" has "obj"'s type (its owner), and
    /// a module-level "f()" has nothing.
    TypeAst const *CalledOn = nullptr;

    /// For a method called on a value ("obj.method()"), its member
    /// access (".method") and the value it is called on ("obj", the
    /// receiver); nullptr for every other call.
    PostfixExpressionOperatorRuntimeMemberAccessAst const *MemberAccess = nullptr;
    ExpressionAst *Receiver = nullptr;

    /// Whether it is a method called on a value ("obj.method()").
    SPP_ATTR_NODISCARD auto IsMethod() const -> bool { return MemberAccess != nullptr; }
  };

  /// The overloads a call could resolve to: the named function's,
  /// or, for a callee naming none, the one the function type of
  /// a value called through it stands for ("IndirectFnType", a
  /// "FunRef[(S32,), S32]", whose prototype is
  /// "IndirectPrototypeOf"'s).
  SPP_EXP_CLS struct OverloadCandidates {
    Shared<TypeAst const> IndirectFnType;
    Vec<FnOverload> Overloads;
  };

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

  /// One method a scope has under a name, once overrides are
  /// applied: the block declaring it ("sup $m ext FunXxx"), its
  /// prototype, and the "sup" scope it was reached through, whose
  /// generics it inherits.
  SPP_EXP_CLS struct MethodEntry {
    Scope const *FnScope;
    FunctionPrototypeAst *Proto;
    Scope const *ReachedThrough;
  };

  /// Every method "scope" has under "name": for a class, from each
  /// "sup" scope attached to it; for anything else, from itself. An
  /// override hides what it overrides - a method reached nearer the
  /// scope with the same signature as one further up - so what is
  /// left is what a call can reach, and an abstract method left is
  /// one nothing implements. Worked out once per scope and name.
  SPP_EXP_FUN auto MethodsOf(
    Scope const &scope, IdentifierAst const &name) -> Vec<MethodEntry>;

  /// Forget every scope's method table ("GetAllFnScopes") and
  /// every indirect callee's prototype, at the end of a compile.
  SPP_EXP_FUN auto ClearCaches() -> void;

  /// Given a function name and a scope, find all the function
  /// overloads that can be reached. This includes searching
  /// through scopes, handling generics, etc. All overloads
  /// are then processed for eligibility when calling.
  SPP_EXP_FUN auto GetAllFnScopes(
    IdentifierAst const &target_fn_name, Scope const *target_scope,
    ScopeManager &sm, meta::CompilerMetaData *meta) -> Vec<FnOverload>;
}

/// One call's overload resolution, as a solver: what is called
/// and the candidates it could be are found once, from the
/// call's left-hand side, before its arguments are analysed;
/// the arguments are then read against them twice - for the
/// types the candidates agree an argument must be, and (once
/// they are analysed) to check each candidate and choose one.
/// This dual pass allows for an argument to be a function call
/// itself, which may need return-type-overload resolution.
SPP_EXP_CLS class spp::analyse::utils::overload_resolution::OverloadSolver {
public:
  /// Find what "callee" (the call's left-hand side) names, and
  /// its candidates. A callee that names nothing is reported
  /// here.
  OverloadSolver(
    PostfixExpressionOperatorFunctionCallAst &fn_call, ExpressionAst &callee,
    ScopeManager &sm, meta::CompilerMetaData &meta);

  ~OverloadSolver();
  OverloadSolver(OverloadSolver const &) = delete;
  auto operator=(OverloadSolver const &) -> OverloadSolver& = delete;

  /// For each argument of the call, the type every candidate
  /// expects there, or null. Read before the arguments are
  /// analysed, so an argument that is itself a call overloaded
  /// only on its return type, is resolved against it, like
  /// "h(g())".
  SPP_ATTR_NODISCARD auto ExpectedArgTypes() const -> Vec<Shared<TypeRef>>;

  /// For each generic argument of the call, the type every
  /// candidate's comp parameter has there, or null (and null for
  /// a type argument):
  /// the comp twin of "ExpectedArgTypes", read before the generic
  /// arguments are analysed, which are analysed against it.
  SPP_ATTR_NODISCARD auto ExpectedCompArgTypes() const -> Vec<Shared<TypeRef>>;

  /// Check each candidate against the analysed arguments, and
  /// choose the one the call makes: generic inference, type
  /// forwarding, argument validation, and disambiguation. A
  /// method's receiver is injected first (self). Called once
  /// per solver, as it takes the candidates. The second element
  /// is the function type of the indirect callee, or null
  /// for a call of a named function.
  auto Resolve() -> Pair<PassedOverload, Shared<TypeAst const>>;

private:
  /// Check one candidate against the call, without editing the
  /// call: what it is as it matched, or why it does not. An
  /// error that is the call's own rather than the candidate's
  /// is raised.
  auto _Check(
    FnOverload &candidate) -> Expected<PassedOverload, FailedOverload>;

  PostfixExpressionOperatorFunctionCallAst &_Call;
  ScopeManager *_Sm;
  meta::CompilerMetaData *_Meta;
  CallSite _Site;
  OverloadCandidates _Candidates;
};
