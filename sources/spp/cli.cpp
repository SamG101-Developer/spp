module;
#include <spp/macros.hpp>

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
import toml;


#define SPP_VALIDATE_STRUCTURE \
    if (not handle_validate()) { return; }

#define SPP_CLI_NULL \
    bp::v1::std_out > bp::v1::null


inline constexpr std::string OUT_FOLDER = "out";
inline constexpr std::string SRC_FOLDER = "src";
inline constexpr std::string VCS_FOLDER = "vcs";
inline constexpr std::string FFI_FOLDER = "ffi";
inline constexpr std::string MAIN_FILE = "main.spp";
inline constexpr std::string CONFIG_FILE = "spp.toml";

inline const std::string MAIN_FILE_CONTENTS = R"(
    use std::string::Str
    use std::vector::Vec
    use std::void::Void

    fun main(args: Vec[Str]) -> Void {
        std::io::println("Hello world!")
    })";

const auto CONFIG_FILE_CONTENTS = R"(
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
    auto app = cli11::App("SPP build tool", "spp");
    app.require_subcommand(1);
    auto mode = std::string();

    app.add_subcommand("init", "Initialize the new project")
       ->callback(handle_init);

    app.add_subcommand("vcs", "Initialize version control for the project")
       ->callback(handle_vcs);

    app.add_subcommand("build", "Build the project")
       ->callback([&mode] { handle_build(mode); })
       ->add_option("-m,--mode", mode, "Build mode (dev or rel)")
       ->check(cli11::IsMember({"dev", "rel"}))
       ->default_val("dev");

    app.add_subcommand("run", "Run the project")
       ->callback([&mode] { handle_run(mode); })
       ->add_option("-m,--mode", mode, "Run mode (dev or rel)")
       ->check(cli11::IsMember({"dev", "rel"}))
       ->default_val("dev");

    app.add_subcommand("clean", "Clean the project")
       ->callback([&mode] { handle_clean(mode); })
       ->add_option("-m,--mode", mode, "Clean mode (dev, rel or all)")
       ->check(cli11::IsMember({"dev", "rel", "all"}))
       ->default_val("all");

    app.add_subcommand("test", "Test the project")
       ->callback(handle_test);

    app.add_subcommand("validate", "Validate the project")
       ->callback(handle_validate);

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
    utils::files::write_file(cwd / SRC_FOLDER / MAIN_FILE, MAIN_FILE_CONTENTS);
    utils::files::write_file(cwd / CONFIG_FILE, create_default_config_for(cwd.filename().string()));
}


auto spp::cli::handle_vcs()
    -> void {
    // Validate the project structure first.
    using namespace std::string_literals;
    SPP_VALIDATE_STRUCTURE;

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
        auto repo_name = std::string(key);
        auto repo_url = (*info.as_table())["git"].value<std::string>().value();
        auto repo_branch = (*info.as_table())["branch"].value<std::string>().value_or("master");
        auto repo_folder = cwd / VCS_FOLDER / repo_name;

        // Repo doesn't exist locally => clone it.
        if (not std::filesystem::exists(repo_folder)) {
            std::system(("git clone --branch " + repo_branch + " " + repo_url + " " + repo_folder.string()).c_str());
            std::cout << "Cloned "s + repo_name + " from " + repo_url + "\n";
        }
        else {
            std::system(("git -C " + repo_folder.string() + " pull origin " + repo_branch).c_str());
            std::system(("git -C " + repo_folder.string() + " checkout " + repo_branch).c_str());
            std::cout << "Updated "s + repo_name + " from " + repo_url + " (" + repo_branch + ")" + "\n";
        }

        // Copy all DLLs from the VCS's FFI folder into this project's FFI folder.
        auto ffi_repo_folder = repo_folder / FFI_FOLDER;
        if (std::filesystem::exists(ffi_repo_folder)) {
            for (const auto &entry : std::filesystem::directory_iterator(ffi_repo_folder)) {
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
    std::string const &mode,
    const bool skip_vcs)
    -> void {
    // Validate the project structure first.
    SPP_VALIDATE_STRUCTURE;

    // Create the inner directory (rel or dev).
    const auto cwd = std::filesystem::current_path();
    std::filesystem::create_directory(cwd / OUT_FOLDER / mode);

    // Handle VCS if not skipped.
    if (not skip_vcs) { handle_vcs(); }

    // Revalidate (after including the VCS folders).
    SPP_VALIDATE_STRUCTURE;
    const auto build_type =
        toml::parse_file(CONFIG_FILE)["project"].as_table()->at("build").value<std::string>();

    // Validate the mode is "dev" or "rel".
    if (mode != "dev" and mode != "rel") {
        std::cerr << "Error: Invalid mode. Mode must be 'dev' or 'rel'.\n";
        return;
    }

    // Compile the code.
    auto c = compiler::Compiler(
        mode == "dev" ? compiler::Compiler::Mode::DEV : compiler::Compiler::Mode::REL,
        build_type == "exe" ? compiler::Compiler::BuildType::EXE : compiler::Compiler::BuildType::LIB);
    c.compile();
}


auto spp::cli::handle_run(
    std::string const &mode)
    -> void {
    // Build the project first (skip VCS).
    handle_build(mode, true);

    // Run the executable.
    // TODO
}


auto spp::cli::handle_clean(
    std::string const &mode)
    -> void {
    // Validate the project structure first.
    SPP_VALIDATE_STRUCTURE;

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
    SPP_VALIDATE_STRUCTURE;

    // TODO
}


auto spp::cli::handle_validate()
    -> bool {
    using namespace std::string_literals;

    // Check for the key folders/files.
    const auto cwd = std::filesystem::current_path();
    if (not std::filesystem::exists(cwd / SRC_FOLDER)) {
        std::cerr << "Error: Missing 'src' folder.\n";
        return false;
    }
    if (not std::filesystem::exists(cwd / SRC_FOLDER / MAIN_FILE)) {
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
    const auto version = project->at("version").value<std::string>().value_or("");
    const auto version_parts = version | genex::views::split('.') | genex::to<std::vector>();
    if (version_parts.size() != 3) {
        std::cerr << "Error: Invalid version format in spp.toml. Version must follow 'major.minor.patch' format.\n";
        return false;
    }

    // Check the build type is either "exe" or "lib".
    const auto build_type = project->at("build").value<std::string>().value_or("");
    if (build_type != "exe" and build_type != "lib") {
        std::cerr << "Error: Invalid build type in spp.toml. Build type must be 'exe' or 'lib'.\n";
        return false;
    }

    // For the VCS folders, validate each VCS entry (if it exists).
    for (auto const &vcs_dir : std::filesystem::directory_iterator(cwd / VCS_FOLDER)) {
        std::filesystem::current_path(cwd / vcs_dir);
        handle_validate();
        std::filesystem::current_path(cwd);
    }

    // Check the FFI subfolders are structured properly.
    const auto ext = get_system_shared_library_extension();
    for (auto const &ffi_dir : std::filesystem::directory_iterator(cwd / FFI_FOLDER)) {
        if (not std::filesystem::is_directory(ffi_dir)) {
            std::cerr << "Error: Non-directory found in 'ffi' folder: "s + ffi_dir.path().filename().string() + "\n";
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
    std::string const &project_name)
    -> std::string {
    // Inject the project name into the config template.
    auto content = std::string(CONFIG_FILE_CONTENTS);
    const auto pos = content.find('$');
    content.replace(pos, 1, project_name);
    return content;
}


auto spp::cli::get_system_shared_library_extension()
    -> std::string {
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
    std::string const &mode,
    std::string &&main_code)
    -> void {
    // Validate the project structure first.
    SPP_VALIDATE_STRUCTURE;

    // Create the inner directory (rel or dev).
    const auto cwd = std::filesystem::current_path();
    std::filesystem::create_directory(cwd / OUT_FOLDER / mode);
    SPP_VALIDATE_STRUCTURE;

    // Compile the code.
    const auto m = mode == "dev" ? compiler::Compiler::Mode::DEV : compiler::Compiler::Mode::REL;
    const auto c = compiler::Compiler::for_unit_tests(m, std::move(main_code));
    c->compile();
}
