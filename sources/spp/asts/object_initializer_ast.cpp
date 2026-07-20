module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.utils.uid;
import llvm;
import genex;

SPP_MOD_BEGIN
spp::asts::ObjectInitializerAst::ObjectInitializerAst(
    decltype(Type) type,
    decltype(ArgGroup) &&arg_group) :
    Type(std::move(type)),
    ArgGroup(std::move(arg_group)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->ArgGroup);
    Source.OriginalType = AstClone(Type);
}

spp::asts::ObjectInitializerAst::~ObjectInitializerAst() = default;

auto spp::asts::ObjectInitializerAst::PosStart() const
    -> std::size_t {
    // Use the type.
    return Source.OriginalType->PosStart();
}

auto spp::asts::ObjectInitializerAst::PosEnd() const
    -> std::size_t {
    // Use the argument group.
    return ArgGroup->PosEnd();
}

auto spp::asts::ObjectInitializerAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ObjectInitializerAst>(
        AstClone(Type), AstClone(ArgGroup));
}

auto spp::asts::ObjectInitializerAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Type);
    SPP_STRING_APPEND(ArgGroup);
    SPP_STRING_END;
}

auto spp::asts::ObjectInitializerAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::errors::SppObjectInitializerVariantError;
    using analyse::utils::type_utils::IsTypeBorrowed;
    using analyse::utils::type_utils::IsTypeVariant;

    // Get the base class symbol (no generics) and check it exists.
    meta->Save();
    meta->SkipTypeAnalysisGenericChecks = true;
    Type->WithoutGenerics()->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();

    // Check this type isn't a borrow violation.
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*Type, *sm),
        {sm->CurrentScope}, ERR_ARGS(*this, *Source.OriginalType, "object initializer"));
    const auto base_cls_sym = sm->CurrentScope->GetTypeSymbol(Type->WithoutGenerics());

    // If the type is a variant type, prevent instantiation.
    RaiseIf<SppObjectInitializerVariantError>(
        IsTypeVariant(*base_cls_sym->FqName(), *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*Source.OriginalType));

    // Prepare the object initializer arguments.
    meta->Save();
    meta->ObjectInitType = Type->WithoutGenerics();
    ArgGroup->Stage6_PreAnalyseSemantics(sm, meta);
    meta->Restore();

    // Determine the generic inference source and target values.
    auto generic_infer_source = ArgGroup->Args
        | genex::views::transform([sm, meta](auto const &x) { return MakePair(x->Name, x->Val->InferType(sm, meta)); })
        | genex::to<Vec>();

    auto generic_infer_target = not base_cls_sym->IsGeneric
        ? base_cls_sym->Type->Impl->Members
        | genex::views::ptr
        | genex::views::cast_dynamic<ClassAttributeAst*>()
        | genex::views::transform([&](auto const &x) { return MakePair(x->Name, base_cls_sym->LinkedScope->GetTypeSymbol(x->Type)->FqName()); })
        | genex::to<Vec>()
        : spp::Vec<Pair<std::shared_ptr<IdentifierAst>, std::shared_ptr<TypeAst>>>();

    meta->Save();
    meta->InferSource = {generic_infer_source.begin(), generic_infer_source.end()};
    meta->InferTarget = {generic_infer_target.begin(), generic_infer_target.end()};
    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type)->FqName();
    meta->Restore();

    meta->Save();
    meta->ObjectInitType = Type;
    ArgGroup->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();
}

auto spp::asts::ObjectInitializerAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the object argument group.
    ArgGroup->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::ObjectInitializerAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Convert the inner elements to compile-time values.
    auto cmp_elems = ObjectInitializerArgumentGroupAst::NewEmpty();
    for (auto const &elem : ArgGroup->Args) {
        elem->Stage9_CompTimeResolve(sm, meta);
        auto cmp_arg = MakeUnique<ObjectInitializerArgumentKeywordAst>(elem->Name, nullptr, std::move(meta->CmpResult));
        cmp_elems->Args.EmplaceBack(std::move(cmp_arg));
    }

    // Wrap the compile-time array value.
    meta->CmpResult = MakeUnique<ObjectInitializerAst>(
        Type, std::move(cmp_elems));
}

auto spp::asts::ObjectInitializerAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::utils::type_utils::GetAllAttrs;

    // Create an empty struct based on the llvm type - will never be a borrow so always stack allocated, not a pointer.
    const auto uid = spp::utils::Uid(this);
    const auto type_sym = sm->CurrentScope->GetTypeSymbol(Type);
    const auto llvm_type = codegen::GetLlvmType(*type_sym, ctx);
    SPP_ASSERT(llvm_type != nullptr);

    const auto attr_names = GetAllAttrs(*type_sym->FqName(), sm)
        | genex::views::tuple_nth<0>
        | genex::to<Vec>();

    // Types with no attributes have nothing to fill in, so they initialize to their zero value. This covers the
    // compiler-known primitives (which don't even lower to structs) and the fat pointer types. Also the base cases for
    // the recursive default initialization.
    if (attr_names.IsEmpty()) { return llvm::Constant::getNullValue(llvm_type); }

    // The physical field order isn't the declaration order, because the S++ layout re-orders the fields to minimize
    // padding, so every attribute's index has to be resolved through the type's field index map.
    const auto field_index = [&](IdentifierAst const &name) {
        const auto decl_index = genex::position(attr_names, [&name](auto const &attr_name) { return *attr_name == name; });
        SPP_ASSERT(decl_index >= 0);
        return codegen::GetPhysicalFieldIndex(*type_sym->LlvmInfo, static_cast<std::size_t>(decl_index));
    };

    // Runtime pathway.
    if (not ctx->InConstantContext) {
        // Set each field value in the aggregate.
        const auto aggregate = codegen::llvm_entry_alloca(llvm_type, "obj_init.aggregate" + uid, ctx);
        for (auto const &arg : ArgGroup->Args) {
            const auto attr_ptr = ctx->Builder.CreateStructGEP(llvm_type, aggregate, field_index(*arg->Name), arg->Name->Val);
            const auto val = arg->Val->Stage11_CodeGen(sm, meta, ctx);

            SPP_ASSERT(val != nullptr and attr_ptr != nullptr);
            ctx->Builder.CreateStore(val, attr_ptr);
        }

        // Return the aggregate.
        SPP_ASSERT(aggregate != nullptr);
        return ctx->Builder.CreateLoad(llvm_type, aggregate, "obj_init.result" + uid);
    }

    // Constant pathway.
    // Set each field value in the constant, indexed by its physical position in the struct.
    const auto struct_type = llvm::cast<llvm::StructType>(llvm_type);
    auto comp_fields = Vec<llvm::Constant*>(struct_type->getNumElements());
    for (auto const &arg : ArgGroup->Args) {
        const auto comp_val = arg->Val->Stage11_CodeGen(sm, meta, ctx);
        comp_fields[field_index(*arg->Name)] = llvm::cast<llvm::Constant>(comp_val);
    }

    // Return the constant struct.
    return llvm::ConstantStruct::get(struct_type, comp_fields.ToStdVector());
}

auto spp::asts::ObjectInitializerAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // The type of the object initializer is the type being initialized. The conventions are added for dummy types being
    // created into values during other ast's analysis. Types cannot be instantiated as borrows in user code.
    return sm->CurrentScope->GetTypeSymbol(Type)->FqName()->WithConvention(AstClone(Type->GetConvention()));
}

auto spp::asts::ObjectInitializerAst::InferTypeForDisplay(
    ScopeManager *,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // Use the source original type.
    return Source.OriginalType;
}

SPP_MOD_END
