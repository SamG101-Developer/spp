module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_iterator;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeIterator);

/// The scope iterator is a special iterator that performs
/// depth-first traversal of the scope tree, starting from
/// the given root scope. It visits the root scope first,
/// then recursively visits each child scope in order. The
/// iterator is compatible with STL and Genex algorithms,
/// and live scope-tree updates.
SPP_EXP_CLS class spp::analyse::scopes::ScopeIterator {
  /// A frame contains a root, and the number of children seen,
  /// for each layer in the tree. This means when a list of
  /// children is exhausted, the next node up can bne checked
  /// for more siblings, instead of moving back to the mast root
  /// node.
  struct Frame {
    /// The root scope to start the traversal from in this frame.
    /// This is provided in the constructor. If the root is
    /// nullptr, then is the sentinel.
    Scope *Node;

    /// The number of chils scopes that have been seen so far.
    /// This is used to determine when to pop the stack and move
    /// back up the tree.
    std::size_t Seen;
  };

  /// The stack that the iterator maintains to keep track of
  /// the current scope and its ancestors. The top of the stack
  /// is the current scope.
  Vec<Frame> _Stack;

public:
  using iterator_category = std::forward_iterator_tag;
  using iterator_concept = std::forward_iterator_tag;
  using value_type = Scope*;
  using difference_type = std::ptrdiff_t;
  using pointer = Scope**;
  using reference = Scope*&;
  using const_pointer = Scope* const*;
  using const_reference = Scope* const&;

  explicit ScopeIterator(Scope *root = nullptr);
  auto operator*() -> reference;
  auto operator*() const -> const_reference;
  auto operator->() -> pointer;
  auto operator->() const -> const_pointer;
  auto operator++() -> ScopeIterator&;
  auto operator++(int) -> ScopeIterator;
  auto operator==(ScopeIterator const &other) const -> bool;
  auto operator!=(ScopeIterator const &other) const -> bool;
};
