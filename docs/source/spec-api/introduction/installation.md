# Installation

## Installing the compiler

This Git repository contains the entire source code of the S++ compiler. The `releases` tab holds pre-compiled binaries
for Windows, macOS and Linux. Download the binary for your operating system. You can also build the compiler from
source, from the branch tag for a stable release, or from the current `main` branch for the latest development version.

For the downloaded binaries, extract the contents of the zip file and run the installer. The installer moves everything
into a directory, which you can configure, and appends that directory to the system PATH variable, so the `spp` command
works from any terminal window.

The binary is a standalone executable. On Windows, the installation directory holds any dependencies. Linux based
systems need those dependencies installed separately. The standard library isn't a global include, because S++ works
like Python's virtual environments, where each project maintains its own dependencies. The `spp init` command creates a
new project, filling a directory with the project scaffold, including the standard library from its Git repository.

## Updating the compiler

The `spp update [--branch branch] [--version version] [--dev]` command updates the compiler binary. When a branch tag
matches the given branch, or a version tag matches the given version, the command downloads and installs that binary
over the current one. With no branch or version, it takes the latest stable release. The `--dev` flag downloads and
installs the latest development version from the `main` branch.

The standard library lives in its own repository, fully isolated from the compiler versions. When the `spp.toml` for the
project names a specific version of the standard library, the build uses that version. With no version, it uses the
latest stable release. Setting `branch=main` selects the latest development version of the standard library.

## Uninstalling the compiler

The only compiler files are the binary itself and the installation directory. To uninstall the compiler, run
`spp uninstall`. The command removes the installation directory and drops it from the system PATH variable. Deleting the
compiler directory by hand leaves nothing behind either, but it leaves the PATH variable pointing at a directory that no
longer exists, which can cause problems later.

The compiler creates no other files or directories on the system, so uninstalling leaves no artifacts behind. Projects
created with the `spp init` command remain in place, ready to delete by hand when no longer needed.
