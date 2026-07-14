module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.statement_ast;
import spp.asts.generate.common_types;
import spp.asts.type_ast;

SPP_MOD_BEGIN
spp::asts::StatementAst::StatementAst() = default;

spp::asts::StatementAst::~StatementAst() = default;

auto spp::asts::StatementAst::InferType(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // All statements are inferred as the Void type.
    using generate::common_types::VoidType;
    CACHE_TYPE_INFERENCE_AND_RETURN(VoidType(PosStart()));
}

auto spp::asts::StatementAst::Terminates() const
    -> bool {
    // By default, statements do not terminate control flow.
    return false;
}

SPP_MOD_END
