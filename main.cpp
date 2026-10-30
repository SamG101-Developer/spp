import spp.cli;
import std;
import std.compat;

/// Only import the mimalloc allocator if the CMake configuration has permitted
/// it; multiple CI jobs, such as sanitizing, cannot use mimalloc as it breaks
/// the analysis.
#if !defined(SPP_NO_MIMALLOC)
import mimalloc;
#endif

/// General boot function for the entire compiler, which invokes the CLI,
/// providing access to all the standard commands. Run with "spp build" etc. Set
/// up and configure mimalloc if it is enabled too.
auto main(const std::int32_t argc, char **argv) -> int {
#if !defined(SPP_NO_MIMALLOC)
  mi_option_disable(mi_option_show_stats);
  mi_option_disable(mi_option_verbose);
#endif

  return spp::cli::RunCli(argc, argv);
}
