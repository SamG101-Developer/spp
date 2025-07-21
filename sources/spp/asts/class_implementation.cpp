#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/token_ast.hpp>


auto spp::asts::ClassImplementationAst::new_empty() -> std::unique_ptr<ClassImplementationAst> {
    return std::make_unique<ClassImplementationAst>();
}
