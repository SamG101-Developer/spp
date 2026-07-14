module;
#include <spp/macros.hpp>

module spp.asts.local_variable_single_identifier_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.type_ast;
import spp.codegen.llvm_type;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::LocalVariableSingleIdentifierAst::LocalVariableSingleIdentifierAst(
    decltype(TokMut) &&tok_mut,
    decltype(Name) &&name,
    decltype(Alias) &&alias) :
    TokMut(std::move(tok_mut)),
    Name(std::move(name)),
    Alias(std::move(alias)) {
}

spp::asts::LocalVariableSingleIdentifierAst::~LocalVariableSingleIdentifierAst() = default;

auto spp::asts::LocalVariableSingleIdentifierAst::PosStart() const
    -> std::size_t {
    // Use the "mut" token or name.
    return TokMut != nullptr ? TokMut->PosStart() : Name->PosStart();
}

auto spp::asts::LocalVariableSingleIdentifierAst::PosEnd() const
    -> std::size_t {
    // Use the alias or name.
    return Alias != nullptr ? Alias->PosEnd() : Name->PosEnd();
}

auto spp::asts::LocalVariableSingleIdentifierAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto l = MakeUnique<LocalVariableSingleIdentifierAst>(
        AstClone(TokMut), AstClone(Name), AstClone(Alias));
    l->Conv = AstClone(Conv);
    l->_FromCasePattern = _FromCasePattern;
    return l;
}

auto spp::asts::LocalVariableSingleIdentifierAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokMut).append(TokMut != nullptr ? " " : "");
    SPP_STRING_APPEND(Name).append(Alias != nullptr ? " " : "");
    SPP_STRING_APPEND(Alias);
    SPP_STRING_END;
}

auto spp::asts::LocalVariableSingleIdentifierAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Get the value and its type from the "meta" information.
    const auto val = meta->LetStatementFromUninitialized ? nullptr : meta->LetStatementValue;
    const auto val_type = meta->LetStatementValue != nullptr ? meta->LetStatementValue->InferType(sm, meta) : nullptr;

    // Create a variable symbol for this identifier and value.
    auto sym = MakeUnique<analyse::scopes::VariableSymbol>(
        (Alias != nullptr ? Alias->Name : Name).Get(),
        AstClone(meta->LetStatementExplicitType != nullptr ? meta->LetStatementExplicitType : val_type),
        sm->CurrentScope, TokMut != nullptr or (Conv != nullptr and *Conv == ConventionTag::MUT));

    // Update the type if there is a convention present.
    if (Conv != nullptr) {
        sym->Type = sym->Type->WithConvention(AstClone(Conv));
    }

    // Set the initialization AST (for errors).
    sym->MemInfo->AstInitialization = {Name.Get(), sm->CurrentScope};
    sym->MemInfo->AstInitializationOrigin = {Name.Get(), sm->CurrentScope};

    // Increment the initialization counter for initialized statements.
    if (val != nullptr) {
        sym->MemInfo->InitializationCounter = 1;

        // Set borrow asts based on the value's type's convention.
        if (const auto conv = val_type->GetConvention(); conv != nullptr) {
            sym->MemInfo->AstBorrowed = {val, sm->CurrentScope};
        }
    }

    else {
        // Mark an uninitialized variable as "moved"
        sym->MemInfo->AstMoved = {this, sm->CurrentScope};
    }

    // Add the symbol to the current scope.
    sm->CurrentScope->AddVarSymbol(std::move(sym));
}

auto spp::asts::LocalVariableSingleIdentifierAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // No value => nothing to check.
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    if (meta->LetStatementFromUninitialized) { return; }

    // Check the value's memory.
    meta->LetStatementValue->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*meta->LetStatementValue, *this, *sm, true, true, true, true, true, meta);

    // Get the name or alias symbol to mark it as initialized.
    const auto sym = sm->CurrentScope->GetVarSymbol(Alias != nullptr ? Alias->Name.Get() : Name.Get());
    sym->MemInfo->InitializedBy(*Name, sm->CurrentScope);
    if (Conv != nullptr) {
        sym->MemInfo->AstBorrowed = {Conv.Get(), sm->CurrentScope};
    }
}

auto spp::asts::LocalVariableSingleIdentifierAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Assign the generated value into the variable symbol.
    meta->Save();
    meta->AssignmentTarget = (Alias != nullptr ? Alias->Name : Name).Get();
    meta->LetStatementValue->Stage9_CompTimeResolve(sm, meta);

    const auto var_sym = sm->CurrentScope->GetVarSymbol(Alias != nullptr ? Alias->Name.Get() : Name.Get());
    var_sym->CompTimeValue = std::move(meta->CmpResult);
    meta->Restore();
}

auto spp::asts::LocalVariableSingleIdentifierAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create the alloca for the variable.
    const auto uid = spp::utils::Uid(this);
    const auto type_sym = sm->CurrentScope->GetTypeSymbol(meta->LetStatementExplicitType);
    const auto llvm_type = codegen::llvm_type(*type_sym, ctx);
    SPP_ASSERT(llvm_type != nullptr);

    // Always alloca at the top of the function for stack variables.
    const auto func = ctx->Builder.GetInsertBlock()->getParent();
    const auto entry = &func->getEntryBlock();
    auto temp_builder = llvm::IRBuilder(entry, entry->begin());

    const auto alloca = temp_builder.CreateAlloca(llvm_type, nullptr, "local.alloca" + uid);
    const auto var_sym = sm->CurrentScope->GetVarSymbol(Alias != nullptr ? Alias->Name.Get() : Name.Get());
    var_sym->LlvmInfo->Alloca = alloca;

    // Generate the initializer expression.
    if (not meta->LetStatementFromUninitialized) {
        meta->Save();
        meta->AssignmentTarget = (Alias != nullptr ? Alias->Name : Name).Get();
        const auto llvm_val = meta->LetStatementValue->Stage11_CodeGen(sm, meta, ctx);
        ctx->Builder.CreateStore(llvm_val, alloca);
        meta->Restore();
    }

    // Alloca already added; return nullptr.
    return nullptr;
}

auto spp::asts::LocalVariableSingleIdentifierAst::ExtractNames() const
    -> Vec<IdentifierAst*> {
    // Return the single name as a vector that can get appended to from nesting.
    return {Name.Get()};
}

auto spp::asts::LocalVariableSingleIdentifierAst::ExtractName() const
    -> IdentifierAst* {
    // Return the single name (identifier) for matching capability.
    return Name.Get();
}

SPP_MOD_END
