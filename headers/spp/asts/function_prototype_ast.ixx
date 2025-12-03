module;
#include <spp/macros.hpp>

export module spp.asts.function_prototype_ast;
import spp.asts.ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct FunctionImplementationAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct GenExpressionAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct SupPrototypeExtensionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}


/**
 * The FunctionPrototypeAst represents the prototype of a function. It defines the structure of a function, including
 * its name, parameters, and return type. The body of the function is defined in the FunctionImplementationAst.
 *
 * @n
 * This ASt is further inherited into the SubroutinePrototypeAst and CoroutinePrototypeAst, which add additional
 * analysis checks.
 */
SPP_EXP_CLS struct spp::asts::FunctionPrototypeAst : virtual Ast, mixins::VisibilityEnabledAst, SupMemberAst, ModuleMemberAst {
private:
    std::vector<std::unique_ptr<analyse::scopes::Scope>> m_generic_substituted_scopes;

public:
    /**
     * Optional @c \@abstractmethod annotation. This is used to indicate that the function is abstract and must be
     * implemented in subclasses.
     */
    AnnotationAst *abstract_annotation;

    /**
     * Optional @c \@virtualmethod annotation. This is used to indicate that the function is virtual and can be
     * overridden in subclasses.
     */
    AnnotationAst *virtual_annotation;

    /**
     * Optional @c \@hot or @c \@cold annotation. This is used to indicate that the function is a hot/cold function,
     * which means it is called frequently/infrequently and should be optimized for performance.
     */
    AnnotationAst *temperature_annotation;

    /**
     * Optional @c \@no_impl annotation. This is used to indicate that the function is not implemented, and the usual
     * type checking rules can be suspended (but this function cannot be called).
     */
    AnnotationAst *no_impl_annotation;

    /**
     * Optional @c \@always_inline, @c \@inline, or @c \@no_inline annotation. This is used to indicate that the
     * function should be inlined, or not inlined, or always inlined.
     */
    AnnotationAst *inline_annotation;

    /**
     * If this function is generic, then all generic implementations / substitutions are stored here, for code
     * generation.
     */
    std::vector<std::unique_ptr<FunctionPrototypeAst>> generic_implementations;

    /**
     * The LLVM generated function for this prototype. This is set during the first pass of code generation, and used
     * for further codegen in the second pass (for function calls, etc).
     */
    llvm::Function *llvm_func;

    /**
     * The list of annotations that are applied to this function prototype. There are quite a lot of annotations that
     * can be applied here, including the typical access modifiers, but also @c \@virtualmethod, @c \@abstractmethod,
     * and @c \@hot/\@cold.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The @c fun or @c cor keyword that represents the start of the function prototype. This is used to indicate that a
     * function is being defined.
     */
    std::unique_ptr<TokenAst> tok_fun;

    /**
     * The name of the function prototype. This is the identifier that is used to refer to the function. Uniqueness is
     * not strictly required, as overloading is supported.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * The optional generic parameter group for the function prototype. This is used to define generic types that the
     * function can use. This is not required, and can be omitted if the function does not use generics.
     */
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;

    /**
     * The parameter group for the function prototype. This is used to define the parameters that the function takes.
     * This is required, and must be present in the function prototype.
     */
    std::unique_ptr<FunctionParameterGroupAst> param_group;

    /**
     * The token that represents the arrow @c -> in the function prototype. This separates the parameters from the
     * return type.
     */
    std::unique_ptr<TokenAst> tok_arrow;

    /**
     * The return type of the function prototype. This is the type that the function will return. This is required, and
     * is never "inferrable" from the expressions inside the function.
     */
    std::shared_ptr<TypeAst> return_type;

    /**
     * The implementation of the function prototype. This is the body of the function, and contains the actual code that
     * will be executed when the function is called.
     */
    std::unique_ptr<FunctionImplementationAst> impl;

    /**
     * Save the original function name prior to AST transformations.
     */
    std::unique_ptr<IdentifierAst> orig_name;

    /**
     * Construct the FunctionPrototypeAst with the arguments matching the members.
     * @param annotations The list of annotations that are applied to this function prototype.
     * @param tok_fun The @c fun or @c cor keyword that represents the start of the function prototype.
     * @param name The name of the function prototype.
     * @param generic_param_group An optional generic parameter group for the function prototype.
     * @param param_group The parameter group for the function prototype.
     * @param tok_arrow The token that represents the arrow @c -> in the function prototype.
     * @param return_type The return type of the function prototype.
     * @param impl The implementation of the function prototype.
     */
    FunctionPrototypeAst(
        decltype(annotations) &&annotations,
        decltype(tok_fun) &&tok_fun,
        decltype(name) &&name,
        decltype(generic_param_group) &&generic_param_group,
        decltype(param_group) &&param_group,
        decltype(tok_arrow) &&tok_arrow,
        decltype(return_type) &&return_type,
        decltype(impl) &&impl);

    ~FunctionPrototypeAst() override;

    SPP_AST_KEY_FUNCTIONS;

private:
    auto m_deduce_mock_class_type() const -> std::shared_ptr<TypeAst>;

public:
    auto print_signature(std::string const &owner) const -> std::string;

    auto register_generic_substituted_scope(std::unique_ptr<analyse::scopes::Scope> &&scope) -> void;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
