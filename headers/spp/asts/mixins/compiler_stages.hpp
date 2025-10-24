#pragma once
#include <map>
#include <stack>

#include <spp/codegen/llvm_ctx.hpp>
#include <spp/utils/errors.hpp>
#include <spp/utils/ptr_cmp.hpp>


/// @cond
namespace spp::analyse::scopes {
    struct TypeSymbol;
}

namespace spp::asts {
    struct Ast;
    struct IdentifierAst;
    struct TypeAst;
}

namespace spp::analyse::scopes {
    class ScopeManager;
}

namespace spp::asts::mixins {
    struct CompilerStages;
    struct CompilerMetaDataState;
    struct CompilerMetaData;
}

/// @endcond


/**
 * The compiler stages are a list of functions that each AST can implement, and will be ran recursively from its parent
 * AST. The exceptions are the first 3 functions, which are applies to top level ASTs exclusively.
 */
struct spp::asts::mixins::CompilerStages {
    using ScopeManager = spp::analyse::scopes::ScopeManager;
    CompilerStages() = default;

    virtual ~CompilerStages() = default;

    /**
     * The preprocessor stage performs AST mutation and transformation before any analysis or scope generate is done.
     * This is key for the function architecture (transforming methods into callable types) etc.
     * @param[in, out] ctx The context AST for this AST.
     */
    virtual auto stage_1_pre_process(Ast *ctx) -> void;

    /**
     * Top level scopes must be generated first, and represent the scopes for modues, classes, functions and
     * superimpositions. Symbols may also be generated for global constants. Note that type-aliases cannot be handled
     * here, as there is no guarantee their corresponding "old types" have been generated yet.
     * @param[in, out] sm The scope manager to hold generated scopes.
     */
    virtual auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void;

    /**
     * Aliases at the module and superimposition level are generated here. At this stage, all the classes will have been
     * created, so it is safe for the aliases to be mapped. This separation allows for modules to be processed in any
     * order without identifier errors.
     * @param[in, out] sm The scope manager to hold generated aliases.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * Qualify types that have been written as non-fully-qualified in all ASTs that are not in the bodies of top level
     * ASTs. For example, superimposition target types. This allows the compiler to target types from any scope.
     * @param[in, out] sm The scope manager to identify types in.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * Attach superimposition scopes to the respective target types. This must be done in its own stage as it relies on
     * all types and aliases being generated (superimposing on aliases is fine), and for the types to be qualified.
     * @param[in, out] sm The scope manager to find target type scopes in.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * There are some checks that have to happen after the superscopes have all been attached but must happen before
     * general semantic analysis, as not handling these errors can cause strange analysis errors. This will only run on
     * top level ASTs.
     * @param[in, out] sm The scope manager to do pre-analysis in.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * General analysis of all ASTs, except memory-oriented checks. All identifier checks, type checking, function
     * checking etc is handled here. Almost all semantic errors are thrown from this point.
     * @param[in, out] sm The scope manager to do analysis in.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * All memory oriented checks, such as ownership checking and law of exclusivity enforcement happen in this stage.
     * It is separate from semantic analysis because some ASTs have complex memory checking code, and the flow of the
     * semantic analysis can cause memory errors to be raised when there isn't any. The correct order of memory checks
     * can be enforced separately from semantic analysis this way.
     * @param[in, out] sm The scope manager to get symbol's memory information from.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * Generate some LLVM IR code from the ASTs. This is IR that is needed for the rest of the program to be generated.
     * Required to make the ASTs order-agnostic.
     * @param[in, out] sm The scope manager to get symbol's memory information from.
     * @param[in, out] meta Metadata to pass between ASTs.
     * @param[in, out] ctx The LLVM context to generate code into.
     * @return The LLVM value generated from this AST.
     */
    virtual auto stage_9_code_gen_1(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value*;

    /**
     * Finish the LLVM IR generation for the remaining (majority) of the ASTs. This will then all get linked together
     * by the compiler to produce an executable, with internal modules based on the SPP code structure.
     * @param[in, out] sm The scope manager to get symbol's memory information from.
     * @param[in, out] meta Metadata to pass between ASTs.
     * @param[in, out] llvm_mod The LLVM module to generate code into.
     */
    virtual auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, llvm::Module &llvm_mod) -> void;
};


struct spp::asts::mixins::CompilerMetaDataState {
    double current_stage;
    std::shared_ptr<TypeAst> return_type_overload_resolver_type;
    std::shared_ptr<IdentifierAst> assignment_target;
    std::shared_ptr<TypeAst> assignment_target_type;
    bool ignore_missing_else_branch_for_inference;
    ExpressionAst *case_condition;
    analyse::scopes::TypeSymbol *cls_sym;
    analyse::scopes::Scope *enclosing_function_scope;
    TokenAst *enclosing_function_flavour;
    std::vector<std::shared_ptr<TypeAst>> enclosing_function_ret_type;
    analyse::scopes::Scope *current_lambda_outer_scope;
    FunctionPrototypeAst *target_call_function_prototype;
    bool target_call_was_function_async;
    bool prevent_auto_generator_resume;
    std::shared_ptr<TypeAst> let_stmt_explicit_type;
    ExpressionAst *let_stmt_value;
    bool let_stmt_from_uninitialized;
    bool loop_double_check_active;
    std::size_t current_loop_depth;
    LoopExpressionAst *current_loop_ast;
    std::shared_ptr<std::map<std::size_t, std::tuple<ExpressionAst*, std::shared_ptr<TypeAst>, analyse::scopes::Scope*>>> loop_return_types;
    std::shared_ptr<TypeAst> object_init_type;
    std::map<std::shared_ptr<IdentifierAst>, std::shared_ptr<TypeAst>, spp::utils::SymNameCmp<std::shared_ptr<IdentifierAst>>> infer_source;
    std::map<std::shared_ptr<IdentifierAst>, std::shared_ptr<TypeAst>, spp::utils::SymNameCmp<std::shared_ptr<IdentifierAst>>> infer_target;
    ExpressionAst *postfix_expression_lhs;
    ExpressionAst *unary_expression_rhs;
    bool skip_type_analysis_generic_checks;
    analyse::scopes::Scope *type_analysis_type_scope;
};


/**
 * Shared metadata for ASTs, exclusive to the stage of compilation taking place. For example, tracking if an assignment
 * is taking place, when the RHS expression is being analysed.
 */
struct spp::asts::mixins::CompilerMetaData : CompilerMetaDataState {
private:
    std::stack<CompilerMetaDataState> m_history;

public:
    CompilerMetaData();

    auto save() -> void;

    auto restore() -> void;

    auto depth() const -> std::size_t;
};
