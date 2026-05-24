module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_argument_comp_positional_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::GenericArgumentCompPositionalAst::GenericArgumentCompPositionalAst(
    decltype(Val) &&val) :
    GenericArgumentCompAst(std::move(val), utils::OrderableTag::kPositionalArg) {
}

spp::asts::GenericArgumentCompPositionalAst::~GenericArgumentCompPositionalAst() = default;

auto spp::asts::GenericArgumentCompPositionalAst::EqualsGenericArgumentCompPositional(
    GenericArgumentCompPositionalAst const &other) const
    -> Ordering {
    // Equality is based on the value of the argument.
    return *Val == *other.Val ? Ordering::equal : Ordering::less;
}

auto spp::asts::GenericArgumentCompPositionalAst::Equals(
    GenericArgumentAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsGenericArgumentCompPositional(*this);
}

auto spp::asts::GenericArgumentCompPositionalAst::PosStart() const
    -> std::size_t {
    // Use the value.
    return Val->PosStart();
}

auto spp::asts::GenericArgumentCompPositionalAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::GenericArgumentCompPositionalAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericArgumentCompPositionalAst>(
        AstClone(Val));
}

auto spp::asts::GenericArgumentCompPositionalAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::GenericArgumentCompPositionalAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

    // Analyse the value.
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Val),
        {sm->CurrentScope}, ERR_ARGS(*Val.get()));
    Val->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::GenericArgumentCompPositionalAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Ensure the argument isn't moved or partially moved (for all conventions)
    Val->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Val, *Val, *sm, true, true, true, true, true, meta);
}

SPP_MOD_END
