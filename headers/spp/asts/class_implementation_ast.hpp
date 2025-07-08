#ifndef CLASS_IMPLEMENTATION_AST_HPP
#define CLASS_IMPLEMENTATION_AST_HPP

#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/_fwd.hpp>

namespace spp::parse { class ParserSpp; }


struct spp::asts::ClassImplementationAst final : InnerScopeAst<std::unique_ptr<ClassMemberAst>> {
    friend class parse::ParserSpp;
    using InnerScopeAst::InnerScopeAst;

private:
    static auto new_empty() -> std::unique_ptr<ClassImplementationAst>;
};


#endif //CLASS_IMPLEMENTATION_AST_HPP
