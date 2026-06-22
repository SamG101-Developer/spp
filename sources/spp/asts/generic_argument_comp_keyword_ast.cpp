module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_argument_comp_keyword_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;
import spp.lex.tokens;

SPP_MOD_BEGIN
auto spp::asts::GenericArgumentCompKeywordAst::FromSym(
    analyse::scopes::VariableSymbol const &sym)
    -> Unique<GenericArgumentCompKeywordAst> {
    // Get the comptime value from the symbol's memory info.
    const auto c = sym.MemInfo->AstCompTime.get();
    Unique<ExpressionAst> value = nullptr;

    // Depending on that the comptime AST is, get the value.
    if (const auto comptime_param = c->To<GenericParameterCompAst>(); comptime_param != nullptr) {
        value = AstClone(comptime_param->Name->To<ExpressionAst>());
    }
    else if (const auto comptime_arg = c->To<GenericArgumentCompAst>(); comptime_arg != nullptr) {
        value = AstClone(comptime_arg->Val);
    }
    if (auto *value_as_type = value->To<TypeIdentifierAst>(); value_as_type != nullptr) {
        value = IdentifierAst::FromType(*AstCloneShared(value_as_type)); // Don't remove "shared_ptr". Todo: Try new "AstCloneShared"
    }

    // Create the GenericArgumentCompKeywordAst with the name and value.
    return MakeUnique<GenericArgumentCompKeywordAst>(
        TypeIdentifierAst::FromIdentifier(*sym.Name), nullptr, std::move(value));
}

spp::asts::GenericArgumentCompKeywordAst::GenericArgumentCompKeywordAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val) :
    GenericArgumentCompAst(std::move(val), utils::OrderableTag::kKeywordArg),
    Name(std::move(name)),
    TokAssign(std::move(tok_assign)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
}

spp::asts::GenericArgumentCompKeywordAst::~GenericArgumentCompKeywordAst() = default;

auto spp::asts::GenericArgumentCompKeywordAst::EqualsGenericArgumentCompKeyword(
    GenericArgumentCompKeywordAst const &other) const
    -> Ordering {
    // Equality is based on the name and value of the argument.
    return *Name == *other.Name and *Val == *other.Val ? Ordering::equal : Ordering::less;
}

auto spp::asts::GenericArgumentCompKeywordAst::Equals(
    GenericArgumentAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsGenericArgumentCompKeyword(*this);
}

auto spp::asts::GenericArgumentCompKeywordAst::PosStart() const
    -> std::size_t {
    // Use the name.
    return Name->PosStart();
}

auto spp::asts::GenericArgumentCompKeywordAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::GenericArgumentCompKeywordAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericArgumentCompKeywordAst>(
        AstClone(Name), AstClone(TokAssign), AstClone(Val));
}

auto spp::asts::GenericArgumentCompKeywordAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokAssign);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::GenericArgumentCompKeywordAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

    // Analyse the value.
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Val, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Val));
    Val->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::GenericArgumentCompKeywordAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Check the value for memory issues.
    Val->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Val, *Val, *sm, true, true, true, true, true, meta);
}

auto spp::asts::GenericArgumentCompKeywordAst::ViewName() const
    -> StrView {
    // Get the name from the keyword part.
    return Name->To<TypeIdentifierAst>()->Name;
}

SPP_MOD_END
