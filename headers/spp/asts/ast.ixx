module;
#include <spp/macros.hpp>

export module spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.ast_kind_range;
import spp.asts.mixins.compiler_stages;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(Ast);
use(spp::analyse::scopes, class Scope);

GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT
use(spp::asts, struct TokenAst);
use(spp::asts::meta, struct CompilerMetaData);

/// The base ast for all ast nodes in the tree, using the
/// compiler stages as the method stage basis. Provides the
/// token position, clone, casting, stringification, etc.
GCC_BUGZILLA_127341_VTABLE_TYPEINFO_MISSING
SPP_EXP_CLS struct spp::asts::Ast : mixins::CompilerStages {
  SPP_GCC_VTABLE_FIX_BASE;
  ~Ast() override;

  /// The starting position in the source code that this ast
  /// covers. Typically recurses to the first field's start
  /// position, until we get to a token, identifier or type
  /// identifier ast, that store a raw position.
  SPP_ATTR_NODISCARD virtual auto PosStart() const -> std::size_t = 0;

  /// The ending position in the source code that this ast
  /// covers. Typically recurses to the last field's end
  /// position, until we get to a token, identifier or type
  /// identifier ast, that store a raw position and size.
  SPP_ATTR_NODISCARD virtual auto PosEnd() const -> std::size_t = 0;

  /// The size of an ast - usually the difference between the
  /// pos end and start - how many tokens the error
  /// formatter will span.
  SPP_ATTR_NODISCARD auto Size() const -> std::size_t;

  /// The customisable clone method that clones all the fields
  /// from an ast into the new ast. Usually internal flags are
  /// copied over too.
  SPP_ATTR_NODISCARD virtual auto Clone() const
    -> Unique<Ast> = 0;

  /// Convert an ast into a string, for debugging purposes or
  /// writing translated asts out to a file. Not critically
  /// needed, and a macro suite is available to help.
  SPP_ATTR_NODISCARD virtual auto ToString() const -> Str = 0;

  /// An overridable hashing mechanism, implemented over the
  /// identifier and type identifier asts.
  SPP_ATTR_NODISCARD virtual auto AnkerlHash() const -> std::size_t;

  /// Whether the ast can appear in a runtime default context
  /// for a function parameter or a class attribute. A default
  /// is computed and copied into every call or object
  /// initializer that omits a value for it. Defaults to being
  /// not allowed, and compatible asts opt in, potentially
  /// conditionally (elements inside an array for example).
  SPP_ATTR_NODISCARD virtual auto IsAllowedInDefault() const -> bool;

  /// Which concrete ast class this node is. This is used to
  /// bypass RTTI dynamic casting checks; see "To", and "AstAs".
  SPP_ATTR_NODISCARD virtual auto Kind() const noexcept -> AstKind = 0;

  /// The RTTI-bypassed casting check, to convert a node into
  /// a derived type if possible, and otherwise nullptr. Uses
  /// the "Kind" and a static cast, with a rare dynamic cast
  /// fallback for cross casting with mixins.
  template <typename T>
  auto To() -> T* {
    if constexpr (AstKindRange<T>::Known) {
      auto *self = this;
      SPP_OPAQUE_PTR(self);
      if (self == nullptr) { return nullptr; }

      const auto kind = self->Kind();
      return kind >= AstKindRange<T>::First and kind <= AstKindRange<T>::Last
        ? static_cast<T*>(self)
        : nullptr;
    }
    else {
      return dynamic_cast<T*>(this);
    }
  }

  /// A const pointer version of the normal "To" cast conversion,
  /// just adding the "const" tag to each pointer being cast
  /// through.
  template <typename T>
  auto To() const -> T const* {
    if constexpr (AstKindRange<T>::Known) {
      auto *self = this;
      SPP_OPAQUE_PTR(self);
      if (self == nullptr) { return nullptr; }

      const auto kind = self->Kind();
      return kind >= AstKindRange<T>::First and kind <= AstKindRange<T>::Last
        ? static_cast<T const*>(self)
        : nullptr;
    }
    else {
      return dynamic_cast<T const*>(this);
    }
  }

  /// The unchecked cast when we know 100% that a cast will be
  /// valid, bypassing the "Kind" and RTTI casting. Barely
  /// cheaper than the "Kind" cast but still more optimal.
  template <typename T>
  auto ToUnchecked() -> T* {
    return static_cast<T*>(this);
  }

  /// The const version of the unchecked cast, just adding the
  /// "const" tag to the pointers.
  template <typename T>
  auto ToUnchecked() const -> T const* {
    return static_cast<T const*>(this);
  }

  /// The default behaviour is to bind the context into this
  /// ast's "_Ctx" field. This is then used in future compiler
  /// stage steps.
  auto Stage1_PreProcess(Ast *ctx) -> void override;

  /// The default behaviour is to bind the scope into this
  /// ast's "_Scope" field. This is then used in future compiler
  /// stage steps.
  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Getter over the internal context from the stage 1 pass.
  SPP_ATTR_NODISCARD auto GetAstCtx() const -> Ast*;

  /// Getter over the internal scope from the stage 2 pass.
  SPP_ATTR_NODISCARD auto GetAstScope() const -> Scope*;

  /// Setter for the internal context from the stage 1 pass.
  auto SetAstCtx(Ast *ctx) -> void;

  /// Setter for the internal scope from the stage 2 pass.
  auto SetAstScope(Scope *scope) -> void;

protected:
  /// The internal context from stage 1.
  Ast *_Ctx = nullptr;

  /// The internal context from stage 2.
  Scope *_Scope = nullptr;

  /// Creating a raw Ast isn't allowed, but to be created from
  /// its base classes, the constructor is marked as "protected".
  explicit Ast();
};

namespace spp::asts {
  /// A null-safe version of "To", checking if the inputted
  /// pointer is nullptr before before trying to use the "To"
  /// cast. Only needed when we are potentially considering
  /// a nullptr ast.
  SPP_EXP_FUN template <typename T, typename U>
  auto AstAs(U *const ast) -> decltype(ast->template To<T>()) {
    return ast != nullptr ? ast->template To<T>() : nullptr;
  }
}

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::Ast)
