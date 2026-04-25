module;
#include <spp/macros.hpp>

export module spp.asts.annotation_ast;
import spp.asts.ast;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * An AnnotationAst is used to represent a non-code generated transformation of behaviour inside an AST. For example,
 * marking a method as @c \@virtualmethod won't generate any code, but will tag the method as virtual, unlocking
 * additional behaviour in the compiler.
 */
SPP_EXP_CLS struct spp::asts::AnnotationAst final : virtual Ast {
private:
    FunctionPrototypeAst *m_target;

public:
    /**
     * The token that represents the @c ! sign in the annotation. This introduces the annotation.
     */
    std::unique_ptr<TokenAst> tok_exclamation_mark;

    /**
     * The expression of the annotation. This is the identifier that follows the @c @ sign. It is an ExpressionAst,
     * because it can be postfix, ie @c std::annotations::public etc.
     */
    std::unique_ptr<ExpressionAst> name;

    /**
     * Generic arguments being passed into the annotation call. For example, @c !extends[Copy]() uses generic arguments.
     * Generics are optional.
     */
    std::unique_ptr<GenericArgumentGroupAst> gn_arg_group;

    /**
     * Function arguments being passed into the annotation call. For example, @c !cfg(platform="Windows") uses function
     * arguments. Function arguments are optional, but if generic arguments are present, @c () must be too.
     */
    std::unique_ptr<FunctionCallArgumentGroupAst> fn_arg_group;

    auto _spp_key_function() const -> void override;

    /**
     * Construct the AnnotationAst with the arguments matching the members.
     * @param[in] tok_exclamation_mark The token that represents the @c @ sign in the annotation.
     * @param[in] name The name of the annotation.
     * @param[in] gn_arg_group The generic arguments being passed into the annotation call.
     * @param[in] fn_arg_group The function arguments being passed into the annotation call.
     */
    AnnotationAst(
        decltype(tok_exclamation_mark) &&tok_exclamation_mark,
        decltype(name) &&name,
        decltype(gn_arg_group) &&gn_arg_group,
        decltype(fn_arg_group) &&fn_arg_group);

    ~AnnotationAst() override;

    SPP_AST_KEY_FUNCTIONS;

    /**
     * Custom comparison involves comparing the identifier of the annotation. This makes checking for duplicate
     * annotations easier.
     * @return Whether the identifiers of the annotations are equal.
     */
    auto operator==(AnnotationAst const &that) const -> bool;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(analyse::scopes::ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(analyse::scopes::ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};


SPP_MOD_BEGIN
auto spp::asts::AnnotationAst::_spp_key_function() const -> void {}
SPP_MOD_END
