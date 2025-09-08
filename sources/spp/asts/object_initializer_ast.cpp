#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/object_initializer_ast.hpp>
#include <spp/asts/object_initializer_argument_ast.hpp>
#include <spp/asts/object_initializer_argument_group_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <genex/views/cast.hpp>
#include <genex/views/to.hpp>


spp::asts::ObjectInitializerAst::ObjectInitializerAst(
    decltype(type) &&type,
    decltype(arg_group) &&arg_group) :
    PrimaryExpressionAst(),
    type(std::move(type)),
    arg_group(std::move(arg_group)) {
}


spp::asts::ObjectInitializerAst::~ObjectInitializerAst() = default;


auto spp::asts::ObjectInitializerAst::pos_start() const -> std::size_t {
    return type->pos_start();
}


auto spp::asts::ObjectInitializerAst::pos_end() const -> std::size_t {
    return arg_group->pos_end();
}


auto spp::asts::ObjectInitializerAst::clone() const -> std::unique_ptr<Ast> {
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


auto spp::asts::ObjectInitializerAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(arg_group);
    SPP_PRINT_END;
}


auto spp::asts::ObjectInitializerAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Get the base class symbol (no generics) and check it exists.
    type->without_generics()->stage_7_analyse_semantics(sm, meta);
    const auto base_cls_sym = sm->current_scope->get_type_symbol(*type->without_generics());

    // Generic types cannot have any attributes set | TODO: future with constraints will allow some.
    if (base_cls_sym->is_generic and not arg_group->args.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppArgumentNameInvalidError>().with_args(
            *type, "object initializer", *arg_group->args[0], "object initializer argument").with_scopes({sm->current_scope}).raise();
    }

    // Prepare the object initializer arguments.
    meta->save();
    meta->object_init_type = type.get();
    arg_group->stage_6_pre_analyse_semantics(sm, meta);
    meta->restore();

    // Determine the generic inference source and target values.
    auto generic_infer_source = arg_group->args
        | genex::views::transform([sm, meta](auto &&x) { return std::make_pair(x->name, x->val->infer_type(sm, meta)); })
        | genex::views::to<std::vector>();

    auto generic_infer_target = base_cls_sym->type->impl->members
        | genex::views::ptr
        | genex::views::cast_dynamic<ClassAttributeAst*>()
        | genex::views::transform([base_cls_sym](auto &&x) { return std::make_pair(x->name, base_cls_sym->scope->get_type_symbol(*x->type)->fq_name()); })
        | genex::views::to<std::vector>();

    // Analyse the type and object argument group.
    auto tm = ScopeManager(sm->global_scope, base_cls_sym->scope);
    base_cls_sym->type->impl->stage_7_analyse_semantics(sm, meta);

    meta->save();
    meta->infer_source = std::map(generic_infer_source.begin(), generic_infer_source.end());
    meta->infer_target = std::map(generic_infer_target.begin(), generic_infer_target.end());
    arg_group->stage_7_analyse_semantics(sm, meta);
    type->stage_7_analyse_semantics(sm, meta);
    meta->restore();
}


auto spp::asts::ObjectInitializerAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory of the object argument group.
    arg_group->stage_8_check_memory(sm, meta);
}


auto spp::asts::ObjectInitializerAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // The type of the object initializer is the type being initialized. The conventions are added for dummy types being
    // created into values during other ast's analysis. Types cannot be instantiated as borrows in user code.
    return sm->current_scope->get_type_symbol(*type)->fq_name()->with_convention(ast_clone(type->get_convention()));
}
