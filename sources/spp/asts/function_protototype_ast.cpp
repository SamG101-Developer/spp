#include <algorithm>
#include <vector>
#include <genex/views/for_each.hpp>

#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>

#include <genex/views/for_each.hpp>


spp::asts::FunctionPrototypeAst::FunctionPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(tok_fun) &&tok_fun,
    decltype(name) &&name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(param_group) &&param_group,
    decltype(tok_arrow) &&tok_arrow,
    decltype(return_type) &&return_type,
    decltype(impl) &&impl) :
    annotations(std::move(annotations)),
    tok_fun(std::move(tok_fun)),
    name(std::move(name)),
    generic_param_group(std::move(generic_param_group)),
    param_group(std::move(param_group)),
    tok_arrow(std::move(tok_arrow)),
    return_type(std::move(return_type)),
    impl(std::move(impl)) {
}


spp::asts::FunctionPrototypeAst::~FunctionPrototypeAst() = default;


auto spp::asts::FunctionPrototypeAst::pos_start() const -> std::size_t {
    return tok_fun->pos_start();
}


auto spp::asts::FunctionPrototypeAst::pos_end() const -> std::size_t {
    return impl->pos_end();
}


auto spp::asts::FunctionPrototypeAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionPrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(*tok_fun),
        ast_clone(*name),
        ast_clone(*generic_param_group),
        ast_clone(*param_group),
        ast_clone(*tok_arrow),
        ast_clone(*return_type),
        ast_clone(*impl));
}


spp::asts::FunctionPrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_fun);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(param_group);
    SPP_STRING_APPEND(tok_arrow);
    SPP_STRING_APPEND(return_type);
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::FunctionPrototypeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_fun);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(param_group);
    SPP_PRINT_APPEND(tok_arrow);
    SPP_PRINT_APPEND(return_type);
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::FunctionPrototypeAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Get the name of either the module, sup, or sup-ext context name.
    Ast::stage_1_pre_process(ctx);

    // Substitute the "Self" parameter's type with the name of the method.
    const auto self_gen_sub = std::make_unique<GenericArgumentTypeKeywordAst>(generate::common_types::self_type(pos_start()), nullptr, ast_name(ctx));
    auto gen_sub = std::vector<GenericArgumentAst*>();
    gen_sub.emplace_back(self_gen_sub.get());
    if (ast_cast<ModulePrototypeAst>(ctx) != nullptr and param_group->get_self_param()) {
        param_group->get_self_param()->type = param_group->get_self_param()->type->substitute_generics(gen_sub);
    }
    param_group->params | genex::views::for_each([gen_sub](auto &&x) { x->type = x->type->substitute_generics(gen_sub); });
    return_type = return_type->substitute_generics(gen_sub);

    // Preprocess the annotations.
    annotations | genex::views::for_each([ctx](auto &&x) { x->stage_1_pre_process(ctx); });
}
