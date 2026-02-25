module;
#include <spp/macros.hpp>

export module spp.asts.annotation_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct IdentifierAst;
}


/**
 * An AnnotationAst is used to represent a non-code generated transformation of behaviour inside an AST. For example,
 * marking a method as @c \@virtualmethod won't generate any code, but will tag the method as virtual, unlocking
 * additional behaviour in the compiler.
 */
SPP_EXP_CLS struct spp::asts::AnnotationAst final : Ast {
    std::unique_ptr<IdentifierAst> name;

    explicit AnnotationAst(
        decltype(name) &&name);
    ~AnnotationAst() override;
    auto to_rust() const -> std::string override;
};
