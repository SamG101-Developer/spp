module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_type_keyword_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;
import spp.lex.tokens;

SPP_MOD_BEGIN
auto spp::asts::GenericArgumentTypeKeywordAst::FromSym(
    analyse::scopes::TypeSymbol const &sym)
    -> Unique<GenericArgumentTypeKeywordAst> {
    // Extract the value from the symbol's scope, if it exists.
    auto value = sym.LinkedScope->TySym->FqName()->WithConvention(AstClone(sym.Convention.get()));

    // Wrap the value into a type argument.
    return MakeUnique<GenericArgumentTypeKeywordAst>(
        sym.Name, nullptr, std::move(value));
}

spp::asts::GenericArgumentTypeKeywordAst::GenericArgumentTypeKeywordAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) val) :
    GenericArgumentTypeAst(std::move(val), utils::OrderableTag::KEYWORD_ARG),
    Name(std::move(name)),
    TokAssign(std::move(tok_assign)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
}

spp::asts::GenericArgumentTypeKeywordAst::~GenericArgumentTypeKeywordAst() = default;

auto spp::asts::GenericArgumentTypeKeywordAst::EqualsGenericArgumentTypeKeyword(
    GenericArgumentTypeKeywordAst const &other) const
    -> Ordering {
    // Equality is based on the name and value of the argument.
    return *Name == *other.Name and *Val == *other.Val ? Ordering::equal : Ordering::less;
}

auto spp::asts::GenericArgumentTypeKeywordAst::Equals(
    GenericArgumentAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsGenericArgumentTypeKeyword(*this);
}

auto spp::asts::GenericArgumentTypeKeywordAst::PosStart() const
    -> std::size_t {
    // Use the name.
    return Name->PosStart();
}

auto spp::asts::GenericArgumentTypeKeywordAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::GenericArgumentTypeKeywordAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericArgumentTypeKeywordAst>(
        AstCloneShared(Name), AstClone(TokAssign), AstCloneShared(Val));
}

auto spp::asts::GenericArgumentTypeKeywordAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokAssign);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::GenericArgumentTypeKeywordAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the name and value of the generic type argument.
    Val->Stage7_AnalyseSemantics(sm, meta);
    const auto tmp1 = sm->CurrentScope->GetTypeSymbol(Val);
    const auto tmp2 = tmp1->FqName();
    auto tmp3 = AstClone(Val->GetConvention());
    const auto tmp4 = tmp2->WithConvention(std::move(tmp3));
    Val = tmp4;
}

auto spp::asts::GenericArgumentTypeKeywordAst::ViewName() const
    -> StrView {
    // Get the name from the keyword part.
    return Name->To<TypeIdentifierAst>()->Name;
}

SPP_MOD_END
