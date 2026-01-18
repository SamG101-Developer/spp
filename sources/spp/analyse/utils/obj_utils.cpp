module spp.analyse.utils.obj_utils;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import genex;


auto spp::analyse::utils::obj_utils::set_attribute_value(
    asts::ObjectInitializerAst *object,
    asts::Ast *attribute,
    std::unique_ptr<asts::ExpressionAst> &&value,
    scopes::ScopeManager const *sm)
    -> void {
    // Firstly, we need to split the "attribute" as it may be a dotted path.
    auto attr_path = std::vector<std::shared_ptr<asts::IdentifierAst>>();
    while (true) {
        // Ensure we are looking at a postfix expression.
        const auto postfix = attribute->to<asts::PostfixExpressionAst>();
        if (postfix == nullptr) { break; }

        // Ensure the operator is a runtime member access.
        const auto member_access = postfix->op->to<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>();
        if (member_access == nullptr) { break; }

        // Push the member name to the path and continue down the lhs.
        attr_path.push_back(member_access->name);
        attribute = postfix->lhs.get();
    }

    // Reverse the attribute path to get the correct order.
    attr_path |= genex::actions::reverse;
    attr_path |= genex::actions::pop_front;

    // For each attribute, check if the initializer exists for tis attribute.
    // For example, if we have x.y.z = 5, and the types are x->X, y->Y, z->Z,
    //  we need to ensure that X has a Y initializer, which has a Z initializer.
    //  each inner initializer may exist, may not, so check and create if needed.
    auto current_obj_init = object;
    auto current_obj_type = object->type;
    auto current_obj_sym = sm->current_scope->get_type_symbol(current_obj_type);

    for (auto const &attr_name : attr_path) {
        auto is_final = attr_name == attr_path.back();
        current_obj_type = current_obj_sym->scope->get_var_symbol(attr_name)->type;
        current_obj_sym = current_obj_sym->scope->get_type_symbol(current_obj_type);

        // Check if the attribute already exists in the current object initializer.
        const auto found = genex::contains(current_obj_init->arg_group->get_all_args(), *attr_name, [](auto const *x) -> decltype(auto) { return *x->name; });
        if (found) {
            auto const &arg = **genex::find_if(current_obj_init->arg_group->get_all_args(), [&](auto const *x) { return *x->name == *attr_name; });
            const auto obj = arg.val->to<asts::ObjectInitializerAst>();

            // Case: attribute exists, but not as an object initializer. Replace it, but keep old object as the "else"
            // in the initializer, to use all of its other attributes.
            if (obj == nullptr) {
                const auto old_obj_init = current_obj_init;
                auto new_init = is_final
                    ? std::move(value)
                    : std::make_unique<asts::ObjectInitializerAst>(current_obj_sym->fq_name(), nullptr);
                current_obj_init = new_init->to<asts::ObjectInitializerAst>();

                old_obj_init->arg_group->args.emplace_back(
                    std::make_unique<asts::ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(new_init)));
                old_obj_init->arg_group->args.emplace_back(
                    asts::ObjectInitializerArgumentShorthandAst::create_autofill(asts::ast_clone(arg.val)));
                continue;
            }

            // Case: attribute exists, and as an object initializer. In this case, just forward into the next
            // initializer.
            if (not is_final) {
                current_obj_init = obj;
                current_obj_type = current_obj_sym->scope->get_var_symbol(attr_name)->type;
                current_obj_sym = current_obj_sym->scope->get_type_symbol(current_obj_type);
                continue;
            }
            current_obj_init->arg_group->args.emplace_back(
                std::make_unique<asts::ObjectInitializerArgumentKeywordAst>(attr_name, nullptr, std::move(value)));
        }

        // Case: attribute does not exist, so create a new object initializer for it.
        else {
            const auto old_obj_init = current_obj_init;
            auto new_init = is_final
                ? std::move(value)
                : std::make_unique<asts::ObjectInitializerAst>(current_obj_sym->fq_name(), nullptr);
            current_obj_init = new_init->to<asts::ObjectInitializerAst>();

            old_obj_init->arg_group->args.emplace_back(
                std::make_unique<asts::ObjectInitializerArgumentKeywordAst>(ast_clone(attr_name), nullptr, std::move(new_init)));
        }
    }
}


auto spp::analyse::utils::obj_utils::get_attribute_value(
    asts::ObjectInitializerAst const *object,
    asts::IdentifierAst const *attribute)
    -> std::unique_ptr<asts::ExpressionAst> {
    // Check each argument in the object initializer for the target attribute.
    for (auto const &arg : object->arg_group->get_all_args()) {
        if (*arg->name == *attribute) { return asts::ast_clone(arg->val); }
    }
    return nullptr;
}
