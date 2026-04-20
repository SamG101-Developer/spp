module;
#include <spp/macros.hpp>

export module spp.asts:ast;
import :meta;
import spp.abstract;
import spp.analyse.scopes;
import spp.codegen.llvm_ctx;
import llvm;
import std;


namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct TypeAst; // EXP because when this file is imported, TypeAst here is "first seen" occurrence
}

namespace spp::asts::meta {
    SPP_EXP_CLS class AstPrinter;
}


/**
 * The AST base class is inherited by all other AST classes, provided base functionality, including formatted printing
 * and end position identification.
 */
SPP_EXP_CLS struct spp::asts::Ast : AbstractAst {
protected:
    /**
     * The scope of an AST is used when generating top level scopes, to create a simple link between scope and AST.
     */
    analyse::scopes::Scope *m_scope = nullptr;

    /**
     * Create a new AST (base class for all derived ASTs). This constructor is protected to prevent direct instantiation
     * as an AST should always be a specific type of AST, such as a TokenAst, IdentifierAst, etc.
     */
    explicit Ast();

public:
    using ScopeManager = spp::analyse::scopes::ScopeManager;
    using CompilerMetaData = spp::asts::meta::CompilerMetaData;

    ~Ast() override;

    /**
     * The start position is the first position in the source code that contains this AST. An AST will recursively get
     * the start position of the first field, until a TokenAst is reached.
     * @return The first position this AST encompasses.
     */
    SPP_ATTR_NODISCARD virtual auto pos_start() const -> std::size_t = 0;

    /**
     * The end position is the final position in the source code that contains this AST. An AST will recursively get the
     * end position of the final field, until a TokenAst is reached.
     * @return The final position this AST encompasses.
     */
    SPP_ATTR_NODISCARD virtual auto pos_end() const -> std::size_t = 0;

    /**
     * The size of an AST is the number of tokens it encompasses. This is used to determine the size of the AST in the
     * source code, and is used for error reporting. Calculated by subtracting the start position from the end position.
     * @return The size of the AST in tokens.
     */
    SPP_ATTR_NODISCARD auto size() const -> std::size_t;

    /**
     * The clone operator that deep-copies the AST and all its children ASTs. This is used to create a new AST that is a
     * copy of the original AST, preserving its structure and contents. This is useful for creating a new AST that can
     * be modified without affecting the original AST. Can be cast down as needed, as the return type is a
     * @code std::unique_ptr<T>@endcode to the base @c Ast class.
     * @return The cloned AST as a unique pointer to the base Ast class.
     */
    SPP_ATTR_NODISCARD virtual auto clone() const -> std::unique_ptr<Ast> = 0;

    /**
     * Overridable hash function for AST nodes, used for hashing ASTs in data structures (particularly in Ankerl's hash
     * map). Implemented in the @c IdentifierAst and @c TypeIdentifierAst nodes.
     * @return The hash value of the AST.
     */
    SPP_ATTR_NODISCARD virtual auto ankerl_hash() const -> std::size_t;

    /**
     * The preprocessor stage performs AST mutation and transformation before any analysis or scope generate is done.
     * This is key for the function architecture (transforming methods into callable types) etc.
     * @param[in, out] ctx The context AST for this AST.
     */
    virtual auto stage_1_pre_process(AbstractAst *ctx) -> void;

    /**
     * Top level scopes must be generated first, and represent the scopes for modules, classes, functions and
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
     * Resolve any comptime values that haven't got literals assigned to them. This allows for somptime functions to be
     * called, computed, and their values set to the varoables required. Some ASTs will not support this, so they will
     * throw a compile time error, such as loop expressions.
     * @param sm The scope manager to use for resolution.
     * @param meta Associated metadata.
     */
    virtual auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void;

    /**
     * Generate some LLVM IR code from the ASTs. This is IR that is needed for the rest of the program to be generated.
     * Required to make the ASTs order-agnostic.
     * @param[in, out] sm The scope manager to get symbol's memory information from.
     * @param[in, out] meta Metadata to pass between ASTs.
     * @param[in, out] ctx The LLVM context to generate code into.
     * @return The LLVM value generated from this AST.
     */
    virtual auto stage_10_code_gen_1(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value*;

    /**
     * Finish the LLVM IR generation for the remaining (majority) of the ASTs. This will then all get linked together
     * by the compiler to produce an executable, with internal modules based on the SPP code structure.
     * @param[in, out] sm The scope manager to get symbol's memory information from.
     * @param[in, out] meta Metadata to pass between ASTs.
     * @param[in, out] ctx The LLVM context to generate code into.
     * @returns The LLVM value generated from this AST.
     */
    virtual auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value*;

    SPP_ATTR_NODISCARD auto get_ast_scope() const -> analyse::scopes::Scope* {
        return m_scope;
    }

    auto set_ast_scope(analyse::scopes::Scope *scope) -> void {
        m_scope = scope;
    }
};


namespace spp::asts {
    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone(std::unique_ptr<T> const &ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return std::unique_ptr<std::remove_cvref_t<T>>(dynamic_cast<std::remove_cvref_t<T>*>(ast->clone().release()));
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone(std::shared_ptr<T> const &ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return std::unique_ptr<std::remove_cvref_t<T>>(dynamic_cast<std::remove_cvref_t<T>*>(ast->clone().release()));
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone(T *ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return std::unique_ptr<std::remove_cvref_t<T>>(dynamic_cast<std::remove_cvref_t<T>*>(ast->clone().release()));
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_shared(std::unique_ptr<T> &&ast) -> std::shared_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return std::shared_ptr<T>(std::move(ast).release());
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_shared(T *ast) -> std::shared_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return std::shared_ptr<T>(dynamic_cast<T*>(ast->clone().release()));
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_vec(std::vector<T*> const &asts) -> std::vector<std::unique_ptr<T>> {
        std::vector<std::unique_ptr<T>> cloned_asts;
        cloned_asts.reserve(asts.size());
        for (auto const *x : asts) {
            cloned_asts.emplace_back(ast_clone(x));
        }
        return cloned_asts;
    }

    SPP_EXP_FUN template <typename U, typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_vec(std::vector<T*> const &asts) -> std::vector<std::unique_ptr<U>> {
        std::vector<std::unique_ptr<U>> cloned_asts;
        cloned_asts.reserve(asts.size());
        for (auto const *x : asts) {
            cloned_asts.emplace_back(ast_clone(dynamic_cast<U const*>(x)));
        }
        return cloned_asts;
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_vec(std::vector<std::unique_ptr<T>> const &asts) -> std::vector<std::unique_ptr<T>> {
        std::vector<std::unique_ptr<T>> cloned_asts;
        cloned_asts.reserve(asts.size());
        for (auto const &x : asts) {
            cloned_asts.emplace_back(ast_clone(x));
        }
        return cloned_asts;
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_vec(std::vector<std::shared_ptr<T>> const &asts) -> std::vector<std::unique_ptr<T>> {
        std::vector<std::unique_ptr<T>> cloned_asts;
        cloned_asts.reserve(asts.size());
        for (auto const &x : asts) {
            cloned_asts.emplace_back(ast_clone(x));
        }
        return cloned_asts;
    }

    SPP_EXP_FUN template <typename T>
    SPP_ATTR_ALWAYS_INLINE
    auto ast_clone_vec_shared(std::vector<std::shared_ptr<T>> const &asts) -> std::vector<std::shared_ptr<T>> {
        std::vector<std::shared_ptr<T>> cloned_asts;
        cloned_asts.reserve(asts.size());
        for (auto const &x : asts) {
            cloned_asts.emplace_back(x);
        }
        return cloned_asts;
    }

    SPP_EXP_FUN auto ast_name(AbstractAst *ast) -> std::shared_ptr<TypeAst>;

    SPP_EXP_FUN auto ast_body(AbstractAst *ast) -> std::vector<Ast*>;
}
