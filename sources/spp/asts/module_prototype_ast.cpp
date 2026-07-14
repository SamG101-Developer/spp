module;
#include <spp/macros.hpp>

module spp.asts.module_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;

SPP_MOD_BEGIN
spp::asts::ModulePrototypeAst::ModulePrototypeAst(
    decltype(Impl) &&impl) :
    Impl(std::move(impl)) {
}

spp::asts::ModulePrototypeAst::~ModulePrototypeAst() = default;

auto spp::asts::ModulePrototypeAst::PosStart() const
    -> std::size_t {
    // Use the impl.
    return Impl->PosStart();
}

auto spp::asts::ModulePrototypeAst::PosEnd() const
    -> std::size_t {
    // Use the impl.
    return Impl->PosEnd();
}

auto spp::asts::ModulePrototypeAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ModulePrototypeAst>(AstClone(Impl));
}

auto spp::asts::ModulePrototypeAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Impl);
    SPP_STRING_END;
}

auto spp::asts::ModulePrototypeAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Shift to implementation.
    Ast::Stage1_PreProcess(ctx);
    Impl->Stage1_PreProcess(this);
}

auto spp::asts::ModulePrototypeAst::Stage2_GenTopLvlScopes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    Impl->Stage2_GenTopLvlScopes(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage3_GenTopLvlAliases(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    Impl->Stage3_GenTopLvlAliases(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    Impl->Stage4_QualifyTypes(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage5_LoadSupScopes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    Impl->Stage5_LoadSupScopes(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage6_PreAnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    Impl->Stage6_PreAnalyseSemantics(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    Impl->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    Impl->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    Impl->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage10_PreCodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Shift to implementation.
    return Impl->Stage10_PreCodeGen(sm, meta, ctx);
}

auto spp::asts::ModulePrototypeAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Add the entry building block for module level code.

    // Shift to implementation.
    return Impl->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::ModulePrototypeAst::Name() const
    -> Unique<IdentifierAst> {
    using namespace std::literals;
    // Split the file path intro parts.
    auto parts = Vec<Str>();
    for (auto const &entry : std::filesystem::directory_iterator(FilePath)) {
        if (entry.is_directory()) {
            parts.EmplaceBack(entry.path().filename().display_string());
        }
    }

    // Check if "src" is in the file path.
    auto name = Str();
    if (std::ranges::contains(parts, "src"s)) {
        name = parts
            | std::views::drop(std::ranges::find_if(parts, [](auto const &p) { return p == "src"s; }) - parts.begin())
            | std::views::join_with("::"s)
            | std::ranges::to<Str>();
    }
    else {
        name = Vec{parts[0], parts[1] + ".spp"_str}
            | std::views::join_with("::"s)
            | std::ranges::to<Str>();
    }

    return MakeUnique<IdentifierAst>(PosStart(), std::move(name));
}

auto spp::asts::ModulePrototypeAst::FileName() const
    -> Unique<IdentifierAst> {
    // Return the filepath as an IdentifierAst.
    return MakeUnique<IdentifierAst>(PosStart(), FilePath.display_string());
}

SPP_MOD_END
