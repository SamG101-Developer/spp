module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.local_variable_destructure_object_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.destructure_utils;
import spp.analyse.utils.type_utils;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.type_ast;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
spp::asts::LocalVariableDestructureObjectAst::LocalVariableDestructureObjectAst(
    decltype(Type) &&type,
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r) :
    Type(std::move(type)),
    TokL(std::move(tok_l)),
    Elems(std::move(elems)),
    TokR(std::move(tok_r)) {
    //
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
    Source.OriginalType = AstClone(Type);
}

spp::asts::LocalVariableDestructureObjectAst::~LocalVariableDestructureObjectAst() = default;

auto spp::asts::LocalVariableDestructureObjectAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::LocalVariableDestructureObjectAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::LocalVariableDestructureObjectAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LocalVariableDestructureObjectAst>(
        AstCloneShared(Type), AstClone(TokL), AstCloneVec(Elems), AstClone(TokR));
}

auto spp::asts::LocalVariableDestructureObjectAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Type);
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Elems, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::LocalVariableDestructureObjectAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppArgumentMissingError;
    using analyse::errors::SppMultipleRestPatternsError;
    using analyse::errors::SppVariableObjectDestructureWithBoundRestPatternError;
    using analyse::errors::SppTypeMismatchError;
    using analyse::utils::type_utils::TypeEq;

    // Get the value and analyse it and the type.
    const auto val = meta->LetStatementValue;
    const auto val_type = val->InferType(sm, meta);
    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type)->FqName()->WithConvention(AstClone(Type->GetConvention()));

    const auto cls_proto = sm->CurrentScope->GetTypeSymbol(Type)->Type;
    const auto cls_attrs = cls_proto != nullptr ? cls_proto->Impl->Members | genex::views::ptr | genex::to<Vec>() : Vec<Ast*>{};

    const auto attributes = cls_attrs
        | genex::views::cast_dynamic<ClassAttributeAst*>()
        | genex::to<Vec>();

    const auto multi_arg_skips = Elems
        | genex::views::ptr
        | genex::views::cast_dynamic<LocalVariableDestructureSkipMultipleArgumentsAst*>()
        | genex::to<Vec>();

    const auto assigned_attributes = Elems
        | genex::views::ptr
        | genex::views::filter([](auto const &x) { return x->template To<LocalVariableDestructureSkipMultipleArgumentsAst>() == nullptr; })
        | genex::views::transform([](auto const &x) { return x->ExtractName(); })
        | genex::to<Vec>();

    const auto missing_attributes = attributes
        | genex::views::transform([](auto const &x) { return x->Name; })
        | genex::to<Vec>()
        | genex::views::not_in(assigned_attributes, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

    // Check the type matches.
    RaiseIf<SppTypeMismatchError>(
        not TypeEq(*val_type, *Type, *sm->CurrentScope, *sm->CurrentScope, _FromCasePattern),
        {sm->CurrentScope}, ERR_ARGS(*val, *val_type, *Source.OriginalType, *Type));

    // Only 1 "multi-skip" allowed in a destructure.
    RaiseIf<SppMultipleRestPatternsError>(
        multi_arg_skips.Len() > 1,
        {sm->CurrentScope}, ERR_ARGS(*this, *multi_arg_skips[0], *multi_arg_skips[1]));

    // Multi skips cannot be bound.
    RaiseIf<SppVariableObjectDestructureWithBoundRestPatternError>(
        not multi_arg_skips.IsEmpty() and multi_arg_skips[0]->Binding != nullptr,
        {sm->CurrentScope}, ERR_ARGS(*this, *multi_arg_skips[0]));

    // Check all attributes are provided unless there is a multi-skip.
    RaiseIf<SppArgumentMissingError>(
        not missing_attributes.IsEmpty() and multi_arg_skips.IsEmpty(),
        {sm->CurrentScope}, ERR_ARGS(*missing_attributes[0], "attribute", *this, "destructure argument"));

    // Handle nested flow typing, like seen in the case pattern handler for object destructure.
    Shared<IdentifierAst> uid_name = nullptr;
    const ExpressionAst *effective_val = val;
    if (_FromCasePattern and not TypeEq(*val_type, *Type, *sm->CurrentScope, *sm->CurrentScope, false)) {
        const auto uid = spp::utils::Uid(this);
        uid_name = MakeShared<IdentifierAst>(PosStart(), uid);
        auto uid_var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, uid_name, nullptr);
        _CondLet = MakeUnique<LetStatementInitializedAst>(nullptr, std::move(uid_var), nullptr, nullptr, AstClone(val));
        _CondLet->Stage7_AnalyseSemantics(sm, meta);
        _CondSym = sm->CurrentScope->GetVarSymbol(uid_name);
        _FlowSym = MakeShared<analyse::scopes::VariableSymbol>(*_CondSym);
        _FlowSym->Type = Type;
        sm->CurrentScope->AddVarSymbol(_FlowSym);
        effective_val = uid_name.get();
    }

    // Create expanded "let" statements for each part of the destructure.
    for (const auto elem : Elems | genex::views::ptr) {
        // Skip any conversion for unbound multi argument skipping.
        if (elem->To<LocalVariableDestructureSkipMultipleArgumentsAst>() != nullptr) {
        }

        // Skip any conversion for single argument skipping.
        else if (const auto cast_elem_1 = elem->To<LocalVariableSingleIdentifierAst>(); cast_elem_1 != nullptr) {
            auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, AstClone(cast_elem_1->Name));
            auto pstfx = MakeUnique<PostfixExpressionAst>(AstClone(effective_val), std::move(field));
            auto new_ast = MakeUnique<LetStatementInitializedAst>(nullptr, AstClone(elem), nullptr, nullptr, std::move(pstfx));
            if (_FromCasePattern) { new_ast->Var->MarkFromCasePattern(); }
            new_ast->Stage7_AnalyseSemantics(sm, meta);
            _NewAsts.EmplaceBack(std::move(new_ast));
        }

        // Skip any conversion for single argument skipping.
        else if (elem->To<LocalVariableDestructureSkipSingleArgumentAst>() != nullptr) {
        }

        // Handle and other nested destructure or single identifier.
        else if (const auto cast_elem_2 = elem->To<LocalVariableDestructureAttributeBindingAst>(); cast_elem_2 != nullptr) {
            auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, AstClone(cast_elem_2->Name));
            auto pstfx = MakeUnique<PostfixExpressionAst>(AstClone(effective_val), std::move(field));
            auto new_ast = MakeUnique<LetStatementInitializedAst>(nullptr, AstClone(cast_elem_2->Val), nullptr, nullptr, std::move(pstfx));
            if (_FromCasePattern) { new_ast->Var->MarkFromCasePattern(); }
            new_ast->Stage7_AnalyseSemantics(sm, meta);
            _NewAsts.EmplaceBack(std::move(new_ast));
        }
    }
}

auto spp::asts::LocalVariableDestructureObjectAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the temp variable's memory first if flow typing introduced one.
    if (_CondLet) { _CondLet->Stage8_CheckMemory(sm, meta); }
    // Check the memory state of the elements.
    for (auto const &x : _NewAsts) { x->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::LocalVariableDestructureObjectAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve the temp variable first if flow typing introduced one.
    if (_CondLet) { _CondLet->Stage9_CompTimeResolve(sm, meta); }
    // Comptime resolve each element.
    for (auto const &x : _NewAsts) { x->Stage9_CompTimeResolve(sm, meta); }
}

auto spp::asts::LocalVariableDestructureObjectAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // If flow typing introduced a temp variable, generate it. _FlowSym replaced _CondSym in
    // the symbol table (same scope, same string key), so LocalVariableSingleIdentifierAst::Stage11
    // inside _CondLet already sets _FlowSym->LlvmInfo->Alloca — no copy needed.
    if (_CondLet) {
        _CondLet->Stage11_CodeGen(sm, meta, ctx);
    }
    // Generate the "let" statements for each element.
    for (auto const &ast : _NewAsts) { ast->Stage11_CodeGen(sm, meta, ctx); }
    return nullptr;
}

auto spp::asts::LocalVariableDestructureObjectAst::ExtractNames() const
    -> Vec<Shared<IdentifierAst>> {
    // Walk the nested bindings for variable names.
    return analyse::utils::destructure_utils::GetNestedBindingIdentifiers(Elems);
}

auto spp::asts::LocalVariableDestructureObjectAst::ExtractName() const
    -> Shared<IdentifierAst> {
    // No single identifier for destructured bindings.
    return analyse::utils::destructure_utils::UnmatchableSingleIdentifier(PosStart());
}

SPP_MOD_END
