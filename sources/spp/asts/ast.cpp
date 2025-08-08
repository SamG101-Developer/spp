#include <spp/asts/ast.hpp>


spp::asts::Ast::Ast() = default;


auto spp::asts::Ast::size() const -> std::size_t {
    return pos_end() - pos_start();
}
