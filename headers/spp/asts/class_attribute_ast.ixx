module;
#include <spp/macros.hpp>

export module spp.asts.class_attribute_ast;
import spp.asts.ast;
import spp.asts.class_member_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClassAttributeAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

/**
 * The ClassAttributeAst represents an attribute of a class. It is defined on the class prototype ast, and is used to
 * add "state" to a type.
 */
SPP_EXP_CLS struct spp::asts::ClassAttributeAst final : Ast, ClassMemberAst, mixins::VisibilityAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The list of annotations that are applied to this class attribute. Typically, access modifiers in this context.
     */
    Vec<Unique<AnnotationAst>> Annotations;

    /**
     * The name of the class attribute. This is the identifier that is used to refer to the attribute on the class, and
     * must be unique to the class.
     */
    Unique<IdentifierAst> Name;

    /**
     * The token that represents the colon @c : in the class attribute definition. This separates the name from the type.
     */
    Unique<TokenAst> TokColon;

    /**
     * The type of the class attribute. This is the type that the attribute will hold, and must be specified.
     */
    Unique<TypeAst> Type;

    /**
     * An optional default value for the class attribute. This is the value that will be assigned to the attribute if no
     * value is provided when creating an instance of the class. Otherwise, standard "default initialization" will be
     * used, which is the default value of the type.
     */
    Unique<ExpressionAst> DefaultVal;

    struct {
        Unique<TypeAst> OriginalType;
    } Source;

    /**
     * Construct the ClassAttributeAst with the arguments matching the members.
     * @param[in] annotations The list of annotations that are applied to this class attribute.
     * @param[in] name The name of the class attribute.
     * @param[in] tok_colon The token that represents the colon @c : in the class attribute definition.
     * @param[in] type The type of the class attribute.
     * @param[in] default_val An optional default value for the class attribute.
     */
    ClassAttributeAst(
        decltype(Annotations) &&annotations,
        decltype(Name) &&name,
        decltype(TokColon) &&tok_colon,
        decltype(Type) &&type,
        decltype(DefaultVal) &&default_val);

    ~ClassAttributeAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage1_PreProcess(Ast *ctx) -> void override;

    auto Stage2_GenTopLvlScopes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage5_LoadSupScopes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::ClassAttributeAst)
