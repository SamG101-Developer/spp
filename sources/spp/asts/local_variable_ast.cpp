module spp.asts.local_variable_ast;
import spp.asts.let_statement_initialized_ast;


spp::asts::LocalVariableAst::LocalVariableAst() :
    m_from_case_pattern(false) {
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


auto spp::asts::LocalVariableAst::mark_from_case_pattern()
    -> void {
    m_from_case_pattern = true;
}
