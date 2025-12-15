module spp.codegen.llvm_type_registration;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.analyse.scopes.symbols;
import spp.asts.boolean_literal_ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_mangle;
import spp.lex.tokens;
import genex;
import llvm;
import std;


auto spp::codegen::register_llvm_type_info(
    asts::ClassPrototypeAst const *cls_proto,
    LLvmCtx *ctx)
    -> void {
    // $ types are 0-size types in LLVM.
    if (cls_proto->name->operator std::string()[0] == '$') {
        const auto zero_size_struct = llvm::StructType::create(*ctx->context, mangle::mangle_type_name(*cls_proto->get_cls_sym()));
        zero_size_struct->setBody({}, true);
        cls_proto->get_cls_sym()->llvm_info->llvm_type = zero_size_struct;
    }

    // Get the class symbol from the current scope.
    const auto scope = cls_proto->get_ast_scope();
    const auto cls_sym = scope->ty_sym;

    // For compiler known types, specialize the llvm type symbols.
    const auto ancestor_names = scope->ancestors()
        | genex::views::drop_last(1)
        | genex::views::transform([](auto *x) { return x->name_as_string(); })
        | genex::views::materialize
        | genex::views::reverse
        | genex::to<std::vector>();

    if (ancestor_names == std::vector<std::string>{"std", "void", "Void"}) {
        cls_sym->llvm_info->llvm_type = llvm::Type::getVoidTy(*ctx->context);
        return;
    }

    if (ancestor_names == std::vector<std::string>{"std", "boolean", "Bool"}) {
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt1Ty(*ctx->context);
        return;
    }

    if (ancestor_names[0] == "std" and ancestor_names[1] == "num" and ancestor_names[2].starts_with("sized") and ancestor_names[3].starts_with("Sized")) {
        const auto type_part = ancestor_names[2];
        const auto bit_width_ast = scope->ty_sym->fq_name()->type_parts().back()->generic_arg_group->comp_at("bit_width")->val->to<asts::IntegerLiteralAst>();
        if (bit_width_ast == nullptr) { return; }

        const auto bit_width = std::stoul(scope->ty_sym->fq_name()->type_parts().back()->generic_arg_group->comp_at("bit_width")->val->to<asts::IntegerLiteralAst>()->val->token_data);
        if (type_part == "sized_integer") {
            cls_sym->llvm_info->llvm_type = llvm::Type::getIntNTy(*ctx->context, static_cast<unsigned int>(bit_width));
            return;
        }

        if (type_part.starts_with("sized_floating_point")) {
            if (bit_width == 8) {
                cls_sym->llvm_info->llvm_type = llvm::Type::getFloatingPointTy(*ctx->context, llvm::APFloatBase::Float8E4M3());
            }
            else if (bit_width == 16) {
                cls_sym->llvm_info->llvm_type = llvm::Type::getFloatingPointTy(*ctx->context, llvm::APFloatBase::IEEEhalf());
            }
            else if (bit_width == 32) {
                cls_sym->llvm_info->llvm_type = llvm::Type::getFloatingPointTy(*ctx->context, llvm::APFloatBase::IEEEsingle());
            }
            else if (bit_width == 64) {
                cls_sym->llvm_info->llvm_type = llvm::Type::getFloatingPointTy(*ctx->context, llvm::APFloatBase::IEEEdouble());
            }
            else if (bit_width == 128) {
                cls_sym->llvm_info->llvm_type = llvm::Type::getFloatingPointTy(*ctx->context, llvm::APFloatBase::IEEEquad());
            }
            return;
        }
    }

    // Empty struct, will fill in stage_10 when all attributes' types have been generated.
    cls_sym->llvm_info->llvm_type = llvm::StructType::create(*ctx->context, mangle::mangle_type_name(*cls_sym));
}
