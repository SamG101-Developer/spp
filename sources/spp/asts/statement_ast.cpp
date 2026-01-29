module spp.asts.statement_ast;
import spp.asts.generate.common_types;


spp::asts::StatementAst::~StatementAst() = default;


auto spp::asts::StatementAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // All statements are inferred as the Void type.
    return generate::common_types::void_type(pos_start());
}


auto spp::asts::StatementAst::terminates() const
    -> bool {
    // By default, statements do not terminate control flow.
    return false;
}
