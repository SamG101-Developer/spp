#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::LocalVariableAst::LocalVariableAst() :
    m_mapped_let(nullptr) {
}


spp::asts::LocalVariableAst::~LocalVariableAst() = default;


auto spp::asts::LocalVariableAst::extract_names() const -> std::vector<std::shared_ptr<IdentifierAst>> {
    return {};
}


auto spp::asts::LocalVariableAst::extract_name() const -> std::shared_ptr<IdentifierAst> {
    return nullptr;
}
