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
     * @param[in] tok_at_sign The token that represents the @c @ sign in the annotation.
     * @param[in] name The name of the annotation.
     * @param[in] gn_arg_group The generic arguments being passed into the annotation call.
     * @param[in] fn_arg_group The function arguments being passed into the annotation call.
     */
    AnnotationAst(
        decltype(tok_exclamation_mark) &&tok_at_sign,
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

    /**
     * Preprocessing an @c AnnotationAst involves setting tags on certain contextual ASTs, if they are the correct type.
     * Errors are not thrown for incorrect contextual ASTs, as a scope manager is not present at this stage, so cannot
     * provide the correct error formatter to the semantic error being thrown.
     * @param[in] ctx The context that this annotation is being applied to.
     */
    auto stage_1_pre_process(Ast *ctx) -> void override;

    /**
     * There are three key checks that are performed in this stage:
     *      1. The context AST is checked to be of a compatible type for the annotation. For example, a
     *      @c \@virtualmethod annotation can only be applied to a method, and not a free function or class.
     *      2. The annotation is checked for conflicting annotations on the same context AST. For example, a
     *      @c \@public annotation cannot be applied to a method that is already marked as @c @private.
     *      3. The annotation is checked to be a builtin annotation, that is known to the compiler. Custom annotations
     *      are not supported.
     * @param[in] sm The scope manager to check the context AST against.
     * @param[in,out] meta Associated metadata.
     */
    auto stage_2_gen_top_level_scopes(analyse::scopes::ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(analyse::scopes::ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};


SPP_MOD_BEGIN
auto spp::asts::AnnotationAst::_spp_key_function() const -> void {}
SPP_MOD_END
