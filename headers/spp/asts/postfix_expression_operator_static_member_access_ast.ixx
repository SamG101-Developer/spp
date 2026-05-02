module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorStaticMemberAccessAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorStaticMemberAccessAst final : PostfixExpressionOperatorAst {
    /**
     * The @c :: token that indicates a static member access operation in a postfix expression.
     */
    Unique<TokenAst> TokDblColon;

    /**
     * The identifier that represents the member being accessed. This is the name of the member in the class or struct.
     */
    Shared<IdentifierAst> Name;

    /**
     * Construct the PostfixExpressionOperatorMemberAccessAst with the arguments matching the members.
     * @param[in] tok_dbl_colon The @c :: token that indicates a static member access operation in a postfix expression.
     * @param[in] name The identifier that represents the member being accessed.
     */
    explicit PostfixExpressionOperatorStaticMemberAccessAst(
        decltype(TokDblColon) &&tok_dbl_colon,
        decltype(Name) &&name);

    ~PostfixExpressionOperatorStaticMemberAccessAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

    SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<Ast*> override;
};
