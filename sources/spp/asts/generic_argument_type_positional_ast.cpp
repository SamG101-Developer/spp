module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_type_positional_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::GenericArgumentTypePositionalAst::GenericArgumentTypePositionalAst(
    decltype(Val) val) :
    GenericArgumentTypeAst(std::move(val), utils::OrderableTag::kPositionalArg) {
}

spp::asts::GenericArgumentTypePositionalAst::~GenericArgumentTypePositionalAst() = default;

auto spp::asts::GenericArgumentTypePositionalAst::EqualsGenericArgumentTypePositional(
    GenericArgumentTypePositionalAst const &other) const
    -> Ordering {
    // Equality is based on the value of the argument.
    return *Val == *other.Val ? Ordering::equal : Ordering::less;
}

auto spp::asts::GenericArgumentTypePositionalAst::Equals(
    GenericArgumentAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsGenericArgumentTypePositional(*this);
}

auto spp::asts::GenericArgumentTypePositionalAst::PosStart() const
    -> std::size_t {
    // Use the val.
    return Source.OriginalValPosStart;
}

auto spp::asts::GenericArgumentTypePositionalAst::PosEnd() const
    -> std::size_t {
    // Use the val.
    return Source.OriginalValPosEnd;
}

auto spp::asts::GenericArgumentTypePositionalAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericArgumentTypePositionalAst>(
        AstCloneShared(Val));
}

auto spp::asts::GenericArgumentTypePositionalAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::GenericArgumentTypePositionalAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    if (Val->IsSelfType() and sm->CurrentScope->AstNode != nullptr and sm->CurrentScope->AstNode->To<InnerScopeExpressionAst>() == nullptr) { return; }
    if (Val->IsSelfType()) { Val = sm->CurrentScope->GetEnclosingSelfType(*meta); }
    Val->Stage7_AnalyseSemantics(sm, meta);

    // Analyse the name and value of the generic type argument.
    const auto tmp1 = sm->CurrentScope->GetTypeSymbol(Val);
    const auto tmp2 = tmp1->FqName();
    auto tmp3 = AstClone(Val->GetConvention());
    const auto tmp4 = tmp2->WithConvention(std::move(tmp3));
    Val = tmp4;
}

SPP_MOD_END
