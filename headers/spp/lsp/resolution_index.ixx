module;
#include <spp/macros.hpp>

export module spp.lsp.resolution_index;
import spp.utils.error_formatter;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct Ast);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct ObjectInitializerArgumentKeywordAst);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::lsp::resolution_index, struct CompTimeValue);
use(spp::lsp::resolution_index, struct Member);
use(spp::lsp::resolution_index, struct MemberList);
use(spp::lsp::resolution_index, struct NamesInScope);
use(spp::lsp::resolution_index, struct ResolvedName);
use(spp::lsp::resolution_index, struct Signature);
use(spp::utils::errors, struct SourceSpan);

/// One name in the source and what it turned out to mean:
/// where it is written, what kind of thing it named, what
/// it is called, the type it has (for a value), and where
/// that thing was declared. It is the answer to the two
/// questions an editor asks about the symbol under the
/// cursor - what is this, and where does it come from.
SPP_EXP_CLS struct spp::lsp::resolution_index::ResolvedName {
  SourceSpan Use;
  Str Kind;
  Str Name;
  Str Type;
  SourceSpan Definition;

  /// What a compile-time constant worked out to, for the
  /// names that are one. Empty for everything else.
  Str Value = {};
};

/// One thing reachable through a "." or a "::": a method, an
/// attribute, a constant, or - on a namespace - a function
/// or a namespace under it.
SPP_EXP_CLS struct spp::lsp::resolution_index::Member {
  Str Name;
  Str Kind;
  Str Type;
  SourceSpan Definition;

  /// What a function member can be called as, one per overload.
  /// This is because otherwise we just get the mock $Type for
  /// functions.
  Vec<Str> Signatures = {};
};

/// Everything reachable through one type or one namespace,
/// which is what a completion list after "." or "::" is.
SPP_EXP_CLS struct spp::lsp::resolution_index::MemberList {
  Str Owner;
  Str Of;
  Vec<Member> Members;
};

/// The parameters a call resolved to, and where in the source
/// that call's arguments are written. It is what an editor
/// offers when the caret sits between the brackets, and what
/// it shows while the arguments are being typed.
SPP_EXP_CLS struct spp::lsp::resolution_index::Signature {
  SourceSpan Arguments;
  Str Name;
  Vec<Member> Params;
};

/// The names one scope holds and the part of the file it
/// covers. A caret sits inside several of these at once -
/// a block inside a function inside a module - and what
/// can be written there is what they hold between them.
SPP_EXP_CLS struct spp::lsp::resolution_index::NamesInScope {
  SourceSpan Where;
  Vec<Member> Names;
};

/// What a "cmp" declaration worked out to: the constant is
/// computed while the program is compiled, so the answer
/// is known and worth showing beside the declaration that
/// asked for it.
SPP_EXP_CLS struct spp::lsp::resolution_index::CompTimeValue {
  SourceSpan Where;
  Str Name;
  Str Value;
};

/// Where a name's meaning is recorded as analysis works it
/// out. Nothing is recorded unless an editor asked for it,
/// and then only for the one file it asked about: a name is
/// resolved thousands of times in a project, and all but a
/// handful of those are in files nobody is looking at.
namespace spp::lsp::resolution_index {
  /// Record the uses written in these files, and no others.
  SPP_EXP_FUN auto EnableFiles(Vec<Str> files) -> void;

  /// Record the uses written anywhere in the project's own
  /// code. One compile analyses every module there is, so
  /// this costs a compile no more than indexing a single
  /// file does, and answers for every file at once rather
  /// than making the next file asked about pay for another
  /// whole compile. Dependencies are not included.
  SPP_EXP_FUN auto EnableProject() -> void;

  /// Record nothing, which is the default.
  SPP_EXP_FUN auto Disable() -> void;

  /// Whether anything is being recorded. Checked before the
  /// work of building an entry is done.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto IsEnabled() -> bool;

  /// Whether uses written in this file are being kept. Asked
  /// before a use is described, because describing one means
  /// locating it in its module, and most of the names a compile
  /// resolves are in files nobody asked about.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto Wants(Str const &file) -> bool;

  /// Whether anything written in this scope's module is being
  /// kept. Cheap, and worth asking before doing anything that
  /// describes a use - inferring a type, above all.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto WantsScope(Scope const &scope) -> bool;

  /// Record a use of a variable, constant, parameter or method,
  /// if there is one: the caller has just looked the symbol up
  /// and a null one is the ordinary answer. Called from postfix
  /// member access (static + runtime) analysis.
  SPP_EXP_FUN auto RecordVariable(
    Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta, VariableSymbol const *sym_ptr) -> void;

  /// Record a use of a type, if there is one. Called from type
  /// identifier analysis.
  SPP_EXP_FUN auto RecordType(
    Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta, TypeSymbol const *sym_ptr) -> void;

  /// Record what a written name turned out to be: the variable
  /// it names, or (when nothing of that name is a variable) the
  /// namespace it opens. Called from identifier analysis.
  SPP_EXP_FUN auto RecordIdentifier(
    Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta, VariableSymbol const *sym) -> void;

  /// Record the type an expression has, so that what follows a "."
  /// on it can be offered: a call or a chain of accesses is not a
  /// name, and has nothing else that says what it produced. Postfix
  /// runtime member access with a non-symbolic left-hand-side.
  SPP_EXP_FUN auto RecordExpression(
    Ast const &use, ScopeManager const &sm, CompilerMetaData const &meta,
    std::function<Shared<TypeAst>()> const &infer) -> void;

  /// Record a name reached through "::" on a namespace, when
  /// what it names is another namespace rather than something
  /// declared in one. The namespace equivalent of "record
  /// variable", also called from postfix static member analysis.
  SPP_EXP_FUN auto RecordNamespaceMember(
    IdentifierAst const &use, ScopeManager const &sm, CompilerMetaData const &meta, Scope const &ns_scope) -> void;

  /// Record which attribute each of an object initializer's
  /// named arguments names. This enabled completion of fields
  /// for an object initialization.
  SPP_EXP_FUN auto RecordObjectInitializerArguments(
    Vec<ObjectInitializerArgumentKeywordAst*> const &args,
    Vec<Tup<Shared<IdentifierAst>, TypeRef, Scope*>> const &attrs,
    ScopeManager const &sm, CompilerMetaData const &meta) -> void;

  /// Record what a call resolved to: the parameters its brackets
  /// are filled with, and which parameter each of the names
  /// written in it refers to. Must be called before the matched
  /// arguments replace the written ones, because matching names
  /// every argument after the parameter it filled and places that
  /// name where the argument was written - so afterwards a name
  /// nobody typed cannot be told from one somebody did.
  SPP_EXP_FUN auto RecordFnCallArguments(
    FunctionCallArgumentGroupAst const &arg_group, ScopeManager const &sm, CompilerMetaData const &meta,
    FunctionPrototypeAst const &proto, Scope const &overload_scope) -> void;

  /// Record what a "cmp" declaration computed. Called once the
  /// value has been worked out, which is the only point at
  /// which the compiler itself knows it.
  SPP_EXP_FUN auto RecordCompTimeValue(
    Ast const &name, ScopeManager const &sm, CompilerMetaData const &meta, Str value) -> void;

  /// Record what can be written inside @p node , which is the
  /// scope the analysis is currently in. Called from the nodes
  /// that make scopes rather than by walking the scope tree: a
  /// scope keeps a pointer to the node that made it, and some
  /// of those nodes are temporaries that analysis has since let
  /// go of.
  SPP_EXP_FUN auto RecordScopeOf(
    Ast const &node, ScopeManager const &sm, bool whole_file = false) -> void;

  /// Everything recorded so far.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto GetAllEntries() -> Vec<ResolvedName> const&;

  /// Code completion: what each type and namespace reached from
  /// the indexed files holds, which is what an editor offers
  /// after a "." or a "::". Gathered as the names are recorded
  /// rather than afterwards, because the scopes it reads are
  /// gone by the time a compile has finished.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto GetAllMembers() -> Vec<MemberList> const&;

  /// Every "cmp" declaration in the indexed files, and what it
  /// computed.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto GetAllComptimeValues() -> Vec<CompTimeValue> const&;

  /// Every call in the indexed files, with the parameters it
  /// resolved to.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto GetAllSignatures() -> Vec<Signature> const&;

  /// What each part of the indexed files can name.
  SPP_EXP_FUN SPP_ATTR_NODISCARD auto GetAllScopes() -> Vec<NamesInScope> const&;

  /// Forget everything, for the next compile in this process.
  SPP_EXP_FUN auto Clear() -> void;
}
