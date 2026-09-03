import spp.cli;
import std;
import std.compat;

#ifndef SPP_NO_MIMALLOC
import mimalloc;
#endif

auto main(const std::int32_t argc, char **argv) -> int {
#ifndef SPP_NO_MIMALLOC
  mi_option_disable(mi_option_show_stats);
  mi_option_disable(mi_option_verbose);
#endif

  // Temporary test code to test a small project.
  std::filesystem::current_path(
    std::filesystem::absolute(argv[0]).parent_path().parent_path() / "project");

  // Bare invocation runs the corpus, which is what this binary is mostly used for by hand. Anything else goes to the
  // cli, which requires a subcommand and would otherwise reject an empty argv. The mode matches what "spp run" itself
  // defaults to, so the two are the same thing.
  if (argc < 2) {
    spp::cli::handle_run("dev");
    return 0;
  }

  spp::cli::run_cli(argc, argv);
  return 0;
}
