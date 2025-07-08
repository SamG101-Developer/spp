#ifndef COMPILER_STAGES_HPP
#define COMPILER_STAGES_HPP

#include <cstdint>

namespace spp::asts {
    struct Ast;
}

namespace spp::analyse::scopes {
    class ScopeManager;
}

namespace spp::asts::mixins {
    struct CompilerStages;
    struct CompilerMetaData;
}


/**
 * The compiler stages are a list of functions that each AST can implement, and will be ran recursively from its parent
 * AST. The exceptions are the first 3 functions, which are applies to top level ASTs exclusively.
 */
struct spp::asts::mixins::CompilerStages {
    using ScopeManager = analyse::scopes::ScopeManager;

    CompilerStages() = default;

    virtual ~CompilerStages() = default;

    /**
     * The preprocessor stage performs AST mutation and transformation before any analysis or scope generate is done.
     * This is key for the function architecture (transforming methods into callable types) etc.
     * @param[in, out] ctx The context AST for this AST.
     */
    virtual auto pre_process(Ast *ctx) -> void;

    /**
     * Top level scopes must be generated first, and represent the scopes for modues, classes, functions and
     * superimpositions. Symbols may also be generated for global constants. Note that type-aliases cannot be handled
     * here, as there is no guarantee their corresponding "old types" have been generated yet.
     * @param[in, out] sm The scope manager to hold generated scopes.
     */
    virtual auto gen_top_level_scopes(ScopeManager *sm) -> void;

    /**
     * Aliases at the module and superimposition level are generated here. At this stage, all the classes will have been
     * created, so it is safe for the aliases to be mapped. This separation allows for modules to be processed in any
     * order without identifier errors.
     * @param[in, out] sm The scope manager to hold generated aliases.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * Qualify types that have been written as non-fully-qualified in all ASTs that are not in the bodies of top level
     * ASTs. For example, superimposition target types. This allows the compiler to target types from any scope.
     * @param[in, out] sm The scope manager to identify types in.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * Attach superimposition scopes to the respective target types. This must be done in its own stage as it relies on
     * all types and aliases being generated (superimposing on aliases is fine), and for the types to be qualified.
     * @param[in, out] sm The scope manager to find target type scopes in.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * There are some checks that have to happen after the superscopes have all been attached but must happen before
     * general semantic analysis, as not handling these errors can cause strange analysis errors.
     * @param[in, out] sm The scope manager to do pre-analysis in.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * General analysis of all ASTs, except memory-oriented checks. All identifier checks, type checking, function
     * checking etc is handled here. Almost all semantic errors are thrown from this point.
     * @param[in, out] sm The scope manager to do analysis in.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * All memory oriented checks, such as ownership checking and law of exclusivity enforcement happen in this stage.
     * It is separate from semantic analysis because some ASTs have complex memory checking code, and the flow of the
     * semantic analysis can cause memory errors to be raised when there isn't any. The correct order of memory checks
     * can be enforced separately from semantic analysis this way.
     * @param[in, out] sm The scope manager to get symbol's memory information from.
     * @param[in, out] meta Metadata to pass between ASTs.
     */
    virtual auto check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * Generate LLVM IR code from the ASTs. This will then all get linked together by the compiler to produce an
     * executable, with internal modules based on the SPP code structure.
     */
    virtual auto code_gen() -> void;
};


/**
 * Shared metadata for ASTs, exclusive to the stage of compilation taking place. For example, tracking if an assignment
 * is taking place, when the RHS expression is being analysed.
 */
struct spp::asts::mixins::CompilerMetaData {
    std::size_t current_stage;
};


#endif //COMPILER_STAGES_HPP
