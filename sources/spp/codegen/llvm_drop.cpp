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
  using analyse::utils::type_members::GetAllParts;
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
  auto const *const bare_sym = type_sym.AsBoundSymbol();
  const auto bare_name = bare_sym->FqName();

  // Only a type that has no destructor of its own is destroyed part by part - its elements when it is a tuple or an
  // array, its attributes otherwise. A struct's parts are reached through its lowered form, which has to be one.
  const auto is_indexable = IsTypeCompTimeIndexable(*bare_name, *sm->CurrentScope);
  const auto is_arr = is_indexable and IsTypeArr(*bare_name, *sm->CurrentScope);
  if (not is_indexable and not llvm::isa<llvm::StructType>(elem_ty)) { return; }

  const auto parts = GetAllParts(*bare_name, *sm->CurrentScope);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

  // Reverse order: the last part built is the first one
  // destroyed, mirroring the order they were initialized in.
  for (auto i = parts.Len(); i > 0uz; --i) {
    auto const &part = parts[i - 1uz];
    if (part.Sym == nullptr or part.Sym == &type_sym or part.Sym == bare_sym) { continue; }
    if (not NeedsDrop(*part.Sym, *sm, meta)) { continue; }

    // An array is one value repeated, so its elements are reached by indexing into it; a tuple's and a struct's are
    // separate fields. The S++ layout re-orders a struct's fields to minimize padding, so a declaration index has to
    // be mapped through to wherever the field actually ended up.
    const auto index = static_cast<std::uint32_t>(part.Index);
    const auto part_ptr = is_arr
      ? ctx->Builder.CreateGEP(
        elem_ty, ptr, {llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, index)},
        "drop.elem" + uid + "." + part.Step)
      : is_indexable
        ? ctx->Builder.CreateStructGEP(elem_ty, ptr, index, "drop.elem" + uid + "." + part.Step)
        : ctx->Builder.CreateStructGEP(
          elem_ty, ptr, GetPhysicalFieldIndex(*type_sym.LlvmInfo, part.Index),
          "drop.field" + uid + "." + part.Step);
    EmitDrop(*part.Sym, part_ptr, sm, meta, ctx);
  }
}
