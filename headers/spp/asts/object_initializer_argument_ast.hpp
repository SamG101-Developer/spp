#pragma once
#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/type_inferrable.hpp>


/**
 * The ObjectInitializerArgumentAst is the base class representing an argument in a object initialization. It is
 * inherited into the "shorthand" and "keyword" variants.
 */
struct spp::asts::ObjectInitializerArgumentAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;

    /**
     * The name of the argument. This is the identifier that is used to refer to the argument in the function call. For
     * shorthand args, this is autofilled by cloning the value, and casting it to an IdentifierAst. Otherwise, it is
     * passed explicitly from the keyword arg parser.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * The expression that is being passed as the argument to the object initialization. Both positional and keyword
     * arguments have a value.
     */
    std::unique_ptr<ExpressionAst> val;

    /**
     * Construct the ObjectInitializerArgumentAst with the arguments matching the members.
     * @param[in] name The name of the argument.
     * @param[in] val The expression that is being passed as the argument to the object initialization.
     */
    explicit ObjectInitializerArgumentAst(
        decltype(name) &&name,
        decltype(val) &&val);

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, mixins::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
