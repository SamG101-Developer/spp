#pragma once
#include <spp/asts/_fwd.hpp>
#include <spp/asts/sup_member_ast.hpp>
#include <spp/asts/type_statement_ast.hpp>


struct spp::asts::SupTypeStatementAst final : virtual SupMemberAst, virtual TypeStatementAst {
    using TypeStatementAst::TypeStatementAst;
};
