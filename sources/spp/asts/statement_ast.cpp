module spp.asts.statement_ast;
import spp.asts.generate.common_types;


auto spp::asts::StatementAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // All statements are inferred as the Void type.
    return generate::common_types::void_type(pos_start());
}
