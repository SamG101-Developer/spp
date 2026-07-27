# Project structure

S++ follows a strict project structure that the compiler verifies at compile time. A few key files and folders must
exist in the project root, and the compiler raises an error when it can't find them. The structure looks like this:

```
proj_name
├── src*               # Source code folder
│   ├── proj_name      # Source code module folder
│   └── src/main.spp*  # Main source file (entry point)
├── out                # Build output folder
├── vcs                # VCS imported libraries folder
├── ffi                # FFI libraries folder
└── spp.toml*          # Project configuration file
```

The project needs the files and folders marked with an `*` to compile successfully. The compiler also looks for the
other folder names, but doesn't require them.

## The `src` folder

The `src` folder contains the code of the program. The `main.spp` file is the entry point, and must contain the `main`
method. The folder beside `main.spp`, which must carry the name of the project, holds the source code. All modules and
source files for the project belong there.

## The `out` folder

The `out` folder holds the compiled output of the project. The compiler creates this folder, and recreates it after a
deletion. It contains subfolders for the build type, such as `dev` or `rel`, and the compiled output files go into those
subfolders. Deleting the folder only costs extra recompilation, because the compiler then rebuilds the project to
generate the output files again.

## The `vcs` folder

The `vcs` section of the `spp.toml` file populates the `vcs` folder, which stores external libraries hosted on version
control systems such as Git. The compiler clones the repositories named in `spp.toml` into this folder, and the source
code from those libraries becomes available under the VCS project name as the root namespace part.

For example, if the `spp.toml` file contains the following:

```toml
[vcs]
std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "master" }
lib_a = { git = "<URL>", branch = "master" }
```

The `std` files then become available as `std::string::Str`, and the `lib_a` files as `lib_a::module::Type`. Compiling
the project checks the VCS libraries for updates, and the compiler updates the libraries in the `vcs` folder when it
finds changes.

Giving the hash or tag of a fixed version under the `branch` attribute prevents automatic updates. The library then
stays at that version and doesn't update when the project compiles, which keeps the project on a specific version of a
library and protects it from breaking changes.

## The `ffi` folder

The `ffi` folder adds code that other languages have compiled into shared libraries. A key example is `libc`,
whose allocation, console, file and socket methods sit under the hood of the `std` library. As with the `vcs` folder,
the root names of the FFI projects work as namespaces, such as `libc::...`

The [FFI](../modules/FFI.md) section covers the structure of the `ffi` folder in more detail. A typical `ffi` subfolder
looks like this:

```
ffi
└── v8
    ├── lib
    │   ├── libv8.so
    │   ├── libv8.dylib
    │   └── libv8.dll
    └── stub.spp
```
