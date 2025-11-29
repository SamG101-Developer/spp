module;
#include <genex/to_container.hpp>
#include <genex/actions/sort.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/views/cast_dynamic.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/join.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/transform.hpp>

#include <spp/macros.hpp>

module spp.asts.object_initializer_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.asts.convention_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


spp::asts::ObjectInitializerAst::ObjectInitializerAst(
    decltype(type) type,
    decltype(arg_group) &&arg_group) :
    PrimaryExpressionAst(),
    type(std::move(type)),
    arg_group(std::move(arg_group)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->arg_group);
}


spp::asts::ObjectInitializerAst::~ObjectInitializerAst() = default;


auto spp::asts::ObjectInitializerAst::pos_start() const
    -> std::size_t {
    return type->pos_start();
}


auto spp::asts::ObjectInitializerAst::pos_end() const
    -> std::size_t {
    return arg_group->pos_end();
}


auto spp::asts::ObjectInitializerAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ObjectInitializerAst>(
        ast_clone(type),
        ast_clone(arg_group));
}


spp::asts::ObjectInitializerAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(arg_group);
    SPP_STRING_END;
}


auto spp::asts::ObjectInitializerAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(arg_group);
    SPP_PRINT_END;
}


auto spp::asts::ObjectInitializerAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Get the base class symbol (no generics) and check it exists.
    meta->save();
    meta->skip_type_analysis_generic_checks = true;
    type->without_generics()->stage_7_analyse_semantics(sm, meta);
    meta->restore();
    const auto base_cls_sym = sm->current_scope->get_type_symbol(type->without_generics());

    // Generic types cannot have any attributes set | TODO: future with constraints will allow some.
    if (base_cls_sym->is_generic and not arg_group->args.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppObjectInitializerGenericWithArgsError>().with_args(
            *type, *arg_group->args[0]).with_scopes({sm->current_scope}).raise();
    }

    // Generic types being initialized uses pure default initialization, so there is no inference to be done.
    if (base_cls_sym->is_generic) {
        return;
    }

    // Prepare the object initializer arguments.
    meta->save();
    meta->object_init_type = type->without_generics();
    arg_group->stage_6_pre_analyse_semantics(sm, meta);
    meta->restore();

    // Determine the generic inference source and target values.
    auto generic_infer_source = arg_group->args
        | genex::views::transform([sm, meta](auto &&x) { return std::make_pair(x->name, x->val->infer_type(sm, meta)); })
        | genex::to<std::vector>();

    auto generic_infer_target = base_cls_sym->type->impl->members
        | genex::views::ptr
        | genex::views::cast_dynamic<ClassAttributeAst*>()
        | genex::views::transform([base_cls_sym](auto &&x) { return std::make_pair(x->name, base_cls_sym->scope->get_type_symbol(x->type)->fq_name()); })
        | genex::to<std::vector>();

    // Analyse the type and object argument group.
    auto tm = ScopeManager(sm->global_scope, base_cls_sym->scope);
    base_cls_sym->type->impl->stage_7_analyse_semantics(&tm, meta);

    meta->save();
    meta->infer_source = {generic_infer_source.begin(), generic_infer_source.end()};
    meta->infer_target = {generic_infer_target.begin(), generic_infer_target.end()};
    type->stage_7_analyse_semantics(sm, meta);
    meta->restore();

    meta->save();
    meta->object_init_type = type;
    arg_group->stage_7_analyse_semantics(sm, meta);
    meta->restore();
}


auto spp::asts::ObjectInitializerAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the object argument group.
    arg_group->stage_8_check_memory(sm, meta);
}


auto spp::asts::ObjectInitializerAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create an empty struct based on the llvm type.
    const auto llvm_type = sm->current_scope->get_type_symbol(type)->llvm_info->llvm_type;
    const auto aggregate = ctx->builder.CreateAlloca(llvm_type, nullptr, "obj_init.aggregate");

    // Re-order the arguments to match the fields on the type.
    const auto cls_sym = sm->current_scope->get_type_symbol(type);
    const auto attributes = std::vector{cls_sym->scope}
        | genex::views::concat(cls_sym->scope->sup_scopes())
        | genex::views::transform([](auto const &scope) { return ast_cast<ClassPrototypeAst>(scope->ast)->impl.get(); })
        | genex::views::transform([](auto const &impl) { return impl->members | genex::views::ptr | genex::to<std::vector>(); })
        | genex::views::join
        | genex::views::cast_dynamic<ClassAttributeAst*>()
        | genex::to<std::vector>();

    // Sort the arguments (by name) to match the type's attributes.
    auto sorted_args = arg_group->args
        | genex::views::ptr
        | genex::to<std::vector>();

    sorted_args |= genex::actions::sort([&attributes](auto const &a, auto const &b) {
        const auto a_index = genex::algorithms::position(attributes, [&a](auto const &attr) { return *attr->name == *a->name; });
        const auto b_index = genex::algorithms::position(attributes, [&b](auto const &attr) { return *attr->name == *b->name; });
        return a_index < b_index;
    });

    // Set each attribute value in the aggregate.
    for (auto i = 0uz; i < sorted_args.size(); ++i) {
        const auto &arg = sorted_args[i];
        const auto attr_ptr = ctx->builder.CreateStructGEP(llvm_type, aggregate, static_cast<std::uint32_t>(i), arg->name->val);
        const auto val = arg->val->stage_10_code_gen_2(sm, meta, ctx);
        ctx->builder.CreateStore(val, attr_ptr);
    }

    // Return the aggregate.
    return ctx->builder.CreateLoad(llvm_type, aggregate, "obj_init.result");
}


auto spp::asts::ObjectInitializerAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // The type of the object initializer is the type being initialized. The conventions are added for dummy types being
    // created into values during other ast's analysis. Types cannot be instantiated as borrows in user code.
    return sm->current_scope->get_type_symbol(type)->fq_name()->with_convention(ast_clone(type->get_convention()));
}
