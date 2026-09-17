module spp.analyse.utils.instantiation_queue;

namespace spp::analyse::utils::instantiation_queue {
  namespace {
    /// The templates waiting, in the order they were recorded.
    auto _Pending = Vec<FunctionPrototypeAst*>();

    /// The index of the next template to be drained.
    auto _Cursor = 0uz;

    /// Membership of the unconsumed part of the pending templates.
    auto _Queued = Set<FunctionPrototypeAst*>();
  }
}

auto spp::analyse::utils::instantiation_queue::Enqueue(
  FunctionPrototypeAst *fn_template) -> void {
  // A template already waiting is left where it is: draining
  // one re-reads its substitution list from scratch, so a
  // single entry covers however many instantiations were
  // registered against it in the meantime.
  if (fn_template == nullptr) { return; }
  if (not _Queued.insert(fn_template).second) { return; }
  _Pending.EmplaceBack(fn_template);
}

auto spp::analyse::utils::instantiation_queue::Pop()
  -> FunctionPrototypeAst* {
  // Nothing left, so release what the drain accumulated. Doing
  // it here rather than leaving a spent cursor behind is what
  // lets a later compilation in the same process start from an
  // empty record.
  if (_Cursor >= _Pending.Len()) {
    Clear();
    return nullptr;
  }

  // Membership is dropped as the entry is handed out, not when
  // it was recorded, so a template instantiated again while it
  // is being drained is recorded afresh rather than silently
  // skipped.
  const auto fn_template = _Pending[_Cursor++];
  _Queued.erase(fn_template);
  return fn_template;
}

auto spp::analyse::utils::instantiation_queue::Clear() -> void {
  _Pending.Clear();
  _Queued.clear();
  _Cursor = 0uz;
}
