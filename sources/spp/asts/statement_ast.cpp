module;
#include <spp/macros.hpp>

module spp.asts.statement_ast;
import spp.asts.generate.common_types;
import spp.asts.type_ast;

SPP_MOD_BEGIN
spp::asts::StatementAst::StatementAst() = default;

spp::asts::StatementAst::~StatementAst() = default;

auto spp::asts::StatementAst::InferType(
    ScopeManager *,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // All statements are inferred as the Void type.
    using generate::common_types::VoidType;
    return VoidType(PosStart());
}

auto spp::asts::StatementAst::Terminates() const
    -> bool {
    // By default, statements do not terminate control flow.
    return false;
}

SPP_MOD_END
