module spp.asts;


spp::asts::StatementAst::StatementAst() = default;


spp::asts::StatementAst::~StatementAst() = default;


auto spp::asts::StatementAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // All statements are inferred as the Void type.
    return common_types::void_type(pos_start());
}


auto spp::asts::StatementAst::terminates() const
    -> bool {
    // By default, statements do not terminate control flow.
    return false;
}
