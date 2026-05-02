module;
#include <spp/macros.hpp>

export module spp.analyse.utils.expr_utils;
import spp.utils.types;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct ExpressionAst;
}

namespace spp::analyse::utils::expr_utils {
    SPP_EXP_CLS struct PrimaryExpressionOptions {
        bool AllowTypeAst = false;
        bool AllowTokenAst = false;
    };

    SPP_EXP_FUN auto IsPrimaryExprTypeValid(
        asts::ExpressionAst const &expr,
        PrimaryExpressionOptions &&options = {})
        -> bool;

    SPP_EXP_FUN auto ValidateNoUnreachableCode(
        Vec<asts::StatementAst*> const &members,
        scopes::ScopeManager const &sm)
        -> void;
}
