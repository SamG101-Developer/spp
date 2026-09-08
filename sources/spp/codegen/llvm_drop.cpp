module;
#include <spp/macros.hpp>

module spp.codegen.llvm_drop;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_func;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.utils.uid;
import genex;

auto spp::codegen::EmitDrop(
  analyse::scopes::TypeSymbol const &type_sym,
  llvm::Value *ptr,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LlvmCtx *ctx)
  -> void {
  //
  using analyse::utils::drop_utils::FindDropOverload;
  using analyse::utils::drop_utils::NeedsDrop;
  using analyse::utils::type_members::GetAllAttrs;
  using analyse::utils::type_predicates::GetNthTypeOfIndexableType;
  using analyse::utils::type_predicates::IsIndexWithinBound;
  using analyse::utils::type_predicates::IsTypeArr;
  using analyse::utils::type_predicates::IsTypeCompTimeIndexable;

  // Destroying a value that owns nothing is a no-op;
  // an "S32" local, or a struct built only from them,
  // leaves no instructions behind at all.
  if (not NeedsDrop(type_sym, *sm, meta)) { return; }

  const auto uid = "." + spp::utils::Uid();

  // A generator is a bare coroutine handle, not a struct: destroying it means destroying the frame it refers to.
  // Nothing else frees that frame - a coroutine that ran to completion is parked on its final suspend and one
  // abandoned part-way is parked mid-body, and both hold their frame until someone destroys them.
  //
  // Todo: "llvm.coro.destroy" releases the frame's storage but runs no destructors for the values living in it, so a
  //  generator abandoned while holding owned locals still leaks those. That needs drops emitted into the coroutine's
  //  own cleanup path.
  if (analyse::utils::type_predicates::IsTypeGen(*type_sym.FqName(), *sm->CurrentScope)) {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    const auto handle = ctx->Builder.CreateLoad(ptr_ty, ptr, "drop.gen.handle" + uid);

    // A handle is null only if it was never assigned one; destroying that would fault.
    const auto func = ctx->Builder.GetInsertBlock()->getParent();
    const auto live_bb = llvm::BasicBlock::Create(*ctx->Context, "drop.gen.live" + uid, func);
    const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "drop.gen.done" + uid, func);
    ctx->Builder.CreateCondBr(ctx->Builder.CreateIsNotNull(handle, "drop.gen.ok" + uid), live_bb, done_bb);

    ctx->Builder.SetInsertPoint(live_bb);
    ctx->Builder.CreateIntrinsic(llvm::Intrinsic::coro_destroy, {}, {handle}, {}, "");
    ctx->Builder.CreateBr(done_bb);
    ctx->Builder.SetInsertPoint(done_bb);
    return;
  }

  // The type's own destructor runs before its attributes
  // are destroyed, because it is still free to read them.
  if (const auto drop_proto = FindDropOverload(type_sym, *sm, meta); drop_proto != nullptr) {
    // A "drop" reached only through this path may not have
    // been declared yet, because nothing in the source
    // ever called it. Declare it into the module that owns
    // its definition, exactly as a written call site would.
    if (drop_proto->GetLlvmFunc() == nullptr) {
      const auto owner_ctx = drop_proto->OwnerCtx();
      drop_proto->GenerateLlvmDeclaration(sm, meta, owner_ctx != nullptr ? owner_ctx : ctx);
    }

    if (drop_proto->GetLlvmFunc() != nullptr) {
      const auto drop_func = GetOrAddTargetIntoCurrentModule(
        *drop_proto->GetLlvmFunc()->Target, *GetEmissionModule(*ctx));

      // Destroying a value consumes it, so "drop" takes "self"
      // by move, which lowers to the value itself rather than
      // to a pointer to it. The value is loaded out of the
      // storage this is destroying through. Todo: Bandaid?
      const auto fn_ty = drop_proto->GetLlvmFunc()->Target->getFunctionType();
      const auto self_ty = fn_ty->getNumParams() > 0
        ? fn_ty->getParamType(0)
        : GetLlvmType(type_sym, ctx);
      const auto self_val = ctx->Builder.CreateLoad(self_ty, ptr, "drop.self" + uid);
      ctx->Builder.CreateCall(drop_func, {self_val});
    }
    return;
  }

  const auto elem_ty = GetLlvmType(type_sym, ctx);

  // A bound generic parameter stands for its argument: the symbol keeps the parameter's name ("T"), which is not a
  // name the checks below can read a tuple or an array off. "NeedsDrop" resolves through for the same reason.
  auto const *const bare_sym = type_sym.IsGeneric and type_sym.LinkedScope != nullptr
    and type_sym.LinkedScope->TySym != nullptr and type_sym.LinkedScope->TySym.get() != &type_sym
    ? type_sym.LinkedScope->TySym.get()
    : &type_sym;
  const auto bare_name = bare_sym->FqName();

  // Positional checks for tuple / array types, as they don't
  // hold attribute names.
  if (IsTypeCompTimeIndexable(*bare_name, *sm->CurrentScope)) {
    const auto is_arr = IsTypeArr(*bare_name, *sm->CurrentScope);
    const auto elems = IsIndexWithinBound(0uz, *bare_name, *sm->CurrentScope).second;
    const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

    // Reverse order, as with attributes below: the last
    // element built is the first one destroyed.
    for (auto i = elems; i > 0uz; --i) {
      const auto index = static_cast<std::uint32_t>(i - 1uz);
      const auto elem_type = GetNthTypeOfIndexableType(i - 1uz, *bare_name, *sm->CurrentScope);
      const auto elem_type_sym = sm->CurrentScope->GetTypeSymbol(elem_type.get());
      if (elem_type_sym == &type_sym or elem_type_sym == bare_sym) { continue; }
      if (not NeedsDrop(*elem_type_sym, *sm, meta)) { continue; }

      const auto name = "drop.elem" + uid + "." + std::to_string(index);
      const auto elem_ptr = is_arr
        ? ctx->Builder.CreateGEP(
          elem_ty, ptr, {llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, index)}, name)
        : ctx->Builder.CreateStructGEP(elem_ty, ptr, index, name);
      EmitDrop(*elem_type_sym, elem_ptr, sm, meta, ctx);
    }
    return;
  }

  // Only a type that has no destructor of its own is destroyed
  // attribute by attribute, in reverse declaration order,
  // mirroring the order they were initialized in.
  auto attrs = GetAllAttrs(*type_sym.FqName(), *sm->CurrentScope);
  const auto struct_ty = elem_ty;
  if (not llvm::isa<llvm::StructType>(struct_ty)) { return; }

  for (auto i = attrs.Len(); i > 0uz; --i) {
    const auto attr_index = i - 1uz;
    const auto attr_type_sym = std::get<1>(attrs[attr_index]);
    if (attr_type_sym == nullptr or attr_type_sym == &type_sym) { continue; }
    if (not NeedsDrop(*attr_type_sym, *sm, meta)) { continue; }

    // The S++ layout re-orders fields to minimize padding,
    // so the declaration index has to be mapped through to
    // wherever the field actually ended up in the lowered
    // struct.
    const auto field_index = GetPhysicalFieldIndex(*type_sym.LlvmInfo, attr_index);
    const auto field_ptr = ctx->Builder.CreateStructGEP(
      struct_ty, ptr, field_index, "drop.field" + uid + "." + std::get<0>(attrs[attr_index])->Val);
    EmitDrop(*attr_type_sym, field_ptr, sm, meta, ctx);
  }
}
