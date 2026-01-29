import spp.cli;
import llvm;
import std;
import std.compat;

#include <execinfo.h>


auto print_stacktrace_for_sigsegv(void *user_data, const char *reason, bool gen_crash_diag) -> void {
    void *array[10];

    // get void*'s for all entries on the stack
    const std::size_t size = backtrace(array, 10);

    // print out all the frames to stderr
    std::printf("Error: signal SIGSEGV:\n");
    std::printf("Reason: %s\n", reason);
    std::printf("Backtrace:\n");
    backtrace_symbols_fd(array, static_cast<int>(size), 2);
}


int main(const std::int32_t, char **argv) {
    // Temporary test code to test a small project.
    llvm::install_fatal_error_handler(print_stacktrace_for_sigsegv);
    std::filesystem::current_path(std::filesystem::absolute(argv[0]).parent_path().parent_path() / "project");
    spp::cli::handle_build("dev");
    // spp::cli::run_cli(argc, argv);
    return 0;
}
