module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast; // TODO: GCC BUG REQUIRES THIS
    SPP_EXP_CLS template <typename T>
    struct InnerScopeAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

/**
 * The InnerScopeAst class represents an inner scope in the abstract syntax tree. It is used to define a scope for top
 * level structure implementations (classes, functions, etc.). Bodies of expression asts, such as the @c case or @c loop
 * structures use the InnerScopeExpressionAst class, which is a specialization of this class, with support for type
 * inference etc.
 * @tparam T The type of the members in the inner scope.
 */
SPP_EXP_CLS template <typename T>
struct spp::asts::InnerScopeAst : Ast {
    /**
     * The @c { token that represents the start of the inner scope. This is used to indicate the beginning of the scope
     * and is typically followed by a list of members or statements that belong to this scope.
     */
    Unique<TokenAst> TokL;

    /**
     * The list of members in the inner scope. They are all the @c T type, or a derived type. This allows for a flexible
     * structure where different types of ASTs can be included in the same inner scope, as long as they derive from
     * a common base class.
     */
    Vec<T> Members;

    /**
     * The @c } token that represents the end of the inner scope. This is used to indicate the end of the scope after
     * all the members or statements have been defined.
     */
    Unique<TokenAst> TokR;

    static auto NewEmpty() -> Unique<InnerScopeAst>;

    /**
     * Construct the InnerScopeAst with the arguments matching the members.
     * @param[in] tok_l The @c { token that represents the start of the inner scope.
     * @param[in] members The list of members in the inner scope.
     * @param[in] tok_r The @c } token that represents the end of the inner scope.
     */
    InnerScopeAst(
        decltype(TokL) &&tok_l,
        decltype(Members) &&members,
        decltype(TokR) &&tok_r);

    ~InnerScopeAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    SPP_ATTR_NODISCARD auto FinalMember() const -> Ast*;

protected:
    InnerScopeAst();
};
