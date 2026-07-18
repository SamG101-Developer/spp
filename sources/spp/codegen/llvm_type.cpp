module spp.codegen.llvm_type;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.analyse.scopes.symbols;
import spp.asts.boolean_literal_ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_mangle;
import spp.lex.tokens;
import spp.utils.types;
import genex;
import llvm;
import std;


auto spp::codegen::RegisterLlvmTypeInfo(
    asts::ClassPrototypeAst const *cls_proto,
    LLvmCtx const *ctx)
    -> void {
    // Note: because symbols have a convention attached to them, retrieval handles pointer logic for borrows.

    // $ types are 0-size types in LLVM.
    if (cls_proto->Name->IsCompilerGeneratedType()) {
        const auto zero_size_struct = llvm::StructType::create(*ctx->Context, mangle::mangle_type_name(*cls_proto->GetClsSym()));
        zero_size_struct->setBody({}, true);
        cls_proto->GetClsSym()->LlvmInfo->LlvmType = zero_size_struct;
    }

    // Get the class symbol from the current scope.
    const auto scope = cls_proto->GetAstScope();
    const auto cls_sym = scope->TySym;

    // For compiler known types, specialize the llvm type symbols.
    const auto ancestor_names = scope->Ancestors()
        | genex::views::drop_last(1)
        | genex::views::transform([](auto *x) { return x->NameAsString(); })
        | genex::to<Vec>()
        | genex::views::reverse
        | genex::to<Vec>();

    if (ancestor_names == Vec<Str>{"std", "void", "Void"}) {
        // Lower S++ "Void" to the llvm "void" type.
        cls_sym->LlvmInfo->LlvmType = llvm::Type::getVoidTy(*ctx->Context);
        return;
    }

    if (ancestor_names == Vec<Str>{"std", "boolean", "Bool"}) {
        // Lower S++ "Bool" to the llvm "i1" type.
        cls_sym->LlvmInfo->LlvmType = llvm::Type::getInt1Ty(*ctx->Context);
        return;
    }

    if (ancestor_names == Vec<Str>{"std", "string_view", "StrView"}) {
        // Lower S++ "StrView" to the llvm "i8*" type.
        cls_sym->LlvmInfo->LlvmType = llvm::PointerType::get(*ctx->Context, 0);
        return;
    }

    if (ancestor_names[0] == "std" and ancestor_names[1] == "num" and ancestor_names[2].starts_with("sized") and ancestor_names[3].starts_with("Sized")) {
        const auto type_part = ancestor_names[2];
        const auto bit_width_ast = scope->TySym->FqName()->TypeParts().Back()->GnArgGroup->CompAt("w")->Val->To<asts::IntegerLiteralAst>();
        if (bit_width_ast == nullptr) { return; }

        const auto bit_width = std::stoul(scope->TySym->FqName()->TypeParts().Back()->GnArgGroup->CompAt("w")->Val->To<asts::IntegerLiteralAst>()->Val->TokenData);
        if (type_part == "sized_integer") {
            cls_sym->LlvmInfo->LlvmType = llvm::Type::getIntNTy(*ctx->Context, static_cast<unsigned int>(bit_width));
            return;
        }

        if (type_part.starts_with("sized_floating_point")) {
            if (bit_width == 8) {
                cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, llvm::APFloatBase::Float8E4M3());
            }
            else if (bit_width == 16) {
                cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, llvm::APFloatBase::IEEEhalf());
            }
            else if (bit_width == 32) {
                cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, llvm::APFloatBase::IEEEsingle());
            }
            else if (bit_width == 64) {
                cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, llvm::APFloatBase::IEEEdouble());
            }
            else if (bit_width == 128) {
                cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, llvm::APFloatBase::IEEEquad());
            }
            return;
        }
    }


    // If the type already exists in LLVM, skip.
    if (const auto llvm_type = llvm::StructType::getTypeByName(*ctx->Context, mangle::mangle_type_name(*cls_sym)); llvm_type != nullptr) {
        cls_sym->LlvmInfo->LlvmType = llvm_type;
        return;
    }

    // Empty struct, will fill in stage_10 when all attributes' types have been generated.
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::create(*ctx->Context, mangle::mangle_type_name(*cls_sym));
}

auto spp::codegen::GetLlvmType(
    analyse::scopes::TypeSymbol const &type_sym,
    LLvmCtx const *ctx)
    -> llvm::Type* {
    // Either return the llvm type bound to the symbol, or a pointer for borrows.
    return type_sym.Convention != nullptr ? llvm::PointerType::get(*ctx->Context, 0) : type_sym.LlvmInfo->LlvmType;
}
