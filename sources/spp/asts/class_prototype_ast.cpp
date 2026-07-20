module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.class_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import genex;
import llvm;

SPP_MOD_BEGIN
spp::asts::ClassPrototypeAst::ClassPrototypeAst(
    decltype(Annotations) &&annotations,
    decltype(TokCls) &&tok_cls,
    decltype(Name) name,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(Impl) &&impl) :
    Annotations(std::move(annotations)),
    TokCls(std::move(tok_cls)),
    Name(std::move(name)),
    GnParamGroup(std::move(generic_param_group)),
    Impl(std::move(impl)),
    _ClsSym(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCls, lex::SppTokenType::KW_CLS, "cls");
    SPP_SET_AST_TO_DEFAULT_SHARED_IF_NULLPTR(this->GnParamGroup);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Impl);
}

spp::asts::ClassPrototypeAst::~ClassPrototypeAst() = default;

auto spp::asts::ClassPrototypeAst::PosStart() const
    -> std::size_t {
    // Use the "cls" token.
    return TokCls->PosStart();
}

auto spp::asts::ClassPrototypeAst::PosEnd() const
    -> std::size_t {
    // Use the name.
    return Name->PosEnd();
}

auto spp::asts::ClassPrototypeAst::Clone() const
    -> Unique<Ast> {
    auto ast = MakeUnique<ClassPrototypeAst>(
        AstCloneVec(Annotations), AstClone(TokCls), AstClone(Name), AstClone(GnParamGroup), AstClone(Impl));
    ast->Visibility = Visibility;
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    ast->_ClsSym = _ClsSym;
    for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
    return ast;
}

auto spp::asts::ClassPrototypeAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_EXTEND(Annotations, "\n").append(not Annotations.IsEmpty() ? "\n" : "");
    SPP_STRING_APPEND(TokCls).append(" ");
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(GnParamGroup).append(" ");
    SPP_STRING_APPEND(Impl);
    SPP_STRING_END;
}

auto spp::asts::ClassPrototypeAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing annotations and the body.
    Ast::Stage1_PreProcess(ctx);
    for (auto const &a : Annotations) { a->Stage1_PreProcess(this); }
    Impl->Stage1_PreProcess(this);
}

auto spp::asts::ClassPrototypeAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create the class scope, which is the scope for the class prototype.
    auto scope_name = analyse::scopes::ScopeTypeIdentifierName(Name);
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    Ast::Stage2_GenTopLvlScopes(sm, meta);

    // Run the generation steps for the annotations.
    for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

    // Generate the symbols for the class prototype, and handle generic parameters.
    meta->ClsSym = _GenerateSymbols(sm);
    GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
    Impl->Stage2_GenTopLvlScopes(sm, meta);

    // Move out of the class scope, as the class scope is now complete.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage3_GenTopLvlAliases(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Skip the class scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the types in the class body.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    for (auto const &a : Annotations) { a->Stage4_QualifyTypes(sm, meta); }
    GnParamGroup->Stage4_QualifyTypes(sm, meta);
    Impl->Stage4_QualifyTypes(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load the super scopes for the class body.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }

    // Add the "Self" symbol into the scope.
    if (not Name->IsCompilerGeneratedType()) {
        const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name);
        const auto self_sym = MakeShared<analyse::scopes::TypeSymbol>(
            MakeUnique<TypeIdentifierAst>(Name->PosStart(), "Self", nullptr),
            sm->SelfProto(), cls_sym->LinkedScope, sm->CurrentScope);
        sm->CurrentScope->AddTypeSymbol(self_sym);
    }

    Impl->Stage5_LoadSupScopes(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage6_PreAnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for the class body.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage6_PreAnalyseSemantics(sm, meta);

    // Check the type isn't recursive.
    const auto recursion = analyse::utils::type_utils::IsTypeRecursive(*this, *sm);
    RaiseIf<analyse::errors::SppRecursiveTypeError>(
        recursion != nullptr, {sm->CurrentScope},
        ERR_ARGS(*this, *recursion));

    sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse semantics for the class body.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Re-map "Self" to the true type.
    if (not Name->IsCompilerGeneratedType()) {
        const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name);
        const auto self_sym = sm->CurrentScope->GetTypeSymbol(MakeUnique<TypeIdentifierAst>(Name->PosStart(), "Self", nullptr), true);
        self_sym->Type = cls_sym->Type;
        cls_sym->AliasedBySyms.EmplaceBack(self_sym);
    }

    for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }
    GnParamGroup->Stage7_AnalyseSemantics(sm, meta);
    Impl->Stage7_AnalyseSemantics(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check memory for the class body.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage8_CheckMemory(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Skip the class body.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }
    Impl->Stage9_CompTimeResolve(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::ClassPrototypeAst::Stage10_PreCodeGen(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate code for the class body.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    const auto cls_sym = sm->CurrentScope->TySym;

    // $ types are pre-set with a packed empty body.
    if (Name->IsCompilerGeneratedType()) {
        sm->MoveOutOfCurrentScope();
        return nullptr;
    }

    // If this is a raw generic class like Vec[T], then generate the generic implementations.
    if (genex::any_of(sm->CurrentScope->AllTypeSymbols(), [](auto const &sym) { return sym->IsGeneric; })) {
        for (auto const &[generic_scope, generic_ast] : _GenericSubstitutions) {
            generic_ast->_FillLlvmLayout(sm, generic_scope->TySym.get(), ctx);
        }
    }

    _FillLlvmLayout(sm, cls_sym.get(), ctx);

    sm->MoveOutOfCurrentScope();
    return nullptr;
}

auto spp::asts::ClassPrototypeAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the class symbol.
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage11_CodeGen(sm, meta, ctx);
    sm->MoveOutOfCurrentScope();
    return nullptr;
}

auto spp::asts::ClassPrototypeAst::RegisterGenericSubstitution(
    analyse::scopes::Scope *scope,
    Unique<ClassPrototypeAst> &&new_ast)
    -> void {
    // Just somewhere to store the new_ast as a unique_ptr.
    _GenericSubstitutions.EmplaceBack(scope, std::move(new_ast));
}

auto spp::asts::ClassPrototypeAst::GetRegisteredGenericSubstitutions() const
    -> Vec<Pair<analyse::scopes::Scope*, ClassPrototypeAst*>> {
    // Return the generic substituted scopes as raw pointers.
    return _GenericSubstitutions
        | genex::views::transform([](auto const &x) { return MakePair(x.First, x.Second.get()); })
        | genex::to<Vec>();
}

auto spp::asts::ClassPrototypeAst::GetClsSym() const
    -> Shared<analyse::scopes::TypeSymbol> {
    return _ClsSym;
}

auto spp::asts::ClassPrototypeAst::_GenerateSymbols(
    ScopeManager *sm)
    -> analyse::scopes::TypeSymbol* {
    auto is_dollar_type = Name->IsCompilerGeneratedType();
    auto sym_name = AstClone(Name->TypeParts()[0]);
    sym_name->GnArgGroup = GenericArgumentGroupAst::FromParams(*GnParamGroup);

    // Create the symbols as TypeSymbol pointers, so AliasSymbols can also be used.
    Shared<analyse::scopes::TypeSymbol> symbol_1 = nullptr;
    Shared<analyse::scopes::TypeSymbol> symbol_2 = nullptr;

    // Create the symbol for the type, include generics if applicable, like Vec[T].
    symbol_1 = MakeShared<analyse::scopes::TypeSymbol>(
        std::move(sym_name), this, sm->CurrentScope, sm->CurrentScope, sm->CurrentScope->ParentModule(), false,
        is_dollar_type);
    sm->CurrentScope->TySym = symbol_1;
    sm->CurrentScope->Parent->AddTypeSymbolCheckConflict(symbol_1);
    _ClsSym = sm->CurrentScope->TySym;

    // If the type was generic, like Vec[T], also create a base Vec symbol.
    if (not GnParamGroup->Params.IsEmpty()) {
        symbol_2 = MakeShared<analyse::scopes::TypeSymbol>(
            AstClone(Name->TypeParts()[0]), this, sm->CurrentScope, sm->CurrentScope,
            sm->CurrentScope->ParentModule(), false, is_dollar_type);
        symbol_2->GenericImpl = symbol_1.get();
        sm->CurrentScope->TySym = symbol_2;
        const auto ret_sym = symbol_2.get();
        sm->CurrentScope->Parent->AddTypeSymbolCheckConflict(symbol_2);
        return ret_sym;
    }

    return _ClsSym.get();
}

static auto ApplyStructLayout(
    llvm::StructType *struct_type,
    spp::Vec<llvm::Type*> const &field_types,
    const spp::codegen::StructLayout layout,
    spp::codegen::LlvmTypeSymInfo *sym_info,
    spp::codegen::LLvmCtx const *ctx)
    -> void {
    switch (layout) {
        case spp::codegen::StructLayout::C: {
            // Keep declaration order, with natural alignment padding.
            struct_type->setBody(field_types.ToStdVector(), false);
            sym_info->FieldIndexMap.clear();
            break;
        }
        case spp::codegen::StructLayout::Packed: {
            // Keep declaration order, but remove all inter-field padding.
            struct_type->setBody(field_types.ToStdVector(), true);
            sym_info->FieldIndexMap.clear();
            break;
        }
        case spp::codegen::StructLayout::Spp: {
            // Re-order the fields to minimize padding, and record where each declared attribute ended up, so that
            // codegen can map a declaration index to its physical field index.
            auto [sorted_types, index_map] = spp::codegen::SortMembersForSppLayout(field_types, ctx);
            struct_type->setBody(sorted_types.ToStdVector(), false);
            sym_info->FieldIndexMap = std::move(index_map);
            break;
        }
        default: {
            std::unreachable();
        }
    }
}

auto spp::asts::ClassPrototypeAst::_FillLlvmLayout(
    ScopeManager const *sm,
    analyse::scopes::TypeSymbol const *type_sym,
    codegen::LLvmCtx *ctx) const
    -> void {
    // Todo: error if attribute's default value if a comp generic value?? Also TEST THIS

    // Non-struct types are compiler known special types, so don't have any field generation.
    const auto lt = codegen::GetLlvmType(*type_sym, ctx);
    if (lt == nullptr or not llvm::isa<llvm::StructType>(lt)) {
        return;
    }

    auto types = analyse::utils::type_utils::GetAllAttrs(*type_sym->FqName(), sm)
        | genex::views::transform([&](auto const &pair) { return codegen::GetLlvmType(*std::get<1>(pair), ctx); })
        | genex::to<Vec>();

    // If there are any generic types present (llvm_type is nullptr), skip the layout generation.
    if (genex::all_of(types, [](auto const &x) { return x != nullptr; })) {
        const auto struct_type = llvm::dyn_cast<llvm::StructType>(lt);
        ApplyStructLayout(struct_type, types, codegen::StructLayout::Spp, type_sym->LlvmInfo.get(), ctx);
    }

    // Pass this layout to aliases too (the field re-ordering as well as the type itself).
    for (auto const &alias : type_sym->AliasedBySyms) {
        alias->LlvmInfo->LlvmType = lt;
        alias->LlvmInfo->FieldIndexMap = type_sym->LlvmInfo->FieldIndexMap;
    }
}

SPP_MOD_END
