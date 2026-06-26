module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_call_argument_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.order_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;
import sys;

SPP_MOD_BEGIN
auto spp::asts::FunctionCallArgumentGroupAst::NewEmpty()
    -> Unique<FunctionCallArgumentGroupAst> {
    // Empty ast.
    return MakeUnique<FunctionCallArgumentGroupAst>(
        nullptr, decltype(Args)(), nullptr);
}

spp::asts::FunctionCallArgumentGroupAst::FunctionCallArgumentGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Args) &&args,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Args(std::move(args)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(", not args.IsEmpty() ? args.Front()->PosStart() : 0);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")", not args.IsEmpty() ? args.Back()->PosEnd() : 0);
}

spp::asts::FunctionCallArgumentGroupAst::~FunctionCallArgumentGroupAst() = default;

auto spp::asts::FunctionCallArgumentGroupAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::FunctionCallArgumentGroupAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::FunctionCallArgumentGroupAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<FunctionCallArgumentGroupAst>(
        AstClone(TokL), AstCloneVec(Args), AstClone(TokR));
}

auto spp::asts::FunctionCallArgumentGroupAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Args, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::FunctionCallArgumentGroupAst::GetAllArgs() const
    -> Vec<FunctionCallArgumentAst*> {
    // Filter by casting.
    auto out = Vec<FunctionCallArgumentAst*>();
    for (auto const &arg : Args) {
        out.EmplaceBack(arg.get());
    }
    return out;
}

auto spp::asts::FunctionCallArgumentGroupAst::GetKeywordArgs() const
    -> Vec<FunctionCallArgumentKeywordAst*> {
    // Filter by casting.
    auto out = Vec<FunctionCallArgumentKeywordAst*>();
    for (auto const &arg : Args) {
        if (auto *kw_arg = arg->To<FunctionCallArgumentKeywordAst>(); kw_arg != nullptr) {
            out.EmplaceBack(kw_arg);
        }
    }
    return out;
}

auto spp::asts::FunctionCallArgumentGroupAst::GetPositionalArgs() const
    -> Vec<FunctionCallArgumentPositionalAst*> {
    // Filter by casting.
    auto out = Vec<FunctionCallArgumentPositionalAst*>();
    for (auto const &arg : Args) {
        if (auto *pos_arg = arg->To<FunctionCallArgumentPositionalAst>(); pos_arg != nullptr) {
            out.EmplaceBack(pos_arg);
        }
    }
    return out;
}

auto spp::asts::FunctionCallArgumentGroupAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppExpansionOfNonTupleError;
    using analyse::errors::SppIdentifierDuplicateError;
    using analyse::errors::SppOrderInvalidError;
    using analyse::errors::SppInvalidMutationError;
    using analyse::utils::order_utils::DoOrderArgs;
    using analyse::utils::type_utils::IsTypeTup;

    // Check there are no duplicate argument names.
    const auto arg_names = GetKeywordArgs()
        | genex::views::transform([](auto const &x) { return x->Name.get(); })
        | genex::to<Vec>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<Vec>();

    RaiseIf<SppIdentifierDuplicateError>(
        not arg_names.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*arg_names[0], *arg_names[1], "keyword function-argument"));

    // Check the arguments are in the correct order.
    const auto unordered_args = DoOrderArgs(Args
        | genex::views::ptr
        | genex::views::cast_dynamic<mixins::OrderableAst*>()
        | genex::to<Vec>());

    RaiseIf<SppOrderInvalidError>(
        not unordered_args.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(unordered_args[1].First, *unordered_args[1].Second, unordered_args[0].First, *unordered_args[0].Second));

    // Expand tuple-expansion arguments ("..tuple" => "tuple.0, tuple.1, ...")
    // Must use "materialize" because the list gets updates from within the loop.
    for (auto const &[i, arg] : Args | genex::views::ptr | genex::views::enumerate | genex::to<Vec>()) {
        // Only check position arguments that have ".." tokens.
        const auto pos_arg = arg->To<FunctionCallArgumentPositionalAst>();
        if (pos_arg == nullptr or pos_arg->TokUnpack == nullptr) { continue; }

        // Check the argument value is a tuple expression.
        auto arg_type = arg->InferType(sm, meta);
        RaiseIf<SppExpansionOfNonTupleError>(
            not IsTypeTup(*arg_type, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*pos_arg->TokUnpack, *arg->Val, *arg_type));

        // Replace the tuple-expansion argument with the expanded arguments.
        const auto max = static_cast<sys::ssize_t>(arg_type->TypeParts().Back()->GnArgGroup->Args.Len());
        for (auto j = max - 1; j > -1z; --j) {
            auto field = MakeUnique<IdentifierAst>(arg->Val->PosStart(), std::to_string(j));
            auto new_ast = MakeUnique<PostfixExpressionAst>(
                AstClone(arg->Val),
                MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
            auto new_arg = MakeUnique<FunctionCallArgumentPositionalAst>(AstClone(arg->Conv), nullptr, std::move(new_ast));
            Args.Insert(Args.begin() + static_cast<std::ptrdiff_t>(i), std::move(new_arg));
        }
        genex::actions::erase(Args, Args.begin() + static_cast<std::ptrdiff_t>(i) + max);
    }

    // Analyse the arguments
    for (auto const &arg : Args) {
        arg->Stage7_AnalyseSemantics(sm, meta);
        const auto [sym, _] = sm->CurrentScope->GetVarSymbolOutermost(*arg->Val);
        if (sym == nullptr) { continue; }
        if (arg->Conv == nullptr or *arg->Conv == ConventionTag::REF) { continue; }

        // Immutable symbols cannot be mutated.
        RaiseIf<SppInvalidMutationError>(
            not sym->IsMutable, {sm->CurrentScope},
            ERR_ARGS(*sym->Name, *arg->Conv, *std::get<0>(sym->MemInfo->AstInitialization), "immutable symbol"));

        // Immutable borrows, even if their symbol is mutable, cannot be mutated.
        RaiseIf<SppInvalidMutationError>(
            std::get<0>(sym->MemInfo->AstBorrowed) and *sym->Type->GetConvention() == ConventionTag::REF,
            {sm->CurrentScope}, ERR_ARGS(*sym->Name, *arg->Conv, *std::get<0>(sym->MemInfo->AstBorrowed), "immutable borrow"));
    }
}

auto spp::asts::FunctionCallArgumentGroupAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppMemoryOverlapUsageError;
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    using analyse::utils::mem_utils::MemRegionOverlap;

    // If the target is a coroutine, or the target is called as "async", then pins are required.
    const auto is_target_coro = meta->TargetCallFunctionPrototype and
        meta->TargetCallFunctionPrototype->To<CoroutinePrototypeAst>() != nullptr;
    const auto pins_required = meta->TargetCallWasFunctionAsync or is_target_coro;

    // Define the borrow sets to maintain the law of exclusivity.
    auto borrows_ref = Vec<Ast const*>();
    auto borrows_mut = Vec<Ast const*>();

    // Load pre-existing escaping borrows into the vectors.
    const auto all_syms = sm->CurrentScope->AllVarSymbols();
    for (auto const &sym : all_syms) {
        for (auto const &[borrow, is_mut, _] : sym->MemInfo->AstContainedEscapingBorrows) {
            (is_mut ? borrows_mut : borrows_ref).EmplaceBack(borrow);
        }
    }

    // Get potential handle to bind escaping borrows to.
    const auto handle = meta->AssignmentTarget;
    const auto handle_sym = handle ? sm->CurrentScope->GetVarSymbolOutermost(*handle).First : nullptr;

    for (auto const &arg : Args) {
        // Get the outermost part of the argument as a symbol. If the argument is non-symbolic then there is no need to
        // track borrows to it, as it is a temporary value.
        arg->Stage8_CheckMemory(sm, meta);
        auto [sym, _] = sm->CurrentScope->GetVarSymbolOutermost(*arg->Val);
        if (sym == nullptr) { continue; }

        // Ensure the argument isn't moved or partially moved (applies to all conventions). For non-symbolic arguments,
        // nested checking is done via the argument itself (tuples, arrays, etc). Can borrow attributes so don't check
        // for moving from borrowed context right here.
        ValidateSymbolMemory(*arg->Val, *arg, *sm, false, false, false, false, false, meta);

        if (arg->Conv == nullptr) {
            // Ensure that attributes aren't being moved off of a borrowed value and that pins are maintained. Mark the
            // move or partial move of the argument. Note the "check_pins_linked=False" because function calls can only
            // imply an inner scope, so it is guaranteed that lifetimes aren't being extended.
            ValidateSymbolMemory(*arg->Val, *arg, *sm, true, true, true, true, true, meta);

            // Check the move doesn't overlap with any borrows. This is to ensure that "f(&x, x)" can never happen,
            // because the first argument requires the owned object to outlive the function call, and moving it as the
            // second argument breaks this. Doesn't apply to copyable types.
            if (not sm->CurrentScope->GetTypeSymbol(arg->Val->InferType(sm, meta))->IsCopyable()) {
                auto overlaps = genex::views::concat(borrows_ref, borrows_mut)
                    | genex::views::filter([&arg](auto const &x) { return MemRegionOverlap(*x, *arg->Val); })
                    | genex::to<Vec>();

                RaiseIf<SppMemoryOverlapUsageError>(
                    not overlaps.IsEmpty(), {sm->CurrentScope},
                    ERR_ARGS(*overlaps[0], *arg->Val));
            }
        }

        else if (arg->Conv and *arg->Conv == ConventionTag::REF) {
            // Generate the list of overlapping borrows for immutable borrows.
            auto overlaps = borrows_mut
                | genex::views::filter([&arg](auto const &x) { return MemRegionOverlap(*x, *arg->Val); })
                | genex::to<Vec>();

            // Check the immutable borrow doesn't overlap with any other mutable borrows in the same scope.
            RaiseIf<SppMemoryOverlapUsageError>(
                not overlaps.IsEmpty(), {sm->CurrentScope},
                ERR_ARGS(*overlaps[0], *arg->Val));

            // Save any escaping borrows into the handle's memory info.
            if (handle and pins_required) {
                // TODO: Test suite needs to take handle/lack of handle into account
                handle_sym->MemInfo->AstContainedEscapingBorrows.EmplaceBack(arg->Val.get(), false, sm->CurrentScope);
                sym->MemInfo->AstContainersOfEscapingBorrows.EmplaceBack(handle_sym->Name.get(), arg->Val.get());
            }

            // Add the immutable borrow to the immutable borrow set.
            borrows_ref.EmplaceBack(arg->Val.get());
        }

        else if (arg->Conv and *arg->Conv == ConventionTag::MUT) {
            // Generate the list of overlapping borrows for mutable borrows.
            auto overlaps = genex::views::concat(borrows_ref, borrows_mut)
                | genex::views::filter([&arg](auto &&x) { return MemRegionOverlap(*x, *arg->Val); })
                | genex::to<Vec>();

            // Check the mutable borrow doesn't overlap with any other borrows in the same scope.
            RaiseIf<SppMemoryOverlapUsageError>(
                not overlaps.IsEmpty(), {sm->CurrentScope},
                ERR_ARGS(*overlaps[0], *arg->Val));

            // Save any escaping borrows into the handle's memory info.
            if (handle and pins_required) {
                // TODO: Test suite needs to take handle/lack of handle into account
                handle_sym->MemInfo->AstContainedEscapingBorrows.EmplaceBack(arg->Val.get(), true, sm->CurrentScope);
                sym->MemInfo->AstContainersOfEscapingBorrows.EmplaceBack(handle_sym->Name.get(), arg->Val.get());
            }

            // Add the mutable borrow to the mutable borrow set.
            borrows_mut.EmplaceBack(arg->Val.get());
        }
    }
}

auto spp::asts::FunctionCallArgumentGroupAst::At(
    const char *key) const
    -> FunctionCallArgumentAst const* {
    // Iterate the comptime arguments to find the matching key.
    for (const auto *arg : GetKeywordArgs()) {
        if (arg->Name->Val == key) { return arg; }
    }
    return nullptr;
}

SPP_MOD_END
