# Installation

## Installing the Compiler

This Git repository contains the entire source code of the S++ compiler. Under the `releases` tab, you can find
pre-compiled binaries for various platforms, including Windows, macOS and Linux. Download the appropriate binary for
your operating system. Alternatively, you can build the compiler from source, based on the branch tag per stable
release, or the current `main` branch for the latest development version.

For the downloaded binaries, extract the contents of the zip file, and run the installer. This will move everything into
a specific directory, which can be configured, and will append this directory to the system PATH variable, allowing you
to run the `spp` command from any terminal window.

The binary is a standalone executable, and any dependencies will be included in the installation directory for Windows.
For Linux based systems, dependencies will need to be installed separately. The standard library isn't included as a
global include, as s++ works like Python's virtual environments; each project maintains its own dependencies. You can
create a new project using the `spp init` command, which will fill a directory with the project scaffold, including the
standard library from its Git repository.

## Updating the Compiler

The compiler binary can be updated using the `spp update [--branch branch] [--version version] [--dev]` command. If a
branch tag is found that matches the specified branch, or a version tag that matches the specified version, the binary
will be downloaded and installed, replacing the current binary. If no branch or version is specified, the latest stable
release will be downloaded and installed. Specifying the `--dev` flag will download and install the latest development
version from the `main` branch.

The standard library is its own repository, and is isolated completely from the compiler versions. If the `spp.toml` for
th project is configured to use a specific version of the standard library, that version will be used. If no version is
specified, the latest stable release will be used. Using `branch=main` will use the latest development version of the
standard library.

## Uninstalling the Compiler

The only compiler files are the binary itself, and the installation directory. To uninstall the compiler, run the
command `spp uninstall`. This will remove the installation directory, and remove the directory from the system PATH
variable. Whilst the compiler directory can just be deleted and leave nothing behind, it will leave the PATH variable
with a reference to a non-existent directory, which may cause issues in the future.

The compiler doesn't create any other files or directories on the system, so there will be no artifacts left behind
after uninstalling. Any projects created with the `spp init` command will remain, and can be deleted manually if
required.
