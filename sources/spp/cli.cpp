module;
#include <spp/macros.hpp>

#define SPP_VALIDATE_STRUCTURE(is_exe) \
    if (not handle_validate(is_exe)) { return; }

#define SPP_CLI_NULL \
    bp::v1::std_out > bp::v1::null

module spp.cli;
import spp.analyse.scopes.scope_manager;
import spp.asts.module_prototype_ast;
import spp.compiler.compiler;
import spp.compiler.compiler_boot;
import spp.compiler.module_tree;
import spp.lex.tokens;
import spp.utils.files;
import cli11;
import genex;
import tomlpp;

inline constexpr spp::Str OUT_FOLDER = "out";
inline constexpr spp::Str SRC_FOLDER = "src";
inline constexpr spp::Str VCS_FOLDER = "vcs";
inline constexpr spp::Str FFI_FOLDER = "ffi";
inline constexpr spp::Str MAIN_FILE = "main.spp";
inline constexpr spp::Str CONFIG_FILE = "spp.toml";

inline const spp::Str MAIN_FILE_CONTENTS = R"(
    fun main(args: Vec[Str]) -> Void {
        std::io::println("Hello world!")
    })";

inline const spp::Str CONFIG_FILE_CONTENTS = R"(
    [project]
    name = "$"
    version = "0.1.0"
    build = "exe"

    [vcs]
    std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "master" })";

auto spp::cli::run_cli(
    const std::int32_t argc,
    char **argv)
    -> std::int32_t {
    // Create the CLI object and require that a subcommand is provided.
    auto app = CLI::App("SPP build tool", "spp");
    app.require_subcommand(1);
    auto mode = Str();

    app.add_subcommand("init", "Initialize the new project")
       ->callback(handle_init);

    app.add_subcommand("vcs", "Initialize version control for the project")
       ->callback(handle_vcs);

    app.add_subcommand("build", "Build the project")
       ->callback([&mode] { handle_build(mode); })
       ->add_option("-m,--mode", mode, "Build mode (dev or rel)")
       ->check(CLI::IsMember({"dev", "rel"}))
       ->default_val("dev");

    app.add_subcommand("run", "Run the project")
       ->callback([&mode] { handle_run(mode); })
       ->add_option("-m,--mode", mode, "Run mode (dev or rel)")
       ->check(CLI::IsMember({"dev", "rel"}))
       ->default_val("dev");

    app.add_subcommand("clean", "Clean the project")
       ->callback([&mode] { handle_clean(mode); })
       ->add_option("-m,--mode", mode, "Clean mode (dev, rel or all)")
       ->check(CLI::IsMember({"dev", "rel", "all"}))
       ->default_val("all");

    app.add_subcommand("test", "Test the project")
       ->callback(handle_test);

    app.add_subcommand("validate", "Validate the project")
       ->callback([] { handle_validate(false); });

    app.add_subcommand("version", "Show version information")
       ->callback(handle_version);

    // Parse the command line arguments.
    app.parse(argc, argv);
    return 0;
}

auto spp::cli::handle_init()
    -> void {
    // Check if the current directory is empty or not.
    const auto cwd = std::filesystem::current_path();
    if (not std::filesystem::is_empty(cwd)) {
        std::cerr << "Error: The current directory is not empty. Please run this command in an empty directory.\n";
        return;
    }

    // Create the directory structure (folders).
    std::filesystem::create_directory(cwd / OUT_FOLDER);
    std::filesystem::create_directory(cwd / SRC_FOLDER);
    std::filesystem::create_directory(cwd / VCS_FOLDER);
    std::filesystem::create_directory(cwd / FFI_FOLDER);

    // Add the key files into the directory structure.
    std::filesystem::create_directory(cwd / SRC_FOLDER / cwd.filename());

    // Fill in "main.spp" and "spp.toml" with template content.
    utils::files::WriteFile(cwd / SRC_FOLDER / MAIN_FILE, format_default_file_contents(MAIN_FILE_CONTENTS));
    utils::files::WriteFile(cwd / CONFIG_FILE, create_default_config_for(utils::files::DisplayString(cwd.filename())));
}

auto spp::cli::handle_vcs()
    -> void {
    // Validate the project structure first.
    using namespace std::string_literals;
    SPP_VALIDATE_STRUCTURE(false);

    // Parse the spp.toml config file and get the optional "vcs" section.
    const auto toml = toml::parse_file(CONFIG_FILE);
    if (not toml.contains("vcs")) {
        std::cout << "Error: No [vcs] section found in spp.toml.\n";
        return;
    }

    // Move into the VCS folder.
    const auto cwd = std::filesystem::current_path();
    std::filesystem::current_path(cwd / VCS_FOLDER);

    // Iterate over the vcs section and clone/update the repositories.
    auto vcs = toml["vcs"].as_table();
    for (auto [key, info] : *vcs) {
        auto repo_name = Str(key);
        auto repo_url = (*info.as_table())["git"].value<Str>().value();
        auto repo_branch = (*info.as_table())["branch"].value<Str>().value_or("master");
        auto repo_folder = cwd / VCS_FOLDER / repo_name;

        // Repo doesn't exist locally => clone it.
        if (not std::filesystem::exists(repo_folder)) {
            std::system(("git clone --branch " + repo_branch + " " + repo_url + " " + utils::files::NativeString(repo_folder)).c_str());
            std::cout << "Cloned "s + repo_name + " from " + repo_url + "\n";
        }
        else {
            std::system(("git -C " + utils::files::NativeString(repo_folder) + " pull origin " + repo_branch).c_str());
            std::system(("git -C " + utils::files::NativeString(repo_folder) + " checkout " + repo_branch).c_str());
            std::cout << "Updated "s + repo_name + " from " + repo_url + " (" + repo_branch + ")" + "\n";
        }

        // Copy all DLLs from the VCS's FFI folder into this project's FFI folder.
        auto ffi_repo_folder = repo_folder / FFI_FOLDER;
        if (std::filesystem::exists(ffi_repo_folder)) {
            for (auto const &entry : std::filesystem::directory_iterator(ffi_repo_folder)) {
                std::filesystem::copy(
                    entry.path(), cwd / FFI_FOLDER / entry.path().filename(),
                    std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
            }
        }
    }

    // Move back into the original working directory.
    std::filesystem::current_path(cwd);
}

auto spp::cli::handle_build(
    Str const &mode,
    const bool skip_vcs)
    -> void {
    // Validate the project structure first.
    SPP_VALIDATE_STRUCTURE(false);

    // Create the inner directory (rel or dev).
    const auto cwd = std::filesystem::current_path();
    std::filesystem::create_directory(cwd / OUT_FOLDER / mode);

    // Handle VCS if not skipped.
    if (not skip_vcs) { handle_vcs(); }

    // Revalidate (after including the VCS folders).
    SPP_VALIDATE_STRUCTURE(false);
    const auto build_type =
        toml::parse_file(CONFIG_FILE)["project"].as_table()->at("build").value<Str>();

    // Validate the mode is "dev" or "rel".
    if (mode != "dev" and mode != "rel") {
        std::cerr << "Error: Invalid mode. Mode must be 'dev' or 'rel'.\n";
        return;
    }

    // Compile the code.
    auto c = compiler::Compiler(
        mode == "dev" ? compiler::Compiler::Mode::DEV : compiler::Compiler::Mode::REL,
        build_type == "exe" ? compiler::Compiler::BuildType::EXE : compiler::Compiler::BuildType::LIB);
    c.Compile();
}

auto spp::cli::handle_run(
    Str const &mode)
    -> void {
    // Build the project first (skip VCS).
    handle_build(mode, true);

    // Run the executable.
    // TODO
}

auto spp::cli::handle_clean(
    Str const &mode)
    -> void {
    // Validate the project structure first.
    SPP_VALIDATE_STRUCTURE(false);

    // Remove the appropriate folders.
    const auto cwd = std::filesystem::current_path();
    if (mode == "dev" or mode == "all") {
        std::filesystem::remove_all(cwd / OUT_FOLDER / "dev");
    }
    if (mode == "rel" or mode == "all") {
        std::filesystem::remove_all(cwd / OUT_FOLDER / "rel");
    }
}

auto spp::cli::handle_test()
    -> void {
    // Validate the project structure first.
    SPP_VALIDATE_STRUCTURE(false);

    // TODO
}

auto spp::cli::handle_validate(
    const bool is_exe)
    -> bool {
    using namespace std::string_literals;

    // Check for the key folders/files.
    const auto cwd = std::filesystem::current_path();
    if (not std::filesystem::exists(cwd / SRC_FOLDER)) {
        std::cerr << "Error: Missing 'src' folder.\n";
        return false;
    }
    if (not std::filesystem::exists(cwd / SRC_FOLDER / MAIN_FILE) and is_exe) {
        std::cerr << "Error: Missing 'src/main.spp' file.\n";
        return false;
    }
    if (not std::filesystem::exists(cwd / CONFIG_FILE)) {
        std::cerr << "Error: Missing 'spp.toml' file.\n";
        return false;
    }

    // Create the other folders if they don't exist.
    if (not std::filesystem::exists(cwd / OUT_FOLDER)) {
        std::filesystem::create_directory(cwd / OUT_FOLDER);
    }
    if (not std::filesystem::exists(cwd / VCS_FOLDER)) {
        std::filesystem::create_directory(cwd / VCS_FOLDER);
    }
    if (not std::filesystem::exists(cwd / FFI_FOLDER)) {
        std::filesystem::create_directory(cwd / FFI_FOLDER);
    }

    // Parse the spp.toml config file and get the optional "project" section.
    const auto toml = toml::parse_file(CONFIG_FILE);
    if (not toml.contains("project")) {
        std::cout << "Error: No [project] section found in spp.toml.\n";
        return false;
    }

    // Check the project section has a name, version and build type.
    const auto project = toml["project"].as_table();
    if (not project->contains("name")) {
        std::cerr << "Error: No name found in [project] section of spp.toml.\n";
        return false;
    }
    if (not project->contains("version")) {
        std::cerr << "Error: No version found in [project] section of spp.toml.\n";
        return false;
    }
    if (not project->contains("build")) {
        std::cerr << "Error: No build type found in [project] section of spp.toml.\n";
        return false;
    }

    // Check the version follows "major.minor.patch" format.
    const auto version = project->at("version").value<Str>().value_or("");
    const auto version_parts = version | genex::views::split('.') | genex::to<Vec>();
    if (version_parts.Len() != 3) {
        std::cerr << "Error: Invalid version format in spp.toml. Version must follow 'major.minor.patch' format.\n";
        return false;
    }

    // Check the build type is either "exe" or "lib".
    const auto build_type = project->at("build").value<Str>().value_or("");
    if (build_type != "exe" and build_type != "lib") {
        std::cerr << "Error: Invalid build type in spp.toml. Build type must be 'exe' or 'lib'.\n";
        return false;
    }

    // For the VCS folders, validate each VCS entry (if it exists).
    for (auto const &vcs_dir : std::filesystem::directory_iterator(cwd / VCS_FOLDER)) {
        std::filesystem::current_path(cwd / vcs_dir);
        handle_validate(build_type == "exe");
        std::filesystem::current_path(cwd);
    }

    // Check the FFI subfolders are structured properly.
    const auto ext = get_system_shared_library_extension();
    for (auto const &ffi_dir : std::filesystem::directory_iterator(cwd / FFI_FOLDER)) {
        if (not std::filesystem::is_directory(ffi_dir)) {
            std::cerr << "Error: Non-directory found in 'ffi' folder: "s + utils::files::DisplayString(ffi_dir.path().filename()) + "\n";
            return false;
        }

        // Check for "{library_name}/lib/{library_name}.{ext}" and "{library_name/stub.spp}" files.
        // if (not std::filesystem::exists(ffi_dir.path() / "lib" / (ffi_dir.path().filename().string() + "." + ext))) {
        //     std::cerr << "Error: Missing shared library file in 'ffi/"s + ffi_dir.path().filename().string() + "/lib' folder.\n";
        //     return false;
        // }
        //
        // // Check for stub file.
        // if (not std::filesystem::exists(ffi_dir.path() / "stub.spp")) {
        //     std::cerr << "Error: Missing 'stub.spp' file in 'ffi/"s + ffi_dir.path().filename().string() + "' folder.\n";
        //     return false;
        // }
    }

    // All checks passed.
    return true;
}

auto spp::cli::handle_version()
    -> void {
    // Print the version information.
    std::cout << "SPP version " << SPP_VERSION << "\n";
}

auto spp::cli::create_default_config_for(
    Str const &project_name)
    -> Str {
    // Inject the project name into the config template.
    auto contents = format_default_file_contents(CONFIG_FILE_CONTENTS);
    const auto pos = contents.find('$');
    contents.replace(pos, 1, project_name);
    return contents;
}

auto spp::cli::get_system_shared_library_extension()
    -> Str {
    // Return the appropriate shared library extension for the current OS.
#if defined(_WIN32) || defined(_WIN64)
    return "dll";
#elif defined(__APPLE__)
    return "dylib";
#else
    return "so";
#endif
}

auto spp::cli::unit_test(
    Str const &mode,
    Str &&main_code)
    -> void {
    // Validate the project structure first.
    SPP_VALIDATE_STRUCTURE(false);

    // Create the inner directory (rel or dev).
    const auto cwd = std::filesystem::current_path();
    std::filesystem::create_directory(cwd / OUT_FOLDER / mode);
    SPP_VALIDATE_STRUCTURE(false);

    // Compile the code.
    const auto m = mode == "dev" ? compiler::Compiler::Mode::DEV : compiler::Compiler::Mode::REL;
    const auto c = compiler::Compiler::ForUnitTests(m, std::move(main_code));
    c->Compile();
}

auto spp::cli::format_default_file_contents(
    const StrView contents)
    -> Str {
    return Str(contents);
    // // Remove the first newline, and replace "    " with "".
    // auto out = Str();
    // for (const auto&line: contents | genex::views::split('\n')) {
    //     if (line.Len() <= 0) { continue; }
    //     auto formatted = line | genex::to<Str>();
    //     formatted.replace(formatted.find("    "), 4, "");
    // }
    // return out;
}
