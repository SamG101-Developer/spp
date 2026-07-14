module;
#include <spp/macros.hpp>

export module spp.asts.annotation_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

/**
 * An AnnotationAst is used to represent a non-code generated transformation of behaviour inside an AST. For example,
 * marking a method as @c \@virtualmethod won't generate any code, but will tag the method as virtual, unlocking
 * additional behaviour in the compiler.
 */
SPP_EXP_CLS struct spp::asts::AnnotationAst final : Ast {
    SPP_GCC_VTABLE_FIX

    /**
     * The token that represents the @c ! sign in the annotation. This introduces the annotation.
     */
    Unique<TokenAst> TokExclamationMark;

    /**
     * The expression of the annotation. This is the identifier that follows the @c @ sign. It is an ExpressionAst,
     * because it can be postfix, ie @c std::annotations::public etc.
     */
    Unique<ExpressionAst> Name;

    /**
     * Generic arguments being passed into the annotation call. For example, @c !extends[Copy]() uses generic arguments.
     * Generics are optional.
     */
    Unique<GenericArgumentGroupAst> GnArgGroup;

    /**
     * Function arguments being passed into the annotation call. For example, @c !cfg(platform="Windows") uses function
     * arguments. Function arguments are optional, but if generic arguments are present, @c () must be too.
     */
    Unique<FunctionCallArgumentGroupAst> FnArgGroup;

    /**
     * Construct the AnnotationAst with the arguments matching the members.
     * @param[in] tok_exclamation_mark The token that represents the @c @ sign in the annotation.
     * @param[in] name The name of the annotation.
     * @param[in] gn_arg_group The generic arguments being passed into the annotation call.
     * @param[in] fn_arg_group The function arguments being passed into the annotation call.
     */
    AnnotationAst(
        decltype(TokExclamationMark) &&tok_exclamation_mark,
        decltype(Name) &&name,
        decltype(GnArgGroup) &&gn_arg_group,
        decltype(FnArgGroup) &&fn_arg_group);

    ~AnnotationAst() override;

    SPP_AST_KEY_FUNCTIONS;

    /**
     * Custom comparison involves comparing the identifier of the annotation. This makes checking for duplicate
     * annotations easier.
     * @return Whether the identifiers of the annotations are equal.
     */
    auto operator==(AnnotationAst const &that) const -> bool;

    /**
     * Standard context marking for the annotation. Attaches the annotated AST into the context attribute on this AST.
     * This is used for further analysis in later stages.
     * @param ctx The AST context of this annotation, which is the annotated AST.
     */
    auto Stage1_PreProcess(Ast *ctx) -> void override;

    /**
     * Standard scope setting for the annotation. Attaches the annotated AST's scope into the scope attribute on this
     * AST. For example, the class scope will be set into this AST if a class is being annotated.
     * @param sm The scope manager to use for setting the scope of this annotation.
     * @param meta Associated metadata.
     */
    auto Stage2_GenTopLvlScopes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    /**
     * Ensure the target annotation definition (as a function), is infact a function, and is a "cmp" function. Also, it
     * must have the "!annotation" annotation too, to tightly couple it to the annotation system.
     * @param sm The scope manager to use for searching the annotation name.
     * @param meta Associated metadata.
     */
    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    /**
     * For builtin annotations, set fields on context ASTs (given the annotated AST is of the correct type). For invalid
     * ASTs being annotated (for example, a "virtual_method" on a class, do nothing). The context AST checker is done
     * later in a unified fashion, for builtin and custom annotations.
     * @param sm The scope manager to use for searching the annotation name.
     * @param meta Associated metadata.
     */
    auto Stage5_LoadSupScopes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    /**
     * Analyse the function argument and generic arguments if they exist, and then do overload resolution for the
     * annotation, checking that it actually exists. This is done for builtin annotations, as seen in the
     * "annotations.spp" header.
     * @param sm The scope manager to use for analysing ASTs and performing overload resolution.
     * @param meta Associated metadata.
     */
    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    /**
     * Resolve the annotation at compile time. This involves evaluating the function and generic arguments at compile
     * time, and ensuring that the context AST is of an acceptable type, according to the "target=" argument required on
     * the "!annotation" definition.
     * @param sm The scope manager to use for analysing ASTs and performing compile time resolution.
     * @param meta Associated metadata.
     */
    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

private:
    FunctionPrototypeAst *_Target;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::AnnotationAst)
