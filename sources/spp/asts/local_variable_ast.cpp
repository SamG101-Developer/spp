module spp.asts.local_variable_ast;


spp::asts::LocalVariableAst::LocalVariableAst() :
    m_mapped_let(nullptr),
    m_from_pattern(false) {
}


spp::asts::LocalVariableAst::~LocalVariableAst() = default;


auto spp::asts::LocalVariableAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return {};
}


auto spp::asts::LocalVariableAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return nullptr;
}
