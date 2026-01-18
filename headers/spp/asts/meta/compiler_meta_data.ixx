module;
#include <spp/macros.hpp>

export module spp.asts.meta.compiler_meta_data;
import ankerl;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LoopExpressionAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaDataState;
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::codegen {
    SPP_EXP_CLS struct LLvmCtx;
}


SPP_EXP_CLS struct spp::asts::meta::CompilerMetaDataState {
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
    TokenAst *enclosing_function_cmp;
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
    std::map<std::shared_ptr<IdentifierAst>, std::shared_ptr<TypeAst>, ankerl::ptr_cmp<std::shared_ptr<IdentifierAst>>> infer_source;
    std::map<std::shared_ptr<IdentifierAst>, std::shared_ptr<TypeAst>, ankerl::ptr_cmp<std::shared_ptr<IdentifierAst>>> infer_target;
    ExpressionAst *postfix_expression_lhs;
    ExpressionAst *unary_expression_rhs;
    bool skip_type_analysis_generic_checks;
    analyse::scopes::Scope *type_analysis_type_scope;
    std::shared_ptr<TypeAst> ignore_cmp_generic;
    bool allow_move_deref;
    llvm::BasicBlock *end_bb;
    codegen::LLvmCtx *llvm_ctx;
    llvm::Value *llvm_assignment_target;
    llvm::Value *llvm_assignment_target_type;
    llvm::PHINode *llvm_phi;
    std::map<std::shared_ptr<IdentifierAst>, std::unique_ptr<ExpressionAst>, ankerl::ptr_cmp<std::shared_ptr<IdentifierAst>>> cmp_args;
    std::unique_ptr<ExpressionAst> cmp_result;
};


/**
 * Shared metadata for ASTs, exclusive to the stage of compilation taking place. For example, tracking if an assignment
 * is taking place, when the RHS expression is being analysed.
 */
SPP_EXP_CLS struct spp::asts::meta::CompilerMetaData : CompilerMetaDataState {
private:
    std::stack<CompilerMetaDataState> m_history;

public:
    CompilerMetaData();

    auto save() -> void;

    auto restore(bool heavy = false) -> void;

    auto depth() const -> std::size_t;
};
