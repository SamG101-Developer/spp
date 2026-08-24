module;
#include <spp/macros.hpp>

module spp.codegen.llvm_drop;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.type_utils;
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
  using analyse::utils::drop_utils::FindDelOverload;
  using analyse::utils::drop_utils::NeedsDrop;
  using analyse::utils::type_utils::GetAllAttrs;

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
  if (analyse::utils::type_utils::IsTypeGen(*type_sym.FqName(), *sm->CurrentScope)) {
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
  if (const auto del_proto = FindDelOverload(type_sym, *sm, meta); del_proto != nullptr) {
    // A "del" reached only through a drop may not have
    // been declared yet, because nothing in the source
    // ever called it. Declare it into the module that owns
    // its definition, exactly as a written call site would.
    if (del_proto->GetLlvmFunc() == nullptr) {
      const auto owner_ctx = del_proto->OwnerCtx();
      del_proto->GenerateLlvmDeclaration(sm, meta, owner_ctx != nullptr ? owner_ctx : ctx);
    }

    if (del_proto->GetLlvmFunc() != nullptr) {
      const auto del_func = GetOrAddTargetIntoCurrentModule(
        *del_proto->GetLlvmFunc()->Target, *GetEmissionModule(*ctx));

      // "del" takes "&mut self", which lowers to a pointer
      // parameter, so the storage address is the argument.
      ctx->Builder.CreateCall(del_func, {ptr});
    }
  }

  // Then the attributes, in reverse declaration order,
  // mirroring the order they were initialized in.
  auto attrs = GetAllAttrs(*type_sym.FqName(), *sm);
  const auto struct_ty = GetLlvmType(type_sym, ctx);
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
auto spp::codegen::NeedsDropFlag(
  analyse::scopes::VariableSymbol const &sym)
  -> bool {
  // A move written below the scope the local was declared in - inside a branch body, a loop body, a nested block -
  // is one that control may or may not reach, so whether the value is still here to destroy is not knowable now.
  //
  // This is deliberately decided from where the moves are written rather than from "IsInconsistentlyMoved", which the
  // branch merge only sets when two analysed branches disagree: a "case" with no "else" has nothing to disagree with,
  // and its fall-through path - the one that does not move - is exactly the path that would otherwise leak.
  if (sym.MemInfo == nullptr) { return false; }
  return genex::any_of(sym.MemInfo->LlvmAstMoveSites, [&](auto const &site) {
    return spp::get<1>(site) != sym.ScopeDefinedIn;
  });
}

namespace {
  /**
   * The @c i1 recording whether @p sym still holds a value, creating it on first use. It is allocated in the entry
   * block and starts out cleared, so a path that never reaches the initialization destroys nothing.
   */
  auto GetOrCreateDropFlag(
    spp::analyse::scopes::VariableSymbol &sym,
    spp::codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    if (sym.LlvmInfo == nullptr) { return nullptr; }
    if (sym.LlvmInfo->DropFlag != nullptr) { return sym.LlvmInfo->DropFlag; }

    // Allocated in the entry block so that it dominates every branch that reads it, and cleared there too: a path
    // that never reaches the initialization must destroy nothing, the same answer as having been moved away.
    const auto flag_ty = llvm::Type::getInt1Ty(*ctx->Context);
    const auto flag = spp::codegen::LlvmEntryAlloca(flag_ty, "drop.flag." + sym.Name->Val, ctx);
    auto entry_builder = llvm::IRBuilder(flag->getParent(), std::next(flag->getIterator()));
    entry_builder.CreateStore(llvm::ConstantInt::getFalse(*ctx->Context), flag);

    sym.LlvmInfo->DropFlag = flag;
    return flag;
  }
}

auto spp::codegen::EmitDropFlagSet(
  analyse::scopes::VariableSymbol &sym,
  LlvmCtx *ctx)
  -> void {
  if (not NeedsDropFlag(sym)) { return; }
  const auto flag = GetOrCreateDropFlag(sym, ctx);
  if (flag == nullptr) { return; }
  ctx->Builder.CreateStore(llvm::ConstantInt::getTrue(*ctx->Context), flag);
}

auto spp::codegen::EmitDropFlagClear(
  analyse::scopes::VariableSymbol &sym,
  LlvmCtx *ctx)
  -> void {
  if (not NeedsDropFlag(sym)) { return; }
  const auto flag = GetOrCreateDropFlag(sym, ctx);
  if (flag == nullptr) { return; }
  ctx->Builder.CreateStore(llvm::ConstantInt::getFalse(*ctx->Context), flag);
}

auto spp::codegen::EmitDiscardedValueDrop(
  analyse::scopes::TypeSymbol const &type_sym,
  llvm::Value *value,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LlvmCtx *ctx)
  -> void {
  // A statement that produced nothing, or that ended the block, leaves nothing to destroy here.
  if (value == nullptr) { return; }
  const auto block = ctx->Builder.GetInsertBlock();
  if (block == nullptr or block->hasTerminator()) { return; }

  // Same rule as for a named local: an unwalked body's memory state means nothing, so nothing is destroyed from it.
  if (meta->EnclosingFunctionScope != nullptr and not meta->EnclosingFunctionScope->BodyMemoryAnalysed) { return; }
  if (type_sym.Convention != nullptr) { return; }
  if (not analyse::utils::drop_utils::NeedsDrop(type_sym, *sm, meta)) { return; }

  // The value is an ssa value rather than storage, and destroying one needs an address to work through, so it is
  // parked in a slot of its own first. The slot is reused across iterations, which is safe because the value it
  // holds is destroyed before the statement that produced it is reached again.
  const auto uid = "." + spp::utils::Uid();
  const auto slot = LlvmEntryAlloca(value->getType(), "drop.temp" + uid, ctx);
  ctx->Builder.CreateStore(value, slot);
  EmitDrop(type_sym, slot, sm, meta, ctx);
}

auto spp::codegen::EmitScopeDrops(
  analyse::scopes::Scope const &scope,
  analyse::scopes::VariableSymbol const *skip,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LlvmCtx *ctx)
  -> void {
  // A scope that already branched or returned has nothing
  // left to run: the drops belonging to that path were
  // emitted where it left, and anything added here would
  // be unreachable.
  const auto block = ctx->Builder.GetInsertBlock();
  if (block == nullptr or block->hasTerminator()) { return; }

  // Destroying what a scope owns depends on knowing what it still owns, and that is only known where the memory
  // analysis has been. A body it never reached has symbols reading as untouched - every local still initialized,
  // nothing ever moved - which is indistinguishable from a body that genuinely moves nothing, so the drops emitted
  // from it would destroy values the body had already handed away.
  //
  // Todo: this is not merely defensive. Closure bodies still land here unmarked (nine of them across the corpus), so
  //  everything a closure owns is leaked rather than destroyed. Marking the scope in
  //  "ClosureExpressionAst::Stage8_CheckMemory" does not reach them - the scope code generation names as the body's
  //  enclosing function scope is not the one either mark lands on. Removing this guard segfaults the compiled
  //  program, so the leak is the conservative half of a real bug, not a missing optimisation.
  if (meta->EnclosingFunctionScope != nullptr and not meta->EnclosingFunctionScope->BodyMemoryAnalysed) { return; }


  // Reverse declaration order. The symbol table yields its
  // entries in insertion order, which for locals is the
  // order they were declared in, so walking it backwards
  // destroys the most recently initialized value first.
  const auto syms = scope.AllVarSymbols(true);
  for (auto i = syms.Len(); i > 0uz; --i) {
    auto const *sym = syms[i - 1uz];
    if (sym == skip) { continue; }

    // A borrow does not own what it points at, and a
    // comptime constant is not a runtime value at all.
    if (sym->MemInfo == nullptr) { continue; }
    if (spp::get<0>(sym->MemInfo->AstBorrowed) != nullptr) { continue; }
    if (sym->MemInfo->AstCompTime != nullptr) { continue; }
    if (sym->IsGeneric) { continue; }

    // Only a local that still holds its value when the
    // scope ends is this scope's to destroy. One that
    // was moved out belongs to whoever it was moved to,
    // and one that was never initialized holds nothing.
    if (spp::get<0>(sym->MemInfo->AstInitialization) == nullptr and not NeedsDropFlag(*sym)) { continue; }

    // Partial moves leave some attributes owned and others
    // not; destroying the whole value would destroy an
    // attribute that has already been handed away.
    // Todo: destroy only the attributes that are still live,
    //  rather than skipping the value entirely.
    if (not sym->MemInfo->AstPartialMoves.IsEmpty()) { continue; }

    // Nothing was ever laid out for it (a parameter of a
    // declaration-only function, say).
    if (sym->LlvmInfo == nullptr or sym->LlvmInfo->Alloca == nullptr) { continue; }
    if (sym->Type == nullptr or sym->Type->GetConvention() != nullptr) { continue; }

    const auto type_sym = scope.GetTypeSymbol(sym->Type.get());
    if (type_sym == nullptr) { continue; }

    // A local the analyser could not settle statically carries a flag saying whether it is still here, so its
    // destruction becomes a branch rather than a decision made now.
    if (NeedsDropFlag(*sym) and sym->LlvmInfo->DropFlag != nullptr) {
      if (not analyse::utils::drop_utils::NeedsDrop(*type_sym, *sm, meta)) { continue; }
      const auto func = ctx->Builder.GetInsertBlock()->getParent();
      const auto drop_bb = llvm::BasicBlock::Create(*ctx->Context, "drop.live." + sym->Name->Val, func);
      const auto skip_bb = llvm::BasicBlock::Create(*ctx->Context, "drop.done." + sym->Name->Val, func);
      const auto live = ctx->Builder.CreateLoad(
        llvm::Type::getInt1Ty(*ctx->Context), sym->LlvmInfo->DropFlag, "drop.flag.load." + sym->Name->Val);
      ctx->Builder.CreateCondBr(live, drop_bb, skip_bb);

      ctx->Builder.SetInsertPoint(drop_bb);
      EmitDrop(*type_sym, sym->LlvmInfo->Alloca, sm, meta, ctx);
      ctx->Builder.CreateBr(skip_bb);
      ctx->Builder.SetInsertPoint(skip_bb);
      continue;
    }

    EmitDrop(*type_sym, sym->LlvmInfo->Alloca, sm, meta, ctx);
  }
}

auto spp::codegen::EmitUnwindDrops(
  analyse::scopes::Scope const &innermost,
  analyse::scopes::Scope const *boundary,
  const bool boundary_inclusive,
  analyse::scopes::VariableSymbol const *skip,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LlvmCtx *ctx)
  -> void {
  // Leaving early skips the scope exits that would otherwise
  // have run the drops, so every scope between here and the
  // one being jumped out of has to be destroyed at the jump
  // instead - innermost first, the order they would have been
  // left in had control reached their ends.
  for (auto const *scope = &innermost; scope != nullptr; scope = scope->Parent) {
    if (scope == boundary and not boundary_inclusive) { break; }
    EmitScopeDrops(*scope, skip, sm, meta, ctx);
    if (scope == boundary) { break; }
  }
}
