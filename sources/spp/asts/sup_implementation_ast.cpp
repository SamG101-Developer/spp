#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/token_ast.hpp>


auto spp::asts::SupImplementationAst::new_empty() -> std::unique_ptr<SupImplementationAst> {
    return std::make_unique<SupImplementationAst>();
}
