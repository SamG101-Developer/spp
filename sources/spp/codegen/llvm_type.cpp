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
import spp.asts.generic_argument_type_ast;
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

static auto GetFloatIntrinsic(const std::size_t bit_width) -> llvm::fltSemantics const& {
    switch (bit_width) {
        case 8: { return llvm::APFloatBase::Float8E4M3(); }
        case 16: { return llvm::APFloatBase::IEEEhalf(); }
        case 32: { return llvm::APFloatBase::IEEEsingle(); }
        case 64: { return llvm::APFloatBase::IEEEdouble(); }
        case 128: { return llvm::APFloatBase::IEEEquad(); }
        default: std::unreachable();
    }
    std::unreachable();
}

auto spp::codegen::RegisterLlvmTypeInfo(
    asts::ClassPrototypeAst const *cls_proto,
    LLvmCtx const *ctx)
    -> void {
    // Note: because symbols have a convention attached to them, retrieval handles pointer logic for borrows.

    // $ types are function "mock" types (a $-type generated per function that superimposes n FunXXXs over itself). A
    // function used as a value is one of these mocks, so it lowers to the same { fn_ptr, env_ptr } pair as the function
    // type it extends, making it interchangeable with closures.
    if (cls_proto->Name->IsCompilerGeneratedType()) {
        const auto mock_sym = cls_proto->GetClsSym();
        const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
        mock_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
        return;
    }

    // Get the class symbol from the current scope.
    const auto scope = cls_proto->GetAstScope();
    const auto cls_sym = scope->TySym;

    // For compiler known types, specialize the llvm type symbols.
    const auto ancestor_names = scope->Ancestors()
        | genex::views::drop_last(1)
        | genex::views::transform([](auto *x) { return x->NameAsString(); })
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
        const auto bit_width = std::stoul(bit_width_ast->Val->TokenData);

        if (type_part == "sized_integer") {
            cls_sym->LlvmInfo->LlvmType = llvm::Type::getIntNTy(*ctx->Context, static_cast<unsigned int>(bit_width));
            return;
        }

        if (type_part.starts_with("sized_floating_point")) {
            cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, GetFloatIntrinsic(bit_width));
            return;
        }
    }

    if (ancestor_names == Vec<Str>{"std", "array", "Arr"}) {
        // Lower S++ Arr" to the llvm "[T * n]" type.
        const auto gn_arg_group = cls_sym->FqName()->TypeParts().Back()->GnArgGroup.get();
        const auto length_ast = gn_arg_group->CompAt("n")->Val->To<asts::IntegerLiteralAst>();
        const auto elem_sym = scope->GetTypeSymbol(gn_arg_group->TypeAt("T")->Val);
        if (length_ast != nullptr and elem_sym != nullptr) {
            if (elem_sym->LlvmInfo->LlvmType == nullptr and elem_sym->Type != nullptr) {
                RegisterLlvmTypeInfo(elem_sym->Type, ctx);
            }
            if (const auto elem_llvm_type = GetLlvmType(*elem_sym, ctx); elem_llvm_type != nullptr) {
                cls_sym->LlvmInfo->LlvmType = llvm::ArrayType::get(elem_llvm_type, std::stoull(length_ast->Val->TokenData));
            }
        }
        return;
    }

    if (ancestor_names == Vec<Str>{"std", "function", "FunMov"}
        or ancestor_names == Vec<Str>{"std", "function", "FunMut"}
        or ancestor_names == Vec<Str>{"std", "function", "FunRef"}) {
        // Lower the function types to a { fn_ptr, env_ptr } pair (a "fat pointer": the function code plus a
        // pointer to its captured environment; the env pointer is null for capture-less functions). All three
        // share this layout, so plain functions and closures are interchangeable. A structurally-uniqued
        // literal struct is used so every function-type instantiation maps to the same llvm type.
        const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
        cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
        return;
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
