module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS template <typename T>
    struct InnerScopeAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The InnerScopeAst class represents an inner scope in the abstract syntax tree. It is used to define a scope for top
 * level structure implementations (classes, functions, etc.). Bodies of expression asts, such as the @c case or @c loop
 * structures use the InnerScopeExpressionAst class, which is a specialization of this class, with support for type
 * inference etc.
 * @tparam T The type of the members in the inner scope.
 */
SPP_EXP_CLS template <typename T>
struct spp::asts::InnerScopeAst : virtual Ast {
    /**
     * The @c { token that represents the start of the inner scope. This is used to indicate the beginning of the scope
     * and is typically followed by a list of members or statements that belong to this scope.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of members in the inner scope. They are all the @c T type, or a derived type. This allows for a flexible
     * structure where different types of ASTs can be included in the same inner scope, as long as they derive from
     * a common base class.
     */
    std::vector<T> members;

    /**
     * The @c } token that represents the end of the inner scope. This is used to indicate the end of the scope after
     * all the members or statements have been defined.
     */
    std::unique_ptr<TokenAst> tok_r;

protected:
    InnerScopeAst();

public:
    /**
     * Construct the InnerScopeAst with the arguments matching the members.
     * @param[in] tok_l The @c { token that represents the start of the inner scope.
     * @param[in] members The list of members in the inner scope.
     * @param[in] tok_r The @c } token that represents the end of the inner scope.
     */
    InnerScopeAst(
        decltype(tok_l) &&tok_l,
        decltype(members) &&members,
        decltype(tok_r) &&tok_r);

    ~InnerScopeAst() override;

    SPP_AST_KEY_FUNCTIONS;

    static auto new_empty() -> std::unique_ptr<InnerScopeAst>;

    SPP_ATTR_NODISCARD auto final_member() const -> Ast*;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
