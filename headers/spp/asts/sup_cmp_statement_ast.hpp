#pragma once
#include <spp/asts/_fwd.hpp>
#include <spp/asts/cmp_statement_ast.hpp>
#include <spp/asts/sup_member_ast.hpp>


struct spp::asts::SupCmpStatementAst final : virtual SupMemberAst, virtual CmpStatementAst {
    using CmpStatementAst::CmpStatementAst;
};
