module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope_manager;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_member_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.utils.error_formatter;
import genex;

SPP_MOD_BEGIN
spp::analyse::scopes::ScopeManager::ScopeManager(
    Shared<Scope> const &global_scope,
    Scope *current_scope) :
    GlobalScope(global_scope),
    CurrentScope(current_scope ? current_scope : global_scope.get()) {
}

spp::analyse::scopes::ScopeManager::~ScopeManager() = default;

auto spp::analyse::scopes::ScopeManager::Iter() const
    -> ScopeRange {
    return ScopeRange{CurrentScope};
}

auto spp::analyse::scopes::ScopeManager::Reset(
    Scope *scope,
    std::optional<ScopeIterator> iterator)
    -> void {
    // Set the current scope to the provided scope or global scope.
    CurrentScope = scope ? scope : GlobalScope.get();
    _It = iterator.has_value() ? *iterator : ScopeIterator{CurrentScope};
}

auto spp::analyse::scopes::ScopeManager::CreateAndMoveIntoNewScope(
    ScopeName const &name,
    asts::Ast *ast,
    spp::utils::errors::ErrorFormatter *error_formatter)
    -> Scope* {
    // Create a new scope, using the current scope as the parent scope.
    auto scope = MakeUnique<Scope>(name, CurrentScope, ast, error_formatter);
    CurrentScope->Children.EmplaceBack(std::move(scope));
    ++_It;

    // Set the new scope as the current scope, and advance the iterator to match.
    CurrentScope = CurrentScope->Children.Back().get();
    return CurrentScope;
}

auto spp::analyse::scopes::ScopeManager::MoveOutOfCurrentScope()
    -> Scope* {
    // Exit the current scope into the parent scope.
    CurrentScope = CurrentScope->Parent;
    return CurrentScope;
}

auto spp::analyse::scopes::ScopeManager::MoveToNextScope(
    const bool ignore_alias_class_scopes)
    -> Scope* {
    // For debugging mode only, check if the iterator has reached the end of the generator.
    // Move to the next scope by advancing the iterator.
    CurrentScope = *++_It;
    while (ignore_alias_class_scopes and CurrentScope->TySym != nullptr and CurrentScope->TySym->AliasStmt != nullptr) {
        CurrentScope = *++_It;
    }
    return CurrentScope;
}

auto spp::analyse::scopes::ScopeManager::ExhaustScope()
    -> void {
    // Manual scope skipping.
    const auto final_scope = CurrentScope->FinalChildScope();
    while (CurrentScope != final_scope) {
        MoveToNextScope(false);
    }
}

auto spp::analyse::scopes::ScopeManager::AttachLlvmTypeInfo(
    asts::ModulePrototypeAst const &mod,
    codegen::LLvmCtx *ctx) const
    -> void {
    // Iterate the members of the module, filter to class prototypes, and call the register function.

    auto cls_members = Vec<asts::ClassPrototypeAst*>{};
    for (auto const &member : mod.Impl->Members) {
        if (const auto cls_member = member->To<asts::ClassPrototypeAst>(); cls_member != nullptr) {
            cls_members.EmplaceBack(cls_member);
        }

        if (const auto sup_member = member->To<asts::SupPrototypeFunctionsAst>(); sup_member != nullptr) {
            for (auto const &sup_member_inner : sup_member->Impl->Members) {
                if (const auto cls_sup_member = sup_member_inner->To<asts::ClassPrototypeAst>(); cls_sup_member != nullptr) {
                    cls_members.EmplaceBack(cls_sup_member);
                }
            }
        }

        if (const auto ext_member = member->To<asts::SupPrototypeExtensionAst>(); ext_member != nullptr) {
            for (auto const &ext_member_inner : ext_member->Impl->Members) {
                if (const auto cls_ext_member = ext_member_inner->To<asts::ClassPrototypeAst>(); cls_ext_member != nullptr) {
                    cls_members.EmplaceBack(cls_ext_member);
                }
            }
        }
    }

    for (auto const &cls_proto : cls_members) {
        // If this is not a base generic (Vec::Vec)
        if (cls_proto->GetRegisteredGenericSubstitutions().IsEmpty()) {
            codegen::RegisterLlvmTypeInfo(cls_proto, ctx);

            // All aliases need llvm type info propagated from their aliased types.
            const auto llvm_type = codegen::GetLlvmType(*cls_proto->GetAstScope()->TySym, ctx);
            for (auto const &alias_sym : cls_proto->GetAstScope()->TySym->AliasedBySyms) {
                alias_sym->LlvmInfo->LlvmType = llvm_type;
            }
        }

        // All concrete generic implementations (not Vec::Vec[T]).
        // Todo: don't generate when one of the generics is "comp->identifier" or "type->generic"
        for (auto const &generic_sub : cls_proto->GetRegisteredGenericSubstitutions()) {
            codegen::RegisterLlvmTypeInfo(generic_sub.Second, ctx);

            // All generic aliases need llvm type info propagated from their aliased types.
            const auto llvm_type = codegen::GetLlvmType(*generic_sub.Second->GetAstScope()->TySym, ctx);
            for (auto const &alias_sym : generic_sub.Second->GetAstScope()->TySym->AliasedBySyms) {
                alias_sym->LlvmInfo->LlvmType = llvm_type;
            }
        }
    }
}

auto spp::analyse::scopes::ScopeManager::AttachAllSuperScopes(
    asts::meta::CompilerMetaData *meta)
    -> void {
    // Ensure the scope manager is at the global scope. Attach every super scope structurally, deferring any generic
    // constraint check. This is order-independent because it does not depend on other types' super scopes already being
    // attached. The issue before was that some generic constraints checks were failing deep down because constraints
    // themselves are modelled as sup-scopes. Note for future: just trust this comment.
    Reset();
    auto deferred = Vec<DeferredSupConstraint>();
    for (auto *scope : Iter()) {
        if (scope->TySym == nullptr) { continue; }
        AttachSpecificSuperScopes(*scope, meta, &deferred);
    }

    // Now that every type has its super scopes, validate the deferred generic constraints and prune any attachment
    // whose constraint is unsatisfied (eg "SliceMut[StrView, USize]" retains the constrained
    // "sup [V, I: Zero] SliceMut[V, I]" block only if USize satisfies "Zero").
    Reset();
    PruneUnsatisfiedSupConstraints(deferred, meta);
    Reset();
}

auto spp::analyse::scopes::ScopeManager::AttachSpecificSuperScopes(
    Scope &scope,
    asts::meta::CompilerMetaData *meta,
    Vec<DeferredSupConstraint> *deferred) const
    -> void {
    // Handle type symbols.
    if (scope.TySym != nullptr) {
        const auto non_generic_sym = scope.GetTypeSymbol(scope.TySym->FqName()->WithoutGenerics());
        auto scopes = normal_sup_blocks[non_generic_sym.get()];
        scopes.AppendRange(generic_sup_blocks);
        AttachSpecificSuperScopesImpl(scope, std::move(scopes), meta, deferred);
    }
}

auto spp::analyse::scopes::ScopeManager::AttachSpecificSuperScopesImpl(
    Scope &scope,
    Vec<Scope*> &&sup_scopes,
    asts::meta::CompilerMetaData *meta,
    Vec<DeferredSupConstraint> *deferred) const
    -> void {
    //
    using utils::type_utils::CreateGenericSupScope;
    using utils::type_utils::RelaxedTypeEq;
    using utils::type_utils::GenericInferenceMap;

    // Skip "$" identifiers (functions don't have substitutable members and take up lots of time).
    const auto scope_name = scope.TySym->FqName();
    if (sup_scopes.IsEmpty()) { return; }
    if (scope_name->IsCompilerGeneratedType()) { return; }

    // Clear the sup scopes list.
    scope.DirectSupScopes.Clear();
    const auto fq_type = scope.TySym->FqName();
    auto const &cls_sym = scope.TySym;

    // Iterate through all the super scopes and check if the name matches.
    for (auto *sup_scope : sup_scopes) {
        // Perform a relaxed comparison between the two types (allows for specializations to match bases).
        auto scope_generics_map = GenericInferenceMap();

        // Load the generics.
        if (not RelaxedTypeEq(*fq_type, *asts::AstName(sup_scope->AstNode), *scope.TySym->ScopeDefinedIn, *sup_scope, scope_generics_map, false, false)) { continue; }
        auto scope_generics = asts::GenericArgumentGroupAst::FromMap(std::move(scope_generics_map));

        // Create a generic version of the super scope if needed.
        auto new_sup_scope = static_cast<Scope*>(nullptr);
        auto new_cls_scope = static_cast<Scope*>(nullptr);
        auto sup_sym = static_cast<TypeSymbol*>(nullptr);
        auto defer_constraint = false;

        // Todo: Is this "if-else" quite correct? 2 conditions in the "if", then no "else if" block.
        if (not scope_generics->Args.IsEmpty() and not genex::contains(generic_sup_blocks, sup_scope)) {
            const auto external_generics = scope.TySym->ScopeDefinedIn->GetExtendedGenericSymbols(scope_generics->GetAllArgs());
            std::tie(new_sup_scope, new_cls_scope) = CreateGenericSupScope(*sup_scope, scope, *scope_generics, external_generics, this, meta);
            sup_sym = new_cls_scope ? new_cls_scope->TySym.get() : nullptr;

            // When deferring (the bulk pass), match structurally only and record the constraint for later; the
            // constrained type's own super scopes might not be attached yet, so an inline check would be non order-
            // agnostic. On-demand attachment (deferred == nullptr) checks the constraint inline as before.
            auto _ = GenericInferenceMap();
            if (not RelaxedTypeEq(*fq_type, *asts::AstName(sup_scope->AstNode), *scope.TySym->ScopeDefinedIn, *new_sup_scope, _, false, deferred == nullptr)) { continue; }
            defer_constraint = deferred != nullptr;
        }
        else {
            const auto sup_proto = sup_scope->AstNode->To<asts::SupPrototypeExtensionAst>();
            new_sup_scope = sup_scope;
            new_cls_scope = sup_proto ? scope.GetTypeSymbol(sup_proto->SuperClass)->LinkedScope : nullptr;
            sup_sym = new_cls_scope ? new_cls_scope->TySym.get() : nullptr;
        }

        // Prevent double inheritance, cyclic inheritance and self extension.
        if (const auto ext_ast = sup_scope->AstNode->To<asts::SupPrototypeExtensionAst>(); ext_ast != nullptr) {
            ext_ast->CheckCyclicExtension(*sup_sym, *sup_scope);
            ext_ast->CheckDoubleExtension(*cls_sym, *sup_scope);
            ext_ast->CheckSelfExtension(*sup_scope);
        }

        // Register the super scope against the current scope.
        scope.DirectSupScopes.EmplaceBack(new_sup_scope);

        // Register the super scope's class scope against the current scope, if it is different. This "difference" check
        // ensures that "sup [T] T ext A" doesn't create a "sup A ext A" link.
        const auto cls_scope_attached = new_cls_scope and scope.TySym != new_cls_scope->TySym;
        if (cls_scope_attached) {
            // Todo: is this definitely the generically substituted "new_cls_scope"?
            scope.DirectSupScopes.EmplaceBack(new_cls_scope);
        }

        // Record the constrained attachment so its generic constraint can be validated (and the attachment pruned
        // if unsatisfied) once every type has its super scopes.
        if (defer_constraint) {
            deferred->EmplaceBack(DeferredSupConstraint{&scope, new_sup_scope, cls_scope_attached ? new_cls_scope : nullptr, sup_scope});
        }

        // Check for conflicting "cmp" or "type" statements in the super scopes.
        if (sup_scope->AstNode->To<asts::SupPrototypeExtensionAst>() or sup_scope->AstNode->To<asts::SupPrototypeFunctionsAst>()) {
            CheckConflictingTypeOrCmpStatements(*cls_sym, *sup_scope);
        }
    }
}

auto spp::analyse::scopes::ScopeManager::PruneUnsatisfiedSupConstraints(
    Vec<DeferredSupConstraint> &deferred,
    asts::meta::CompilerMetaData * /*meta*/) const
    -> void {
    // Todo: Genex usage
    using utils::type_utils::RelaxedTypeEq;
    using utils::type_utils::GenericInferenceMap;

    // Repeat until no further attachments are pruned: pruning one attachment can invalidate the constraint of
    // another that depends on it (transitive constraint chains), so a single pass is not sufficient.
    auto changed = true;
    while (changed) {
        changed = false;
        for (auto &dc : deferred) {
            // Skip records that have already been pruned.
            if (dc.owner_scope == nullptr) { continue; }

            // Re-run the constraint check that was deferred previously (RelaxedTypeEq with constraint checking
            // enabled), now against the complete super scope graph.
            auto _ = GenericInferenceMap();
            const auto fq_type = dc.owner_scope->TySym->FqName();
            if (RelaxedTypeEq(*fq_type, *asts::AstName(dc.base_sup_scope->AstNode), *dc.owner_scope->TySym->ScopeDefinedIn, *dc.sup_scope, _, false, true)) { continue; }

            // The constraint is not satisfied, so remove the attached super scope (and its paired class scope).
            auto &sup_scopes = dc.owner_scope->DirectSupScopes;
            sup_scopes.Erase(
                std::ranges::remove_if(sup_scopes, [&](auto const *s) { return s == dc.sup_scope or (dc.sup_cls_scope != nullptr and s == dc.sup_cls_scope); }).begin(),
                sup_scopes.end());
            dc.owner_scope = nullptr;
            changed = true;
        }
    }
}

auto spp::analyse::scopes::ScopeManager::CheckConflictingTypeOrCmpStatements(
    TypeSymbol const &cls_sym,
    Scope const &sup_scope)
    -> void {
    // Get the scopes to check for conflicts in.
    auto dummy = utils::type_utils::GenericInferenceMap();
    const auto existing_scopes = cls_sym.LinkedScope->DirectSupScopes
        | genex::views::filter([&](auto *scope) { return scope->AstNode->template To<asts::SupPrototypeExtensionAst>() or scope->AstNode->template To<asts::SupPrototypeFunctionsAst>(); })
        | genex::views::filter([&](auto *scope) { return utils::type_utils::RelaxedTypeEq(*asts::AstName(sup_scope.AstNode), *asts::AstName(scope->AstNode), sup_scope, *scope->AstNode->GetAstScope(), dummy); })
        | genex::to<Vec>();

    // Check for conflicting "type" statements.
    Vec<Shared<asts::TypeIdentifierAst>> new_types;
    for (auto const *scope : existing_scopes) {
        auto body = asts::AstBody(scope->AstNode);
        for (auto const *member : body) {
            if (auto const *type_stmt = member->To<asts::TypeStatementAst>(); type_stmt != nullptr) {
                for (auto const &new_type : new_types) {
                    RaiseIf<errors::SppIdentifierDuplicateError>(
                        *new_type == *type_stmt->NewType, {scope, &sup_scope},
                        ERR_ARGS(*new_type, *type_stmt->NewType, "associated type"));
                }
                new_types.EmplaceBack(type_stmt->NewType);
            }
        }
    }

    // Check for conflicting "cmp" statements.
    Vec<Shared<asts::IdentifierAst>> new_cmps;
    for (const auto *scope : existing_scopes) {
        auto body = asts::AstBody(scope->AstNode);
        for (auto const *member : body) {
            if (auto const *cmp_stmt = member->To<asts::CmpStatementAst>(); cmp_stmt != nullptr and not cmp_stmt->Type->IsCompilerGeneratedType()) {
                for (auto const &new_cmp : new_cmps) {
                    RaiseIf<errors::SppIdentifierDuplicateError>(
                        *new_cmp == *cmp_stmt->Name, {scope, &sup_scope},
                        ERR_ARGS(*new_cmp, *cmp_stmt->Name, "comptime constant"));
                }
                new_cmps.EmplaceBack(cmp_stmt->Name);
            }
        }
    }
}

auto spp::analyse::scopes::ScopeManager::CurrentIterator()
    -> ScopeIterator& {
    return _It;
}

auto spp::analyse::scopes::ScopeManager::SelfProto() const
    -> asts::ClassPrototypeAst* {
    return _SelfProto.get();
}

auto spp::analyse::scopes::ScopeManager::Cleanup() -> void {
    normal_sup_blocks.clear();
    generic_sup_blocks.Clear();
    temp_scopes.Clear();
}

SPP_MOD_END
