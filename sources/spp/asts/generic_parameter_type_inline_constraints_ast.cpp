module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
auto spp::asts::GenericParameterTypeInlineConstraintsAst::NewEmpty()
    -> Unique<GenericParameterTypeInlineConstraintsAst> {
    return MakeUnique<GenericParameterTypeInlineConstraintsAst>(
        nullptr, Vec<Unique<TypeAst>>{});
}

spp::asts::GenericParameterTypeInlineConstraintsAst::GenericParameterTypeInlineConstraintsAst(
    decltype(TokColon) &&tok_colon,
    Vec<Unique<TypeAst>> &&constraints) :
    TokColon(std::move(tok_colon)) {
    for (auto &&constraint : std::move(constraints)) {
        this->Constraints.EmplaceBack(Shared(std::move(constraint)));
    }
}

spp::asts::GenericParameterTypeInlineConstraintsAst::~GenericParameterTypeInlineConstraintsAst() = default;

auto spp::asts::GenericParameterTypeInlineConstraintsAst::PosStart() const
    -> std::size_t {
    // Use the ":" token.
    return TokColon->PosStart();
}

auto spp::asts::GenericParameterTypeInlineConstraintsAst::PosEnd() const
    -> std::size_t {
    // Use the last constraint.
    return Constraints.IsEmpty() ? TokColon->PosEnd() : Constraints.Back()->PosEnd();
}

auto spp::asts::GenericParameterTypeInlineConstraintsAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericParameterTypeInlineConstraintsAst>(
        AstClone(TokColon),
        AstCloneVec(Constraints));
}

auto spp::asts::GenericParameterTypeInlineConstraintsAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokColon);
    SPP_STRING_EXTEND(Constraints, " & ");
    SPP_STRING_END;
}

auto spp::asts::GenericParameterTypeInlineConstraintsAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Prepare the fully qualified constraints vector.
    auto fq_constraints = decltype(Constraints)();
    fq_constraints.reserve(Constraints.Len());

    // Analyse each constraint type.
    for (auto const &constraint : Constraints) {
        constraint->Stage7_AnalyseSemantics(sm, meta);
        auto const constraint_type_sym = sm->CurrentScope->GetTypeSymbol(constraint->WithoutGenerics());
        fq_constraints.EmplaceBack(
            constraint_type_sym->FqName()->WithGenerics(std::move(constraint->TypeParts().Back()->GnArgGroup)));
    }

    // Replace the constraints with their fully qualified versions.
    Constraints = std::move(fq_constraints);
}

SPP_MOD_END
