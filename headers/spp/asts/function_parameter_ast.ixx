module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_ast;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

/**
 * The FunctionParameterAst provides a common base to all parameter types in a function prototype. It is inherited by
 * the required, optional, variadic and self parameters, and provides the common functionality for all of them.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterAst : Ast, mixins::OrderableAst {
    /**
     * The local variable declaration for this parameter. This is used to create a local variable for the parameter,
     * using the same syntax as variables, such as destructuring.
     */
    Unique<LocalVariableAst> Var;

    /**
     * The token that represents the @c : colon in the function parameter. This separates the parameter name from the
     * type.
     */
    Unique<TokenAst> TokColon;

    /**
     * The type of the parameter. This is used to specify the type of the parameter, such as @c I32 or @c F64 . This is
     * a required field, as the type of the parameter must be known at compile time.
     */
    Shared<TypeAst> Type;

    struct {
        Shared<TypeAst> OriginalType;
    } Source;

    /**
     * Construct the FunctionParameterAst with the arguments matching the members.
     * @param var The local variable declaration for this parameter.
     * @param tok_colon The token that represents the @c : colon in the function parameter.
     * @param type The type of the parameter.
     * @param order_tag The order tag for this parameter, used to enforce ordering rules.
     */
    FunctionParameterAst(
        decltype(Var) &&var,
        decltype(TokColon) &&tok_colon,
        decltype(Type) type,
        utils::OrderableTag order_tag);

    ~FunctionParameterAst() override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>>;

    SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst>;
};
