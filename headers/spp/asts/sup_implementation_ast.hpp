#ifndef SUP_IMPLEMENTATION_AST_HPP
#define SUP_IMPLEMENTATION_AST_HPP

#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/_fwd.hpp>

namespace spp::parse { class ParserSpp; }


struct spp::asts::SupImplementationAst final : InnerScopeAst<std::unique_ptr<SupMemberAst>> {
    friend class parse::ParserSpp;
    using InnerScopeAst::InnerScopeAst;

private:
    static auto new_empty() -> std::unique_ptr<SupImplementationAst>;
};


#endif //SUP_IMPLEMENTATION_AST_HPP
