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

  // The project to work in: whatever "--dir" names, and the sample project beside the binary otherwise - running
  // this straight out of a build tree, which is what pressing run in an ide does, has to land somewhere.
  auto project = std::filesystem::absolute(argv[0]).parent_path().parent_path() / "project";
  for (auto i = 1; i < argc - 1; ++i) {
    if (std::string_view(argv[i]) == "--dir") { project = argv[i + 1]; }
  }
  auto ec = std::error_code();
  std::filesystem::current_path(project, ec);
  if (ec) { std::cerr << "Error: cannot enter " << project << ": " << ec.message() << "\n"; return 1; }

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
