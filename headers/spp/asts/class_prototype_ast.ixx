module;
#include <spp/macros.hpp>

export module spp.asts.class_prototype_ast;
import spp.asts.ast;
import spp.asts.annotation_ast;
import spp.asts.class_implementation_ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.asts.token_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeStatementAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct TypeSymbol;
}


/**
 * The ClassPrototypeAst represents the prototype of a class in the abstract syntax tree. It defines the structure of a
 * class, including its name and any generic parameters it may have. The attributes are defined in the implementation
 * ast for this class, allowing for scoping rules to be made easier.
 */
SPP_EXP_CLS struct spp::asts::ClassPrototypeAst final : virtual Ast, mixins::VisibilityEnabledAst, SupMemberAst, ModuleMemberAst {
    friend struct spp::asts::TypeStatementAst;

private:
    std::vector<std::pair<analyse::scopes::Scope*, std::unique_ptr<ClassPrototypeAst>>> m_generic_substituted_scopes;

    std::shared_ptr<analyse::scopes::TypeSymbol> m_cls_sym;

public:
    /**
     * The list of annotations that are applied to this class prototype. Typically, access modifiers in this context.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The @c cls keyword that represents the start of the class prototype. This is used to indicate that a class is
     * being defined.
     */
    std::unique_ptr<TokenAst> tok_cls;

    /**
     * The name of the class prototype. This is the identifier that is used to refer to the class, and must be unique
     * within the scope.
     */
    std::shared_ptr<TypeAst> name;

    /**
     * An optional generic parameter group for the class prototype. This is used to define generic types that the class
     * can use.
     */
    std::shared_ptr<GenericParameterGroupAst> generic_param_group;

    /**
     * The list of class attributes that are defined on the class prototype. These are the properties that the class
     * will have, and can be accessed through instances of the class.
     */
    std::unique_ptr<ClassImplementationAst> impl;

    /**
     * Construct the ClassPrototypeAst with the arguments matching the members.
     * @param[in] annotations The list of annotations that are applied to this class prototype.
     * @param[in] tok_cls The @c cls keyword that represents the start of the class prototype.
     * @param[in] name The name of the class prototype.
     * @param[in] generic_param_group An optional generic parameter group for the class prototype.
     * @param[in] impl The list of class attributes that are defined on the class prototype.
     */
    ClassPrototypeAst(
        decltype(annotations) &&annotations,
        decltype(tok_cls) &&tok_cls,
        decltype(name) name,
        decltype(generic_param_group) &&generic_param_group,
        decltype(impl) &&impl);

    ~ClassPrototypeAst() override;

    SPP_AST_KEY_FUNCTIONS;

private:
    auto m_generate_symbols(ScopeManager *sm) -> analyse::scopes::TypeSymbol*;

    auto m_fill_llvm_mem_layout(analyse::scopes::ScopeManager *sm, analyse::scopes::TypeSymbol const *type_sym, codegen::LLvmCtx *ctx) -> void;

public:
    auto register_generic_substituted_scope(analyse::scopes::Scope *scope, std::unique_ptr<ClassPrototypeAst> &&new_ast) -> void;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_code_gen_1(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};


spp::asts::ClassPrototypeAst::~ClassPrototypeAst() = default;
