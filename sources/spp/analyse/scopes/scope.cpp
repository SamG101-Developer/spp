module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.symbol_table;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.compiler.module_tree;
import spp.utils.error_formatter;
import spp.utils.algorithms;
import spp.utils.functions;
import spp.utils.ptr;
import ankerl.unordered_dense;

static spp::Unique<spp::asts::IdentifierAst> MASTER_GLOB_NS_SYM_NAME = nullptr;
static spp::Unique<spp::analyse::scopes::NamespaceSymbol> MASTER_GLOB_NS_SYM = nullptr;
static spp::Unique<spp::analyse::scopes::Scope> MASTER_GLOB_NS_SCOPE = nullptr;

SPP_MOD_BEGIN
spp::analyse::scopes::Scope::Scope(
    ScopeName name,
    Scope *parent,
    asts::Ast *ast,
    spp::utils::errors::ErrorFormatter *error_formatter) :
    Name(std::move(name)),
    Parent(parent),
    AstNode(ast),
    TySym(nullptr),
    NsSym(nullptr),
    NonGenericScope(this),
    _ErrorFormatter(error_formatter) {
}

spp::analyse::scopes::Scope::Scope(Scope const &other) :
    Name(std::visit(spp::utils::functions::Overload{
                        [](ScopeIdentifierName const &name) -> ScopeName { return ScopeIdentifierName(asts::AstClone(name.Name)); },
                        [](auto const &name) -> ScopeName { return name; }
                    }, other.Name)),
    Parent(other.Parent),
    AstNode(other.AstNode),
    TySym(other.TySym),
    NsSym(other.NsSym),
    NonGenericScope(other.NonGenericScope),
    InternalTable(other.InternalTable),
    _ErrorFormatter(nullptr) {
    // Copy the children recursively.
    for (auto const &child_scope : other.Children) {
        auto child_copy = MakeUnique<Scope>(*child_scope);
        child_copy->Parent = this;
        Children.EmplaceBack(std::move(child_copy));
    }
}

spp::analyse::scopes::Scope::~Scope() = default;

auto spp::analyse::scopes::Scope::NewGlobal(
    compiler::Module const &mod)
    -> Scope* {
    // Create a new global scope (no parent or ast for the global scope).
    auto scope_name = ScopeBlockName::FromParts(
        "__global__", {}, 0);
    auto glob_scope = MakeUnique<Scope>(
        std::move(scope_name), nullptr, nullptr, mod.error_formatter.Get());

    // Inject the "_global" namespace symbol into this scope (makes lookups orthogonal).
    MASTER_GLOB_NS_SYM_NAME = MakeUnique<asts::IdentifierAst>(0uz, "_global");
    MASTER_GLOB_NS_SYM = MakeUnique<NamespaceSymbol>(
        MASTER_GLOB_NS_SYM_NAME.Get(), glob_scope.Get());
    glob_scope->NsSym = MASTER_GLOB_NS_SYM.Get();

    // Return the global scope.
    MASTER_GLOB_NS_SCOPE = std::move(glob_scope);
    return MASTER_GLOB_NS_SCOPE.Get();
}

auto spp::analyse::scopes::Scope::SearchSupScopesForVar(
    Scope const &scope,
    asts::IdentifierAst const *name)
    -> VariableSymbol* {
    // Recursively search the super scopes for a variable symbol.
    for (auto const *sup_scope : scope.DirectSupScopes) {
        if (const auto sym = sup_scope->GetVarSymbol(name, true); sym != nullptr) { return sym; }
    }

    // No symbol was found, so return nullptr.
    return nullptr;
}

auto spp::analyse::scopes::Scope::SearchSupScopesForType(
    Scope const &scope,
    asts::TypeAst const *name)
    -> TypeSymbol* {
    // Recursively search the super scopes for a type symbol.
    for (auto const *sup_scope : scope.DirectSupScopes) {
        if (const auto sym = sup_scope->GetTypeSymbol(name, true); sym != nullptr) { return sym; }
    }

    // No symbol was found, so return nullptr.
    return nullptr;
}

auto spp::analyse::scopes::Scope::ShiftForNamespacedType(
    Scope const &scope,
    asts::TypeAst const &fq_type)
    -> Pair<const Scope*, asts::TypeIdentifierAst*> {
    // Get the namespace and type parts, to get the scopes.
    const auto ns_parts = fq_type.NsParts();
    const auto type_parts = fq_type.TypeParts();
    auto shifted_scope = &scope;

    // Iterate to move through the namespace parts first.
    for (const auto ns_part : ns_parts) {
        const auto sym = shifted_scope->GetNsSymbol(ns_part);
        if (sym == nullptr) { break; }
        shifted_scope = sym->LinkedScope;
    }

    // Iterate through the type parts (except the final one) next.
    for (auto const &type_part : type_parts | spp::views::drop_last(1)) {
        const auto sym = shifted_scope->GetTypeSymbol(type_part);
        if (sym == nullptr or sym->IsGeneric) { break; }
        shifted_scope = sym->LinkedScope;
    }

    // Return the type scope, and the final type part.
    auto final = type_parts.Back();
    return MakePair(shifted_scope, std::move(final));
}

auto spp::analyse::scopes::Scope::GetErrorFormatter() const
    -> spp::utils::errors::ErrorFormatter* {
    // Return this scope's error formatter, or the parent's if it doesn't exist.
    const auto this_formatter = _ErrorFormatter;
    return this_formatter != nullptr ? this_formatter : Parent->GetErrorFormatter();
}

auto spp::analyse::scopes::Scope::GetGenerics() const
    -> Vec<Unique<asts::GenericArgumentAst>> {
    // Create the symbols list.
    auto syms = Vec<Unique<asts::GenericArgumentAst>>();
    const auto scopes = Ancestors();
    auto type_names = Vec<asts::TypeIdentifierAst*>();
    auto comp_names = Vec<asts::IdentifierAst*>();

    // Check each ancestor scope, accumulating generic symbols.
    for (auto const *scope : scopes) {
        auto all_type_syms = scope->AllTypeSymbols(true)
            | std::views::filter([](auto const &sym) { return sym->IsGeneric; })
            | std::ranges::to<Vec>();

        for (auto const &t : all_type_syms) {
            if (t->LinkedScope == nullptr) { continue; } // unresolved or Self - no concrete value to pre-seed
            if (std::ranges::contains(type_names, *t->Name, spp::meta::deref)) { continue; }
            syms.EmplaceBack(asts::GenericArgumentTypeKeywordAst::FromSym(*t));
            type_names.EmplaceBack(t->Name);
        }

        auto all_var_syms = scope->AllVarSymbols(true)
            | std::views::filter([](auto const &sym) { return sym->IsGeneric; })
            | std::ranges::to<Vec>();

        for (auto const &v : all_var_syms) {
            if (std::ranges::contains(comp_names, *v->Name, spp::meta::deref)) { continue; }
            syms.EmplaceBack(asts::GenericArgumentCompKeywordAst::FromSym(*v));
            comp_names.EmplaceBack(v->Name);
        }
    }

    // Return the list of generic symbols.
    return syms;
}

auto spp::analyse::scopes::Scope::GetExtendedGenericSymbols(
    Vec<asts::GenericArgumentAst*> const &generics,
    asts::TypeAst const *ignore) const
    -> Vec<Symbol*> {
    // Convert the provided generic arguments into symbols. Todo: filter to "is_generic"?
    const auto type_syms = generics
        | spp::views::cast_dynamic<asts::GenericArgumentTypeAst*>
        | std::views::transform([this](auto const &gen_arg) { return GetTypeSymbol(gen_arg->Val.Get()); })
        | std::views::filter([](auto const &sym) { return sym != nullptr and sym->IsGeneric; })
        | spp::views::cast_dynamic<Symbol*>
        | std::ranges::to<Vec>();

    const auto comp_syms = generics
        | spp::views::cast_dynamic<asts::GenericArgumentCompAst*>
        | std::views::filter([&ignore](auto const &gen_arg) { return ignore == nullptr or *gen_arg->Val != *ignore; })
        | std::views::transform([this](auto const &gen_arg) { return GetVarSymbol(gen_arg->Val->template To<asts::IdentifierAst>()); })
        | std::views::filter([](auto const &sym) { return sym != nullptr and sym->IsGeneric; })
        | spp::views::cast_dynamic<Symbol*>
        | std::ranges::to<Vec>();

    auto syms = type_syms;
    syms.AppendRange(comp_syms);

    // Re-use above logic to collect generic symbols from the ancestor scopes.
    const auto scopes = Ancestors()
        | std::views::take_while([](auto *scope) { return not std::holds_alternative<ScopeIdentifierName>(scope->Name); })
        | std::ranges::to<Vec>();

    for (auto const *scope : scopes) {
        for (auto const &sym : scope->AllTypeSymbols(true)
             | std::views::filter([](auto s) { return s->IsGeneric; })) {
            // auto clone = MakeUnique<TypeSymbol>(sym->Name, nullptr, nullptr, nullptr, nullptr, true);
            // clone->IsDirectlyCopyable = sym->IsDirectlyCopyable;
            // clone->IsDirectlyZeroType = sym->IsDirectlyZeroType;
            // clone->GenericConstraints = sym->GenericConstraints;
            syms.EmplaceBack(sym);
        }

        for (auto const &sym : scope->AllVarSymbols(true)
             | std::views::filter([](auto const &s) { return s->IsGeneric; })
             | std::views::filter([&ignore](auto const &s) { return ignore == nullptr or *s->Name == *ignore; })) {
            syms.EmplaceBack(sym);
        }
    }

    // Return the full list of generic symbols.
    return syms;
}

auto spp::analyse::scopes::Scope::AddVarSymbol(
    Unique<VariableSymbol> &&sym)
    -> void {
    // Add a type symbol to the corresponding symbol table.
    InternalTable.VarTbl.Add(sym->Name, std::move(sym));
}

auto spp::analyse::scopes::Scope::AddVarSymbolCheckConflict(
    Unique<VariableSymbol> &&sym)
    -> void {
    // Cannot allow for duplicate comptime definitions.
    const auto existing_sym = GetVarSymbol(sym->Name, false);
    if (existing_sym != nullptr) {
        // const auto is_generic = sym->IsGeneric;
        // const auto is_comptime = sym->MemInfo->AstCompTime != nullptr;
        const auto is_functional = existing_sym->Type != nullptr and existing_sym->Type->IsCompilerGeneratedType();
        RaiseIf<errors::SppIdentifierDuplicateError>(
            not is_functional,
            {this, this},
            ERR_ARGS(*existing_sym->Name, *sym->Name, "comptime variable identifier"));
    }

    // Add a type symbol to the corresponding symbol table.
    InternalTable.VarTbl.Add(sym->Name, std::move(sym));
}

auto spp::analyse::scopes::Scope::AddTypeSymbol(
    Unique<TypeSymbol> &&sym)
    -> void {
    // Add a type symbol to the corresponding symbol table.
    InternalTable.TypeTbl.Add(sym->Name, std::move(sym));
}

auto spp::analyse::scopes::Scope::AddTypeSymbolCheckConflict(
    Unique<TypeSymbol> &&sym)
    -> void {
    // Cannot allow for duplicate definitions.
    const auto existing_sym = GetTypeSymbol(sym->Name, false);
    if (existing_sym != nullptr) {
        const auto is_functional = sym->Name->IsCompilerGeneratedType();
        RaiseIf<errors::SppIdentifierDuplicateError>(
            not is_functional,
            {existing_sym->ScopeDefinedIn, sym->ScopeDefinedIn},
            ERR_ARGS(*existing_sym->Name, *sym->Name, "type identifier"));
    }

    // Add a type symbol to the corresponding symbol table.
    InternalTable.TypeTbl.Add(sym->Name, std::move(sym));
}

auto spp::analyse::scopes::Scope::AddNsSymbol(
    Unique<NamespaceSymbol> &&sym)
    -> void {
    // Add a namespace symbol to the corresponding symbol table.
    InternalTable.NsTbl.Add(sym->Name, std::move(sym));
}

auto spp::analyse::scopes::Scope::RemVarSymbol(
    asts::IdentifierAst const *sym_name)
    -> Unique<VariableSymbol> {
    // Remove a variable symbol from the corresponding symbol table.
    return InternalTable.VarTbl.Rem(sym_name);
}

auto spp::analyse::scopes::Scope::RemTypeSymbol(
    asts::TypeIdentifierAst const *sym_name)
    -> Unique<TypeSymbol> {
    // Remove a type symbol from the corresponding symbol table.
    return InternalTable.TypeTbl.Rem(sym_name);
}

auto spp::analyse::scopes::Scope::RemNsSymbol(
    asts::IdentifierAst const *sym_name)
    -> Unique<NamespaceSymbol> {
    // Remove a namespace symbol from the corresponding symbol table.
    return InternalTable.NsTbl.Rem(sym_name);
}

auto spp::analyse::scopes::Scope::AllVarSymbols(
    const bool exclusive,
    const bool sup_scope_search) const
    -> Vec<VariableSymbol*> {
    // Yield all symbols from the var symbol table.
    auto syms = InternalTable.VarTbl.All();

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and Parent != nullptr) {
        syms.AppendRange(Parent->AllVarSymbols(exclusive, sup_scope_search));
    }

    // For super scope searches, yield from all direct super scopes.
    if (sup_scope_search) {
        for (auto const *sup_scope : SupScopes()) {
            syms.AppendRange(sup_scope->AllVarSymbols(true, false));
        }
    }

    return syms;
}

auto spp::analyse::scopes::Scope::AllTypeSymbols(
    const bool exclusive,
    const bool sup_scope_search) const
    -> Vec<TypeSymbol*> {
    // Yield all symbols from the type symbol table.
    auto syms = InternalTable.TypeTbl.All();

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and Parent != nullptr) {
        syms.AppendRange(Parent->AllTypeSymbols(exclusive, sup_scope_search));
    }

    // For super scope searches, yield from all direct super scopes.
    if (sup_scope_search) {
        for (auto const *sup_scope : DirectSupScopes) {
            syms.AppendRange(sup_scope->AllTypeSymbols(true, false));
        }
    }

    return syms;
}

auto spp::analyse::scopes::Scope::AllNsSymbols(
    const bool exclusive, bool) const
    -> Vec<NamespaceSymbol*> {
    auto syms = InternalTable.NsTbl.All();

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and Parent != nullptr) {
        syms.AppendRange(Parent->AllNsSymbols(exclusive));
    }
    return syms;
}

auto spp::analyse::scopes::Scope::HasVarSymbol(
    asts::IdentifierAst const *sym_name,
    const bool exclusive) const
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return GetVarSymbol(sym_name, exclusive) != nullptr;
}

auto spp::analyse::scopes::Scope::HasTypeSymbol(
    asts::TypeAst const *sym_name,
    const bool exclusive) const
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return GetTypeSymbol(sym_name, exclusive) != nullptr;
}

auto spp::analyse::scopes::Scope::HasNsSymbol(
    asts::IdentifierAst const *sym_name,
    const bool exclusive) const
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return GetNsSymbol(sym_name, exclusive) != nullptr;
}

auto spp::analyse::scopes::Scope::GetVarSymbol(
    asts::IdentifierAst const *sym_name,
    const bool exclusive,
    const bool sup_scope_search) const
    -> VariableSymbol* {
    // Get the symbol from the symbol table if it exists.
    const auto scope = this;
    auto sym = InternalTable.VarTbl.Get(sym_name);

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
        sym = scope->Parent->GetVarSymbol(sym_name, exclusive);
    }

    // If the symbol still hasn't been found, check the super scopes for it.
    if (sym == nullptr and sup_scope_search) {
        sym = SearchSupScopesForVar(*scope, sym_name);
    }

    // Check for a linked aliased variable symbol.
    if (sym != nullptr and sym->AliasSym != nullptr) {
        sym = sym->AliasSym;
    }

    // Return the found symbol, or nullptr.
    return sym;
}

auto spp::analyse::scopes::Scope::GetTypeSymbol(
    asts::TypeAst const *sym_name,
    const bool exclusive,
    const bool sup_scope_search) const
    -> TypeSymbol* {
    // Adjust the scope for the namespace of the type identifier if there is one.
    if (sym_name == nullptr) { return nullptr; }

    auto scope = this;
    auto sym_name_extracted = static_cast<asts::TypeIdentifierAst const*>(nullptr);
    if (sym_name->IsTypeIdentifier()) {
        sym_name_extracted = dynamic_cast<asts::TypeIdentifierAst const*>(sym_name);
    }
    else {
        auto [scope_, sym_name_extracted_] = ShiftForNamespacedType(*this, *sym_name);
        scope = scope_;
        sym_name_extracted = sym_name_extracted_;
    }

    // Get the symbol from the symbol table if it exists.
    auto sym = scope->InternalTable.TypeTbl.Get(sym_name_extracted);

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
        sym = scope->Parent->GetTypeSymbol(sym_name_extracted, exclusive);
    }

    // If the symbol still hasn't been found, check the super scopes for it.
    if (sym == nullptr and sup_scope_search) {
        sym = SearchSupScopesForType(*scope, sym_name_extracted);
    }

    // Return the found symbol, or nullptr.
    return sym;
}

auto spp::analyse::scopes::Scope::GetNsSymbol(
    asts::IdentifierAst const *sym_name,
    const bool exclusive) const
    -> NamespaceSymbol* {
    // Get the symbol from the symbol table if it exists.
    if (sym_name == nullptr) { return nullptr; }
    const auto scope = this;
    auto sym = InternalTable.NsTbl.Get(sym_name);

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->Parent != nullptr) {
        sym = scope->Parent->GetNsSymbol(sym_name, exclusive);
    }

    // Return the found symbol, or nullptr.
    return sym;
}

auto spp::analyse::scopes::Scope::GetVarSymbolOutermost(
    asts::Ast const &expr) const
    -> Pair<VariableSymbol*, Scope const*> {
    // Define helper methods to check expression types.
    auto is_valid_postfix_expression = []<typename OpType>(auto *ast) -> bool {
        auto postfix_expr = ast->template To<asts::PostfixExpressionAst>();
        if (postfix_expr == nullptr) { return false; }

        auto postfix_op = postfix_expr->Op->template To<OpType>();
        return postfix_op != nullptr;
    };

    auto is_valid_postfix_expression_runtime = [is_valid_postfix_expression](auto *ast) -> bool {
        return is_valid_postfix_expression.operator()<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(ast);
    };

    auto is_valid_postfix_expression_static = [is_valid_postfix_expression](auto *ast) -> bool {
        return is_valid_postfix_expression.operator()<asts::PostfixExpressionOperatorStaticMemberAccessAst>(ast);
    };

    auto adjusted_name = &expr;
    if (is_valid_postfix_expression_runtime(&expr)) {
        // Keep moving into the left-hand-side until there is no left-hand-side: "a.b.c" becomes "a".
        while (is_valid_postfix_expression_runtime(adjusted_name)) {
            adjusted_name = adjusted_name->To<asts::PostfixExpressionAst>()->Lhs.Get();
        }

        // Get the symbol (will be in this scope), and return it with the scope.
        auto sym = GetVarSymbol(adjusted_name->To<asts::IdentifierAst>());
        return MakePair(sym, this);
    }

    if (is_valid_postfix_expression_static(&expr)) {
        // This is possible with a left-hand-side type or namespace.
        const auto postfix_expr = expr.To<asts::PostfixExpressionAst>();
        const auto postfix_op = postfix_expr->Op->To<asts::PostfixExpressionOperatorStaticMemberAccessAst>();

        // Type based left-hand-side, such as "some_namespace::Type::static_member()"
        if (const auto type_lhs = postfix_expr->Lhs->To<asts::TypeAst>()) {
            const auto type_sym = GetTypeSymbol(type_lhs);
            const auto var_sym = type_sym->LinkedScope->GetVarSymbol(postfix_op->Name.Get());
            return MakePair(var_sym, const_cast<Scope const*>(type_sym->LinkedScope));
        }

        // Namespace based left-hand-side, such as "a::b::c::my_function()"
        auto namespace_scope = this;
        if (is_valid_postfix_expression_static(adjusted_name)) {
            adjusted_name = adjusted_name->To<asts::PostfixExpressionAst>()->Lhs.Get();
            namespace_scope = namespace_scope->ConvertPostfixToNestedScope(adjusted_name->To<asts::ExpressionAst>());
        }
        auto sym = namespace_scope ? namespace_scope->GetVarSymbol(postfix_op->Name.Get()) : nullptr;
        return MakePair(sym, namespace_scope);
    }

    // Identifiers or non-symbolic expressions can use the normal lookup.
    auto sym = GetVarSymbol(adjusted_name->To<asts::IdentifierAst>());
    return MakePair(sym, this);
}

auto spp::analyse::scopes::Scope::DepthDiff(
    const Scope *scope) const
    -> sys::ssize_t {
    // Create an internal function to call recursively with a counter.
    auto func = [](this auto &&self, const Scope *source, const Scope *target, const sys::ssize_t depth) -> sys::ssize_t {
        if (source == target) { return depth; }
        for (auto const *sup_scope : source->DirectSupScopes) {
            if (const auto result = self(sup_scope, target, depth + 1z); result >= 0z) {
                return result;
            }
        }
        return -1;
    };

    return func(this, scope, 0z);
}

auto spp::analyse::scopes::Scope::FinalChildScope() const
    -> Scope const* {
    // If there are no children, return this scope (base case for the recursion).
    if (Children.IsEmpty()) { return this; }

    // Otherwise, return the final child scope (recursively searching).
    return Children.Back()->FinalChildScope();
}

auto spp::analyse::scopes::Scope::Ancestors() const
    -> Vec<Scope const*> {
    // Get all ancestor scopes, including this scope, and the global scope.
    auto scopes = Vec<Scope const*>();
    for (auto *scope = this; scope != nullptr; scope = scope->Parent) {
        scopes.EmplaceBack(scope);
    }
    return scopes;
}

auto spp::analyse::scopes::Scope::ParentModule() const
    -> Scope* {
    // Get the parent module scope, if it exists.
    for (auto *scope = this; scope != nullptr; scope = scope->Parent) {
        if (std::holds_alternative<ScopeIdentifierName>(scope->Name)) {
            return const_cast<Scope*>(scope); // TODO: REMOVE CONST CAST
        }
    }
    return nullptr;
}

auto spp::analyse::scopes::Scope::TopLevelParentModule() const
    -> Scope* {
    // Get the top level parent module scope (ie until the parent is the global scope).
    for (auto *scope = this; scope != nullptr; scope = scope->Parent) {
        const auto next_scope = scope->Parent;
        if (std::holds_alternative<ScopeBlockName>(next_scope->Name) and std::get<ScopeBlockName>(next_scope->Name).Name.contains("__global__")) {
            return const_cast<Scope*>(scope); // TODO: REMOVE CONST CAST
        }
    }
    return nullptr;
}

auto spp::analyse::scopes::Scope::GetEnclosingTypeScope(
    asts::meta::CompilerMetaData const &meta) const
    -> Scope* {
    // If the current scope is a lambda scope, use the original scope that it overrode.
    if (const auto block_name = std::get_if<ScopeBlockName>(&Name)) {
        if (block_name != nullptr and block_name->Name.contains("<closure-inner")) {
            return meta.OverriddenScopeForClosure->GetEnclosingTypeScope(meta);
        }
    }

    // Walk up the scope chain. Return the first non-compiler-generated type scope, or the LinkedScope of a "Self" type
    // symbol found in a sup-block scope (for module-level sup blocks that have no type scope in their chain).
    for (auto *scope = this; scope != nullptr; scope = scope->Parent) {
        if (scope->TySym != nullptr and not scope->TySym->Name->Name.starts_with("$")) {
            return const_cast<Scope*>(scope);
        }
        for (auto const &ty_sym : scope->InternalTable.TypeTbl.All()) {
            if (ty_sym->Name->Name == "Self" and ty_sym->LinkedScope != nullptr) {
                return ty_sym->LinkedScope;
            }
        }
    }
    return nullptr;
}

auto spp::analyse::scopes::Scope::GetEnclosingSelfType(
    asts::meta::CompilerMetaData const &meta) const
    -> Unique<asts::TypeAst> {
    // If we are already in a module scope, there is no self type.
    auto current_scope = this;
    if (std::holds_alternative<ScopeIdentifierName>(current_scope->Name)) {
        return nullptr;
    }

    while (true) {
        // Escape closure scopes for the Self type.
        if (current_scope->NameAsString().starts_with("<closure-outer")) {
            current_scope = meta.OverriddenScopeForClosure;
        }

        // Only get a scope right under the module scope.
        if (not std::holds_alternative<ScopeIdentifierName>(current_scope->Parent->Name)) {
            current_scope = current_scope->Parent;
            continue;
        }
        return asts::AstClone(asts::AstName(current_scope->AstNode));
    }
    return nullptr;
}

auto spp::analyse::scopes::Scope::SupScopes() const
    -> Vec<Scope*> {
    // Get all super scopes, recursively.
    auto scopes = Vec<Scope*>();
    for (auto *scope : DirectSupScopes) {
        const auto child_scopes = scope->SupScopes();
        scopes.EmplaceBack(scope);
        scopes.AppendRange(child_scopes);
    }
    return scopes;
}

auto spp::analyse::scopes::Scope::SupTypes() const
    -> Vec<asts::TypeAst*> {
    // Get all super types, recursively (filter and map the super scopes).
    // For original generic class scopes, TySym is the bare symbol (no generic args) and _ClsSym holds
    // the parameterised form (e.g. FwdRef[T=T]). Using bare TySym would drop the generic params, so we
    // use _ClsSym->FqName() when TySym has no generic args. Newly-created concrete scopes always have a
    // non-empty GnArgGroup in TySym, so those fall through to the normal path.
    static const auto resolve_fq_name = [](auto *scope) -> asts::TypeAst* {
        const auto cls_proto = scope->AstNode->template To<asts::ClassPrototypeAst>();
        const auto cls_sym = cls_proto ? cls_proto->GetClsSym() : nullptr;
        if (cls_sym and scope->TySym->Name->GnArgGroup->Args.IsEmpty()) {
            return cls_sym->FqName();
        }
        return scope->TySym->FqName();
    };

    return SupScopes()
        | std::views::filter([](auto *scope) { return scope->AstNode->template To<asts::ClassPrototypeAst>(); })
        | std::views::transform(resolve_fq_name)
        | std::ranges::to<Vec>();
}

auto spp::analyse::scopes::Scope::DirectSupTypes() const
    -> Vec<asts::TypeAst*> {
    // Get all direct super types (filter and map the direct super scopes).
    static const auto resolve_fq_name = [](auto *scope) -> asts::TypeAst* {
        const auto cls_proto = scope->AstNode->template To<asts::ClassPrototypeAst>();
        const auto cls_sym = cls_proto ? cls_proto->GetClsSym() : nullptr;
        if (cls_sym and scope->TySym->Name->GnArgGroup->Args.IsEmpty()) {
            return cls_sym->FqName();
        }
        return scope->TySym->FqName();
    };
    return DirectSupScopes
        | std::views::filter([](auto *scope) { return scope->AstNode->template To<asts::ClassPrototypeAst>(); })
        | std::views::transform(resolve_fq_name)
        | std::ranges::to<Vec>();
}

auto spp::analyse::scopes::Scope::ConvertPostfixToNestedScope(
    asts::ExpressionAst const *postfix_ast) const
    -> Scope const* {
    // Get the left-hand-side namespace's member's type.
    if (const auto lhs_as_ident = postfix_ast->To<asts::IdentifierAst>()) {
        const auto ns_sym = GetNsSymbol(lhs_as_ident);
        return ns_sym ? ns_sym->LinkedScope : nullptr;
    }

    // Postfix lhs -> get the ns scopes.
    auto lhs = postfix_ast;
    auto namespaces = Vec<asts::IdentifierAst*>();
    while (auto const *postfix_lhs = lhs->To<asts::PostfixExpressionAst>()) {
        const auto op = postfix_lhs->Op->To<asts::PostfixExpressionOperatorStaticMemberAccessAst>();
        namespaces.EmplaceBack(op->Name->To<asts::IdentifierAst>());
        lhs = postfix_lhs->Lhs.Get();
    }
    if (auto *lhs_as_ident = lhs->To<asts::IdentifierAst>()) {
        // todo: is the condition requires? just body.
        namespaces.EmplaceBack(const_cast<asts::IdentifierAst*>(lhs_as_ident));
    }

    auto scope = this;
    for (auto const *ns : namespaces | std::views::reverse) {
        const auto ns_sym = scope->GetNsSymbol(ns);
        scope = ns_sym ? ns_sym->LinkedScope : nullptr;
        if (scope == nullptr) { break; }
    }
    return scope;
}

auto spp::analyse::scopes::Scope::PrintScopeTree() const
    -> Str {
    //
    using spp::utils::functions::Overload;

    // Indent the children, print the scope name.
    auto func = [](this auto &&self, Scope const *scope, Str const &indent) -> Str {
        auto result = indent + std::visit(
            Overload{
                [](ScopeIdentifierName const &id) { return id.Name->Val; },
                [](ScopeTypeIdentifierName const &id) { return id.Name->Name; },
                [](ScopeBlockName const &block) { return block.Name; }
            }, scope->Name) + "\n";

        for (auto child : scope->Children | spp::views::ptr) {
            result += self(child, indent + "    ");
        }
        return result;
    };

    return func(this, "");
}

auto spp::analyse::scopes::Scope::NameAsString() const
    -> Str {
    // Identifier based scope name.
    if (std::holds_alternative<ScopeIdentifierName>(Name)) {
        auto const name_as_id = std::get<ScopeIdentifierName>(Name).Name.Get();
        return name_as_id->ToString();
    }

    // TypeIdentifier based scope name.
    if (std::holds_alternative<ScopeTypeIdentifierName>(Name)) {
        auto const name_as_type = std::get<ScopeTypeIdentifierName>(Name).Name;
        return name_as_type->ToString();
    }

    // Block name.
    auto const name_as_block_name = std::get<ScopeBlockName>(Name).Name;
    return name_as_block_name;
}

auto spp::analyse::scopes::Scope::FixChildrenToParentPointer()
    -> void {
    // Iterate all children, setting their parent pointer to this scope. Recurse into them.
    for (auto const &child : Children) {
        child->Parent = this;
        child->FixChildrenToParentPointer();
    }
}

SPP_MOD_END
