# Tooling

## Commands

| Command         | Description                                                          | Implemented |
|-----------------|----------------------------------------------------------------------|-------------|
| `spp init`      | Create a new S++ project in the current directory.                   | `Y`         |
| `spp build`     | Compile the S++ project in the current directory.                    | `Y`         |
| `spp run`       | Build and run the S++ project in the current directory.              | `Y`         |
| `spp test`      | Run the tests for the S++ project in the current directory.          | `Y`         |
| `spp vcs`       | Update dependencies for the S++ project in the current directory.    | `Y`         |
| `spp clean`     | Remove build artifacts for the S++ project in the current directory. | `Y`         |
| `spp update`    | Update the S++ compiler to the latest version.                       | `N`         |
| `spp uninstall` | Uninstall the S++ compiler from the system.                          | `N`         |
| `spp version`   | Display the version of the installed S++ compiler.                   | `Y`         |
| `spp help`      | Display help information about S++ commands.                         | `Y`         |

## Initializing a new project

To create a new S++ project, navigate to the target directory in your terminal and run the `init` command. The folder
must be empty, and the command populates it with the files and directories for a basic S++ project structure. The
project structure section covers this in more detail.

The `init` command takes no arguments. Editing the generated `spp.toml` configuration file later changes project
settings, such as dependencies and build options. All configuration lives in the `spp.toml` file rather than in
initialization flags, which keeps the project flexible to reconfigure after creation.

## Building the project

To compile the S++ project, navigate to the project directory in your terminal and run the `build` command. This
compiles all source files in the `src` directory, followed by the `vcs` directory when present. The order of member
declaration doesn't matter in S++, so the order of file processing doesn't matter either. The compiler analyses the stub
files from the `ffi` folder last, which adds those functions to the symbol table.

Once the compiler has produced LLVM IR for all the code, the linker combines the object files into a single executable
binary and links the FFI shared object files as needed. The output binary lands in the `out` directory under the name
from the `spp.toml` configuration file. Without a name, it becomes `<project_name>.exe` on Windows, and
`<project_name>` on Linux and macOS.

S++ uses incremental builds, so it recompiles only the files that changed since the last build. This speeds up the build
process considerably on larger projects. To force a full rebuild, run the `clean` command before the `build` command.
Incremental building covers bytecode generation only: every build still analyses all modules across the project to
generate the scope and symbol tables.

A few flags apply to the build process. They don't live in the config file, because they relate to the build process
itself rather than to the project configuration:

| Flag                   | Description                                                  |
|------------------------|--------------------------------------------------------------|
| `--mode <mode>`        | Set the build mode, `dev` or `rel`. Defaults to `dev`.            |
| `--target-type <type>` | Set the target type: `exe`, `shared` or `static`. Defaults to `exe`. |

A project built as a shared or static library produces no executable, so it needs no entry point. The compiler drops
`main.spp` from the module tree and skips the `main` method checks.

## Running the project

To build and run the S++ project, navigate to the project directory in your terminal and run the `run` command. This
first builds the project through the same process as the `build` command. On a successful build, the resulting
executable binary runs immediately. The command acts as an alias for:

```
spp build && ./out/target-type/<binary_name>
```

The run command accepts the following flag and passes it to the build process:

| Flag            | Description                                       |
|-----------------|---------------------------------------------------|
| `--mode <mode>` | Set the build mode, `dev` or `rel`. Defaults to `dev`. |

This command runs the target, so it supports executable targets only, and it adds the `--target-type exe` flag to the
build process.

## Updating dependencies

The `[vcs]` section of the configuration file defines the dependencies. Most projects add the standard library as a
dependency:

```toml
[vcs]
std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "master" }
```

On each run, the compiler checks whether each dependency already exists in the `vcs` directory. If not, it clones the
dependency from the given Git repository and checks out the given branch or version. If the dependency already exists, a
`git pull` updates it to the latest version on that branch.

To stop a dependency from tracking its repository, pin a specific version in the configuration file:

```toml
[vcs]
std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "68e0b0ae0acde9d2b65394b5c8f1e923e8374b6d" }
```

For code that runs often, one line in the `[project]` section of `spp.toml` skips the `vcs` checks entirely:

```toml
[project]
vcs = false
```

## Getting the version of the compiler

The command `spp version` displays the current version of the S++ compiler installed on the system, along with the build
date and commit hash. This information helps with debugging, and with confirming compatibility against specific versions
of the S++ language and standard library.

A flag also applies the version command recursively to all dependencies in the `vcs` directory:

| Flag     | Description                                       |
|----------|---------------------------------------------------|
| `--deps` | Display version information for all dependencies. |
