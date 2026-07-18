module;
#include <spp/macros.hpp>

export module spp.analyse.utils.expr_utils;
import spp.utils.types;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct ScopeManager;
    SPP_EXP_CLS struct NamespaceSymbol;
    SPP_EXP_CLS struct VariableSymbol;
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::analyse::utils::expr_utils {
    SPP_EXP_CLS struct PrimaryExpressionOptions {
        bool AllowTypeAst = false;
        bool AllowTokenAst = false;
    };

    SPP_EXP_FUN auto IsPrimaryExprTypeValid(
        asts::ExpressionAst const &expr,
        scopes::ScopeManager const &sm,
        PrimaryExpressionOptions &&options = {})
        -> bool;

    SPP_EXP_FUN auto ValidateNoUnreachableCode(
        Vec<asts::StatementAst*> const &members,
        scopes::ScopeManager const &sm)
        -> void;

    SPP_EXP_FUN SPP_ATTR_NORETURN auto RaiseMissingIdentifierAndClosestOptions(
        asts::IdentifierAst const &identifier,
        Vec<Shared<scopes::VariableSymbol>> const &var_symbols,
        Vec<Shared<scopes::NamespaceSymbol>> const &ns_symbols,
        scopes::ScopeManager const &sm)
        -> void;

    SPP_EXP_FUN SPP_ATTR_NORETURN auto RaiseMissingTypeIdentifierAndClosestOptions(
        asts::TypeIdentifierAst const &identifier,
        Vec<Shared<scopes::TypeSymbol>> const &symbols,
        scopes::ScopeManager const &sm)
        -> void;
}
