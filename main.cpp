import spp.cli;
import std;
import std.compat;

#if !defined(SPP_NO_MIMALLOC)
import mimalloc;
#endif

auto main(const std::int32_t argc, char **argv) -> int {
#if !defined(SPP_NO_MIMALLOC)
  mi_option_disable(mi_option_show_stats);
  mi_option_disable(mi_option_verbose);
#endif

  return spp::cli::run_cli(argc, argv);
}
