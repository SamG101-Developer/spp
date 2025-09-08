#include <spp/asts/statement_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>


auto spp::asts::StatementAst::infer_type(
    ScopeManager *,
    mixins::CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // All statements are inferred as the Void type.
    return generate::common_types::void_type(pos_start());
}
