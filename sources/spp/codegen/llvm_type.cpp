module spp.codegen.llvm_type;
import spp.analyse.scopes;
import spp.analyse.utils.scope_utils;
import spp.asts;
import spp.codegen.llvm_mangle;
import spp.lex;

import genex;
import llvm;
import std;


auto spp::codegen::register_llvm_type_info(
    asts::ClassPrototypeAst const *cls_proto,
    LlvmCtx const *ctx)
    -> void {
    // Note: because symbols have a convention attached to them, retrieval handles pointer logic for borrows.

    // $ types are 0-size types in LLVM.
    if (cls_proto->name->to_string()[0] == '$') {
        const auto type_sym = dynamic_cast<analyse::scopes::TypeSymbol*>(cls_proto->get_cls_sym().get());
        const auto zero_size_struct = llvm::StructType::create(*ctx->context, mangle::mangle_type_name(*type_sym));
        zero_size_struct->setBody({}, true);
        const auto type_sym = dynamic_cast<analyse::scopes::TypeSymbol*>(cls_proto->get_cls_sym().get());
        type_sym->llvm_info->llvm_type = zero_size_struct;
    }

    // Get the class symbol from the current scope.
    const auto scope = cls_proto->get_ast_scope();
    const auto cls_sym = analyse::utils::scope_utils::associated_type_symbol(*scope);

    // For compiler known types, specialize the llvm type symbols.
    const auto ancestor_names = scope->ancestors()
        | genex::views::drop_last(1)
        | genex::views::transform([](auto *x) { return x->name_as_string(); })
        | genex::to<std::vector>()
        | genex::views::reverse
        | genex::to<std::vector>();

    if (ancestor_names == std::vector<std::string>{"std", "void", "Void"}) {
        // Lower S++ "Void" to the llvm "void" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getVoidTy(*ctx->context);
        return;
    }

    if (ancestor_names == std::vector<std::string>{"std", "boolean", "Bool"}) {
        // Lower S++ "Bool" to the llvm "i1" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt1Ty(*ctx->context);
        return;
    }

    if (ancestor_names == std::vector<std::string>{"std", "string_view", "StrView"}) {
        // Lower S++ "StrView" to the llvm "i8*" type.
        cls_sym->llvm_info->llvm_type = llvm::PointerType::get(*ctx->context, 0);
        return;
    }

    if (ancestor_names[0] == "std" and ancestor_names[1] == "num" and ancestor_names[2].starts_with("sized") and ancestor_names[3].starts_with("Sized")) {
        const auto type_part = ancestor_names[2];
        const auto bit_width_ast = cls_sym->fq_name()->type_parts().back()->generic_arg_group->comp_at("w")->val->to<asts::IntegerLiteralAst>();
        if (bit_width_ast == nullptr) { return; }

        const auto bit_width = std::stoul(cls_sym->fq_name()->type_parts().back()->generic_arg_group->comp_at("w")->val->to<asts::IntegerLiteralAst>()->val->token_data);
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


    // If the type already exists in LLVM, skip.
    if (const auto llvm_type = llvm::StructType::getTypeByName(*ctx->context, mangle::mangle_type_name(*cls_sym)); llvm_type != nullptr) {
        cls_sym->llvm_info->llvm_type = llvm_type;
        return;
    }

    // Empty struct, will fill in stage_10 when all attributes' types have been generated.
    cls_sym->llvm_info->llvm_type = llvm::StructType::create(*ctx->context, mangle::mangle_type_name(*cls_sym));
}


auto spp::codegen::llvm_type(
    analyse::scopes::TypeSymbol const &type_sym,
    LlvmCtx const *ctx)
    -> llvm::Type* {
    // Either return the llvm type bound to the symbol, or a pointer for borrows.
    return type_sym.convention != nullptr
               ? llvm::PointerType::get(*ctx->context, 0)
               : type_sym.llvm_info->llvm_type;
}
