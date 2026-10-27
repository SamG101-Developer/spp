module;
#include <spp/macros.hpp>

module spp.codegen.llvm_drop;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_fn;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.utils.ice;
import spp.utils.interner;
import spp.utils.uid;
import genex;

namespace spp::codegen {
  namespace {
    /// "EmitDrop", for a value whose parts on the paths in "moved"
    /// (field names, outermost first) have been taken out of it: only
    /// what is left is destroyed, part by part. A value with its own
    /// destructor is never partly moved here (E106).
    auto DropValue(
      analyse::scopes::TypeRef const &type,
      llvm::Value *ptr,
      Vec<Vec<spp::utils::InternedId>> const &moved,
      analyse::scopes::ScopeManager *sm,
      asts::meta::CompilerMetaData *meta,
      LlvmCtx *ctx)
      -> void {
      //
      using analyse::utils::drop_utils::FindDropOverload;
      using analyse::utils::drop_utils::NeedsDrop;
      using analyse::utils::type_members::GetAllParts;
      using analyse::utils::type_predicates::IsTypeArray;
      using analyse::utils::type_predicates::IsTypeCompTimeIndexable;

      // Destroying a value that owns nothing is a no-op;
      // an "S32" local, or a struct built only from them,
      // leaves no instructions behind at all.
      if (not NeedsDrop(type, *sm, meta)) { return; }
      auto const &type_sym = *type.Symbol;

      // Moved out whole: nothing of it is left here.
      if (genex::any_of(moved, [](auto const &path) { return path.empty(); })) { return; }

      const auto uid = "." + spp::utils::Uid();

      // A generator is a bare coroutine handle, not a struct: destroying it means destroying the frame it refers
      // to. Nothing else frees that frame - a coroutine that ran to completion is parked on its final suspend and
      // one abandoned part-way is parked mid-body, and both hold their frame until someone destroys them. The
      // values the frame still owns are dropped by its own destroy edge, at the "gen" it is parked on.
      if (moved.empty() and analyse::utils::type_predicates::IsTypeGenerator(type, *sm->CurrentScope)) {
        const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
        const auto handle = ctx->Builder.CreateLoad(ptr_ty, ptr, "drop.gen.handle" + uid);

        // A handle is null only if it was never assigned one; destroying that would fault.
        const auto func = ctx->Builder.GetInsertBlock()->getParent();
        const auto live_bb = llvm::BasicBlock::Create(*ctx->Context, "drop.gen.live" + uid, func);
        const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "drop.gen.done" + uid, func);
        ctx->Builder.CreateCondBr(ctx->Builder.CreateIsNotNull(handle, "drop.gen.ok" + uid), live_bb, done_bb);

        ctx->Builder.SetInsertPoint(live_bb);
        const auto coro_destroy = llvm::Intrinsic::getOrInsertDeclaration(
          GetEmissionModule(*ctx), llvm::Intrinsic::coro_destroy);
        ctx->Builder.CreateCall(
          coro_destroy, {handle}, CfiGenCallBundles(type, CoroEdge::Destroy, ctx));
        ctx->Builder.CreateBr(done_bb);
        ctx->Builder.SetInsertPoint(done_bb);
        return;
      }

      // The type's own destructor runs before its attributes
      // are destroyed, because it is still free to read them.
      if (const auto drop_proto = FindDropOverload(type_sym, *sm, meta); drop_proto != nullptr) {
        if (not moved.empty()) { spp::Ice("a value with its own destructor is destroyed with a part missing"); }

        // A "drop" reached only through this path may not have
        // been declared yet, because nothing in the source
        // ever called it. Declare it into the module that owns
        // its definition, exactly as a written call site would.
        // Declared from its own scope, where its signature's names ("self: Self", a substitution's bindings) resolve.
        if (drop_proto->GetLlvmFn() == nullptr) {
          const auto owner_ctx = drop_proto->OwnerCtx();
          auto tm = sm->NewManagerAt(drop_proto->GetDeclarationScope());
          drop_proto->GenerateLlvmDeclaration(&tm, meta, owner_ctx != nullptr ? owner_ctx : ctx);
        }

        if (drop_proto->GetLlvmFn() != nullptr) {
          const auto drop_func = GetOrAddTargetIntoCurrentModule(
            *drop_proto->GetLlvmFn()->Target, *GetEmissionModule(*ctx));

          // Destroying a value consumes it, so "drop" takes "self"
          // by move, which lowers to the value itself rather than
          // to a pointer to it. The value is loaded out of the
          // storage this is destroying through, as what "drop"
          // takes: its own "self" parameter's type, which a "self"
          // by move always has. "type_sym" may be a bound generic
          // parameter, which names no layout of its own.
          const auto self_ty = drop_proto->GetLlvmFn()->Target->getFunctionType()->getParamType(0);
          const auto self_val = ctx->Builder.CreateLoad(self_ty, ptr, "drop.self" + uid);
          ctx->Builder.CreateCall(drop_func, {self_val});
        }
        return;
      }

      const auto elem_ty = GetLlvmType(type_sym, ctx);

      // A bound generic parameter stands for its argument: the symbol keeps the parameter's name ("T"), which is not a
      // name the checks below can read a tuple or an array off. "NeedsDrop" resolves through for the same reason.
      auto const *const bare_sym = type_sym.AsBound();

      // Only a type that has no destructor of its own is destroyed part by part - its elements when it is a tuple or an
      // array, its attributes otherwise. A struct's parts are reached through its lowered form, which has to be one.
      const auto is_indexable = IsTypeCompTimeIndexable(type, *sm->CurrentScope);
      const auto is_arr = is_indexable and IsTypeArray(type, *sm->CurrentScope);
      if (not is_indexable and not llvm::isa<llvm::StructType>(elem_ty)) { return; }

      // Each part is read where it was found ("TypePart::Where").
      const auto parts = GetAllParts(type, *sm->CurrentScope);
      const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

      // Reverse order: the last part built is the first one
      // destroyed, mirroring the order they were initialized in.
      for (auto i = parts.size(); i > 0uz; --i) {
        auto const &part = parts[i - 1uz];
        if (part.Ref.Symbol == &type_sym or part.Ref.Symbol == bare_sym) { continue; }
        if (not NeedsDrop(part.Ref, *sm, meta)) { continue; }

        // What was moved out of this part, as paths from it.
        auto part_moved = Vec<Vec<spp::utils::InternedId>>();
        for (auto const &path : moved) {
          if (path.front() == part.Step->NameId()) { part_moved.emplace_back(path.begin() + 1, path.end()); }
        }

        // An array is one value repeated, so its elements are reached by indexing into it; a tuple's and a struct's
        // are separate fields. The S++ layout re-orders a struct's fields to minimize padding, so a declaration index
        // has to be mapped through to wherever the field actually ended up.
        const auto index = static_cast<std::uint32_t>(part.Index);
        const auto part_ptr = is_arr
          ? ctx->Builder.CreateGEP(
            elem_ty, ptr, {llvm::ConstantInt::get(i32_ty, 0), llvm::ConstantInt::get(i32_ty, index)},
            "drop.elem" + uid + "." + part.Step->Val)
          : is_indexable
            ? ctx->Builder.CreateStructGEP(elem_ty, ptr, index, "drop.elem" + uid + "." + part.Step->Val)
            : ctx->Builder.CreateStructGEP(
              elem_ty, ptr, GetPhysicalFieldIndex(*type_sym.LlvmInfo, part.Index),
              "drop.field" + uid + "." + part.Step->Val);
        DropValue(part.Ref, part_ptr, part_moved, sm, meta, ctx);
      }
    }
  }
}

auto spp::codegen::EmitDrop(
  analyse::scopes::TypeRef const &type,
  llvm::Value *ptr,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LlvmCtx *ctx)
  -> void {
  DropValue(type, ptr, {}, sm, meta, ctx);
}

auto spp::codegen::EmitDropRemaining(
  analyse::scopes::TypeRef const &type,
  llvm::Value *ptr,
  Vec<Vec<spp::utils::InternedId>> const &moved,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LlvmCtx *ctx)
  -> void {
  DropValue(type, ptr, moved, sm, meta, ctx);
}
