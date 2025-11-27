module;
#include <spp/macros.hpp>

export module spp.asts.class_member_ast;
import spp.asts.ast;


/**
 * The ClassMemberAst class is the base class for all members of a class in the abstract syntax tree. This is only
 * applicable to the ClassAttributeAst, but allows for simple expansion in the future,
 */
SPP_EXP_CLS struct spp::asts::ClassMemberAst : virtual Ast {
    ~ClassMemberAst() override;
};
