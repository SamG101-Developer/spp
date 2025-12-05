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
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.lex.tokens;

import genex;
import llvm;


spp::asts::ClassPrototypeAst::ClassPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(tok_cls) &&tok_cls,
    decltype(name) name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(impl) &&impl) :
    m_cls_sym(nullptr),
    annotations(std::move(annotations)),
    tok_cls(std::move(tok_cls)),
    name(std::move(name)),
    generic_param_group(std::move(generic_param_group)),
    impl(std::move(impl)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_cls, lex::SppTokenType::KW_CLS, "cls");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_param_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->impl);
}


auto spp::asts::ClassPrototypeAst::pos_start() const
    -> std::size_t {
    return tok_cls->pos_start();
}


auto spp::asts::ClassPrototypeAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::ClassPrototypeAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<ClassPrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_cls),
        ast_clone(name),
        ast_clone(generic_param_group),
        ast_clone(impl));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->m_cls_sym = m_cls_sym;
    ast->visibility = visibility;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto &&a) { a->set_ast_ctx(ast); });
    return ast;
}


spp::asts::ClassPrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_cls).append(" ");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(generic_param_group).append(" ");
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::ClassPrototypeAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_cls).append(" ");
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(generic_param_group).append(" ");
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::ClassPrototypeAst::m_generate_symbols(
    ScopeManager *sm)
    -> analyse::scopes::TypeSymbol* {
    auto sym_name = ast_clone(name->type_parts()[0]);
    sym_name->generic_arg_group = GenericArgumentGroupAst::from_params(*generic_param_group);

    // Create the symbols as TypeSymbol pointers, so AliasSymbols can also be used.
    std::shared_ptr<analyse::scopes::TypeSymbol> symbol_1 = nullptr;
    std::shared_ptr<analyse::scopes::TypeSymbol> symbol_2 = nullptr;

    // Create the symbol for the type, include generics if applicable, like Vec[T].
    symbol_1 = std::make_unique<analyse::scopes::TypeSymbol>(
        std::move(sym_name), this, sm->current_scope, sm->current_scope, sm->current_scope->parent_module());
    sm->current_scope->ty_sym = symbol_1;
    sm->current_scope->parent->add_type_symbol(symbol_1);
    m_cls_sym = sm->current_scope->ty_sym;

    // If the type was generic, like Vec[T], also create a base Vec symbol.
    if (not generic_param_group->params.empty()) {
        symbol_2 = std::make_unique<analyse::scopes::TypeSymbol>(
            ast_clone(name->type_parts()[0]), this, sm->current_scope, sm->current_scope,
            sm->current_scope->parent_module());
        symbol_2->generic_impl = symbol_1.get();
        sm->current_scope->ty_sym = symbol_2;
        const auto ret_sym = symbol_2.get();
        sm->current_scope->parent->add_type_symbol(symbol_2);
        return ret_sym;
    }

    return m_cls_sym.get();
}


auto spp::asts::ClassPrototypeAst::m_fill_llvm_mem_layout(
    analyse::scopes::ScopeManager *sm,
    analyse::scopes::TypeSymbol const *type_sym,
    codegen::LLvmCtx *ctx)
    -> void {
    // Collect the scope and sup scopes to get all attributes.
    auto types = std::vector{type_sym->scope}
        | genex::views::concat(type_sym->scope->sup_scopes())
        | genex::views::transform([](auto *x) { return x->ast; })
        | genex::views::cast_dynamic<ClassPrototypeAst*>()
        | genex::views::transform([](auto *x) { return x->impl->members | genex::views::ptr | genex::to<std::vector>(); })
        | genex::views::join
        | genex::views::cast_dynamic<ClassAttributeAst*>()
        | genex::views::transform([sm](auto *x) { return sm->current_scope->get_type_symbol(x->type)->llvm_info->llvm_type; })
        | genex::to<std::vector>();

    const auto llvm_type_struct = llvm::StructType::create(
        ctx->context, std::move(types), codegen::mangle::mangle_type_name(*type_sym));
    type_sym->llvm_info->llvm_type = llvm_type_struct;
}


auto spp::asts::ClassPrototypeAst::register_generic_substituted_scope(
    analyse::scopes::Scope *scope,
    std::unique_ptr<ClassPrototypeAst> &&new_ast)
    -> void {
    // Just somewhere to store the new_ast as a unique_ptr.
    m_generic_substituted_scopes.emplace_back(scope, std::move(new_ast));
}


auto spp::asts::ClassPrototypeAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing annotations and the body.
    Ast::stage_1_pre_process(ctx);
    annotations | genex::views::for_each([this](auto &&x) { x->stage_1_pre_process(this); });
    impl->stage_1_pre_process(this);
}


auto spp::asts::ClassPrototypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create the class scope, which is the scope for the class prototype.
    auto scope_name = analyse::scopes::ScopeName(std::dynamic_pointer_cast<TypeIdentifierAst>(name));
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    // Run the generation steps for the annotations.
    annotations | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });

    // Generate the symbols for the class prototype, and handle generic parameters.
    meta->cls_sym = m_generate_symbols(sm);
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    impl->stage_2_gen_top_level_scopes(sm, meta);

    // Move out of the class scope, as the class scope is now complete.
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Skip the class scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the types in the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    generic_param_group->stage_4_qualify_types(sm, meta);
    impl->stage_4_qualify_types(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load the super scopes for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_5_load_super_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_6_pre_analyse_semantics(sm, meta);

    // Check the type isn't recursive.
    if (auto &&recursion = analyse::utils::type_utils::is_type_recursive(*this, *sm)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppRecursiveTypeError>()
            .with_args(*this, *recursion)
            .raises_from(sm->current_scope);
    }

    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse semantics for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    generic_param_group->stage_7_analyse_semantics(sm, meta);
    impl->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check memory for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_8_check_memory(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_9_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::ClassPrototypeAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the class symbol.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    const auto cls_sym = sm->current_scope->ty_sym;

    // For compiler known types, specialize the llvm type symbols.
    const auto ancestor_names = sm->current_scope->ancestors()
        | genex::views::transform([](auto *x) { return std::dynamic_pointer_cast<TypeIdentifierAst>(x->ty_sym->name)->name; })
        | genex::to<std::vector>();

    if (ancestor_names == std::vector<std::string>{"std", "void", "Void"}) {
        // S++ Void uses the llvm "void" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getVoidTy(ctx->context);
    }
    if (ancestor_names == std::vector<std::string>{"std", "boolean", "Bool"}) {
        // S++ Bool uses the llvm "i1" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt1Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "S8"}) {
        // S++ S8 uses the llvm "i8" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt8Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "S16"}) {
        // S++ S16 uses the llvm "i16" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt16Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "S32"}) {
        // S++ S32 uses the llvm "i32" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt32Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "S64"}) {
        // S++ S64 uses the llvm "i64" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt64Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "S128"}) {
        // S++ S128 uses the llvm "i128" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt128Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "S256"}) {
        // S++ Bool uses the llvm "i1" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getIntNTy(ctx->context, 256);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "SSize"}) {
        // S++ SSize uses the operating system's pointer size type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getIntNTy(ctx->context, ctx->module->getDataLayout().getPointerSizeInBits());
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "U8"}) {
        // S++ U8 uses the llvm "i8" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt8Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "U16"}) {
        // S++ U16 uses the llvm "i16" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt16Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "U32"}) {
        // S++ U32 uses the llvm "i32" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt32Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "U64"}) {
        // S++ U64 uses the llvm "i64" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt64Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "U128"}) {
        // S++ U128 uses the llvm "i128" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getInt128Ty(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "U256"}) {
        // S++ U256 uses the llvm "i256" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getIntNTy(ctx->context, 256);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "USize"}) {
        // S++ USize uses the operating system's pointer size type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getIntNTy(ctx->context, ctx->module->getDataLayout().getPointerSizeInBits());
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "F8"}) {
        // S++ F8 uses the llvm "quarter" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getFloatingPointTy(ctx->context, llvm::APFloatBase::Float8E4M3());
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "F16"}) {
        // S++ F16 uses the llvm "half" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getHalfTy(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "F32"}) {
        // S++ F32 uses the llvm "float" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getFloatTy(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "F64"}) {
        // S++ F64 uses the llvm "double" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getDoubleTy(ctx->context);
    }
    else if (ancestor_names == std::vector<std::string>{"std", "number", "F128"}) {
        // S++ F128 uses the llvm "fp128" type.
        cls_sym->llvm_info->llvm_type = llvm::Type::getFP128Ty(ctx->context);
    }
    else {
        // No generation for "$" types.
        if (name->type_parts().back()->name[0] == '$') {
            sm->move_out_of_current_scope();
            return nullptr;
        }

        // If this is a raw generic class like Vec[T], then generate the generic implementations.
        if (genex::any_of(sm->current_scope->all_type_symbols() | genex::views::materialize, [](auto const &sym) { return sym->scope == nullptr; })) {
            for (auto &&[generic_scope, generic_ast] : m_generic_substituted_scopes) {
                generic_ast->m_fill_llvm_mem_layout(sm, generic_scope->ty_sym.get(), ctx);
            }
        }

        m_fill_llvm_mem_layout(sm, cls_sym.get(), ctx);
    }

    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::ClassPrototypeAst::get_cls_sym() const
    -> std::shared_ptr<analyse::scopes::TypeSymbol> {
    return m_cls_sym;
}
