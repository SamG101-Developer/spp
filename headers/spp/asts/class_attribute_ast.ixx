module;
#include <spp/macros.hpp>

export module spp.asts.class_attribute_ast;
import spp.asts.ast;
import spp.asts.class_member_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct ClassAttributeAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The ClassAttributeAst represents an attribute of a class. It is defined on the class prototype ast, and is used to
 * add "state" to a type.
 */
SPP_EXP_CLS struct spp::asts::ClassAttributeAst final : Ast, ClassMemberAst {
    std::vector<std::unique_ptr<AnnotationAst>> annotations;
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<TypeAst> type;
    std::unique_ptr<ExpressionAst> default_val;

    explicit ClassAttributeAst(
        decltype(annotations) &&annotations,
        decltype(name) &&name,
        decltype(type) &&type,
        decltype(default_val) &&default_val);
    ~ClassAttributeAst() override;
    auto to_rust() const -> std::string override;
};
