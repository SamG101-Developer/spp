module;
#include <spp/macros.hpp>

export module spp.compiler.compiler;
import spp.compiler.module_tree;
import spp.compiler.out_layout;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct ScopeTree;
}

namespace spp::compiler {
  SPP_EXP_CLS class Compiler;
  SPP_EXP_CLS struct CompilerBoot;
}

inline constexpr auto kCompilerStageNames = spp::Arr{
  "Lexing......................... ",
  "Parsing........................ ",
  "Pre-processing................. ",
  "Generating Top-Level Scopes.... ",
  "Generating Top-Level Aliases... ",
  "Qualifying Types............... ",
  "Loading Super Scopes........... ",
  "Attaching Super Scopes......... ",
  "Pre-Analysing Semantics........ ",
  "Analysing Semantics............ ",
  "Checking Memory Safety......... ",
  "Monomorphising generics........ ",
  "Generating Code (1)............ ",
  "Generating Code (2)............ "
};

/// The entry to the entire compilation process, which generates the module
/// tree, considers any test setups, and creates the compiler boot, which drives
/// the stages of the compiler across the modules.
SPP_EXP_CLS class spp::compiler::Compiler {
  /// The entire module tree, listing every module that will be compiled from
  /// the source folder and the vcs code.
  Unique<ModuleTree> _Modules;

  /// The build mode, either dev or rel. This controls the optimization level
  /// that LLVM uses, and if debug information is generated. Todo: Debug info.
  BuildMode _Mode = BuildMode::DEV;

  /// The build type, either a library or executable. Executables need an
  /// entrance point (main.spp -> "main").
  BuildType _BuildType = BuildType::EXE;

  /// The compiler boot instance which drives each stage across all the modules.
  /// Also handles progress bars / other visuals.
  Unique<CompilerBoot> _Boot;

  /// The compile's scopes; declared before the manager over them, so it
  /// outlives it.
  Unique<analyse::scopes::ScopeTree> _ScopeTree;

  /// The root scope manager whose tree starts at the global scope, and creates
  /// namespace scopes for the package and vcs/ffi, storing a scope per
  /// namespace, function, class, etc.
  Unique<analyse::scopes::ScopeManager> _ScopeManager;

  /// A flag for if we are running the c++ google test suite. This is just used
  /// to disable progress bars being printed (messes up the test suite outputs).
  bool _ForCppGoogleTest = false;

  /// Stop once the analysis is done, leaving monomorphisation, pre-codegen and
  /// codegen unrun. An editor doesn't need anything past the memory analysis as
  /// this is the last analysis stage (no more semantic errors afterwards).
  bool _AnalyseOnly = false;

  /// The names of the unit tests the generated harness runs. Read off the boot
  /// once parsing has written the harness, and kept past "Cleanup" so the
  /// driver can re-run them one at a time.
  Vec<Str> _TestNames;

  /// The compile-time constants the main module declared, and the values they
  /// resolved to, rendered as the source text of the literal each one became.
  /// These are needed for the unit tests that test the comptime engine.
  Map<Str, Str> _CompTimeConstants;

  /// Read the resolved value of every "cmp" the main module declares off its
  /// scope, into "_CompTimeConstants".
  auto CollectCompTimeConstants() -> void;

  /// An empty compiler, filled in by "ForCppGoogleTest".
  Compiler() = default;

public:
  /// The public facing constructor the compiler instance, which receives the
  /// build mode, build type, and a possible test scope for running certain s++
  /// tests instead of the executable.
  explicit Compiler(
    BuildMode mode, BuildType build_type, TestScope const &tests = {});

  /// The static constructor for when we invoke the c++ unit test suite. It uses
  /// a custom module tree constructor internally, focussing the tree onto the
  /// unit tests.
  static auto ForCppGoogleTest(
    BuildMode mode, Str &&main_code) -> Unique<Compiler>;

  /// The compiler instance's destructor is responsible for clearing a group of
  /// static vectors and maps that maintain compilation-wide information. Not
  /// clearing these cause massive memory leaks for multiple runs of the same
  /// compiler (like how the c++ unit test suite works).
  ~Compiler();

  /// Run every stage over the module tree. Analysis reports what it rejects by
  /// throwing, so a build that gets as far as the back end can still fail there
  /// with nothing thrown. Return true when the build produced its artefact.
  SPP_ATTR_NODISCARD auto Compile() -> bool;

  /// Restrict which unit tests the generated harness runs. Must be set before
  /// "Compile", because the filters are applied while the harness is written
  /// rather than when it runs.
  auto SetTestFilters(Str name_filter, Str group_filter) const -> void;

  /// Stop after stage 8 rather than going on to generate anything. Must be set
  /// before "Compile". Nothing is written to the output tree in this mode, so
  /// the artefacts of a real build are left as they were.
  auto SetAnalyseOnly(bool analyse_only) -> void;

  /// How many unit tests the generated harness ended up running. Valid once
  /// "Compile" has run.
  SPP_ATTR_NODISCARD auto TestCount() const -> std::size_t;

  /// The fully qualified name of each test the harness runs. Valid once
  /// "Compile" has run.
  SPP_ATTR_NODISCARD auto TestNames() const -> Vec<Str> const&;

  /// The values the main module's compile-time constants resolved to, by name.
  /// This is the only way to observe what comp-time resolution computed.
  SPP_ATTR_NODISCARD auto CompTimeConstants() const -> Map<Str, Str> const&;

  static auto Cleanup() -> void;
};
