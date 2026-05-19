module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_optional_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::GenericParameterTypeOptionalAst::GenericParameterTypeOptionalAst(
    decltype(Name) &&name,
    decltype(Constraints) &&constraints,
    decltype(TokAssign) &&tok_assign,
    decltype(DefaultVal) &&default_val) :
    GenericParameterTypeAst(std::move(name), std::move(constraints), utils::OrderableTag::kOptionalParam),
    TokAssign(std::move(tok_assign)),
    DefaultVal(std::move(default_val)) {
    // Default the two optional argument groups.
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
}

spp::asts::GenericParameterTypeOptionalAst::~GenericParameterTypeOptionalAst() = default;

auto spp::asts::GenericParameterTypeOptionalAst::PosStart() const
    -> std::size_t {
    // Use the name.
    return Name->PosStart();
}

auto spp::asts::GenericParameterTypeOptionalAst::PosEnd() const
    -> std::size_t {
    // Use the default value.
    return DefaultVal->PosEnd();
}

auto spp::asts::GenericParameterTypeOptionalAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericParameterTypeOptionalAst>(
        AstClone(Name), AstClone(Constraints), AstClone(TokAssign), AstClone(DefaultVal));
}

auto spp::asts::GenericParameterTypeOptionalAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(Constraints);
    SPP_STRING_APPEND(TokAssign);
    SPP_STRING_APPEND(DefaultVal);
    SPP_STRING_END;
}

auto spp::asts::GenericParameterTypeOptionalAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Default behaviour (inline constraints).
    GenericParameterTypeAst::Stage4_QualifyTypes(sm, meta);

    // Handle the default type.
    DefaultVal->Stage7_AnalyseSemantics(sm, meta);
    if (const auto sym = sm->CurrentScope->GetTypeSymbol(DefaultVal->WithoutGenerics()); sym != nullptr) {
        auto temp = sym->FqName()->WithConvention(AstClone(DefaultVal->GetConvention()));
        temp = temp->WithGenerics(AstClone(DefaultVal->TypeParts().Back()->GnArgGroup));
        DefaultVal = std::move(temp);
    }
}

auto spp::asts::GenericParameterTypeOptionalAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the name and default value of the generic type parameter.
    GenericParameterTypeAst::Stage7_AnalyseSemantics(sm, meta);
    DefaultVal->Stage7_AnalyseSemantics(sm, meta);
}

SPP_MOD_END
