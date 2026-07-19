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
| `spp version`   | Uninstall the S++ compiler from the system.                          | `Y`         |
| `spp help`      | Display help information about S++ commands.                         | `Y`         |

## Initializing a new project

To create a new S++ project, navigate to the desired directory in your terminal and run the `init` command. The folder
must be empty, and the command will populate it with the necessary files and directories for a basic S++ project
structure. See the project structure section for more information on this.

No arguments are passed to the `init` command. The generated `spp.toml` configuration file can be modified later to
change project settings, such as dependencies and build options. All configurations are done from the `spp.toml` file,
not from initialization flags. This provides more flexibility in configuring the project after creation.

## Building the project

To compile the S++ project, navigate to the project directory in your terminal and run the `build` command. This will
compile all source files in the `src` directory, followed by the `vcs`directory if is present. Because the order of
member declaration doesn't matter in S++, the order of file processing doesn't matter either. Finally, the stub files
from the `ffi` folder are analysed too, to add these functions into the symbol table.

Once all the code has been compiled into LLVM IR, the linker will combine all the object files into a single executable
binary, linking the ffi shared object files as required. The output binary will be placed in the `out` directory, with
the name specified in the `spp.toml` configuration file, or `<project_name>.exe` on Windows, and `<project_name>` on
Linux and macOS, if no name is specified.

S++ uses incremental builds, so only files that have changed since the last build will be recompiled. This speeds up
the build process significantly for larger projects. If you want to force a full rebuild, you can use the `clean`
command before running the `build` command again. Whilst incremental building is used for the bytecode generation, all
modules will be analysed for scope/symbol table generation for the entire project on each build.

There are some flags available for the build process, that aren't in the config file because they relate to the build
process itself, rather than the project configuration:

| Flag                   | Description                                                  |
|------------------------|--------------------------------------------------------------|
| `--mode <mode>`        | Set the build mode (`dev/rel`). Default is `dev`.            |
| `--target-type <type>` | Set the target type (`exe/shared/static`). Default is `exe`. |

If a project is built as a shared or static library, then because no executable is created, an entry point is not
required, so `main.spp` is removed from the module tree, and `main` method checks are not performed.

## Running the project

To build and run the S++ project, navigate to the project directory in your terminal and run the `run` command. This
will first build the project using the same process as the `build` command. If the build is successful, the resulting
executable binary will be executed immediately. It is effectively an alias for the combined commands:

```
spp build && ./out/target-type/<binary_name>
```

The following flags are available for the run command, which are passed to the build process:

| Flag            | Description                                       |
|-----------------|---------------------------------------------------|
| `--mode <mode>` | Set the build mode (`dev/rel`). Default is `dev`. |

Because the target is being run in this command, only executable targets are supported. So the flag `--target-type exe`
is also added into the build process.

## Updating dependencies

In the configuration file, the dependencies are defined in the `[vcs]` section. Typically, the standard library is added
as a dependency, using the following code:

```toml
[vcs]
std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "master" }
```

Per run, each dependency is checked to see if it is already present in the `vcs` directory. If it is not, it is cloned
from the specified Git repository, and the specified branch or version is checked out. If it is already present, then a
`git pull` is performed to update the dependency to the latest version on the specified branch.

To prevent automatically updating a dependency as it gets updated in its repository, a specific version can be specified
in the configuration file, like so:

```toml
[vcs]
std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "68e0b0ae0acde9d2b65394b5c8f1e923e8374b6d" }
```

If the code needs to be run often, the `vcs` checks can be blanket-skipped in `spp.toml`, by adding the following line
to the `[build]` section:

```toml
[project]
vcs = false
```

## Getting the version of the compiler

The command `spp version` displays the current version of the S++ compiler installed on the system. This includes the
version number, build date, and commit hash. This information is useful for debugging and ensuring compatibility with
specific versions of the S++ language and standard library.

Furthermore there is a flag that can be used to recursively apply the version command to all dependencies in the
`vcs` directory:

| Flag     | Description                                       |
|----------|---------------------------------------------------|
| `--deps` | Display version information for all dependencies. |
