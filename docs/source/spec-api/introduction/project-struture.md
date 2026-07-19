# Project Structure

S++ follows a strict project structure, that the compiler verifies at compile time. There are a number of key files and
folders that must be present in the project root, and the compiler will throw an error if they are not found. The
project structure is as follows:

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

The files and folders designated with an `*` are required for the project to compile successfully. The other folder
names are checked for by the compiler, but are not required to be ran.

## The `src` folder

The `src` folder contains the code fo the program. The `main.spp` file is the entry point of the program, and must
contain the `main` method. The sibling folder to `main.spp`, which must be the name of the project, contains the source
code for the project. This is where all the modules and source files for the project must be placed.

## The `out` folder

The `out` folder is where the compiled output of the project is placed. This folder is created by the compiler, and even
if deleted, will be regenerated. It will contain subfolders regarding the build type, such as `dev` or `rel`, and the
compiled output files will be placed in these subfolders. Deleting this folder will just cause additional recompilation
to occur, as the compiler will need to recompile the project to generate the output files again.

## The `vcs` folder

The `vcs` folder is populated based on the `spp.toml` file's `vcs` section. This folder is used to store external
libraries hosted on version control systems (VCS), such as Git. The compiler will clone the repositories specified in
the `spp.toml` file into this folder, and the source code from these libraries can be used by simply using the vcs
project name as the root namespace part.

For example, if the `spp.toml` file contains the following:

```toml
[vcs]
std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "master" }
lib_a = { git = "<URL>", branch = "master" }
```

Then the `std` files can be used with `std::string::Str`, and the `lib_a` files can be used with `lib_a::module::Type`.
The VCS libraries are checked for updates when the project is compiled, and if there are any changes, the compiler will
automatically update the libraries in the `vcs` folder.

Automatic updates can be prevented by providing the hash/tag to the fixed version of the library, under the `branch`
attribute. This will keep the library at that version, and not update it when the project is compiled. This is useful
for ensuring that the project always uses a specific version of a library, and does not break due to changes in the
library.

## The `ffi` folder

The `ffi` folder is used to add code from other languages that has been compiled to shared libraries. A key example of
this is `libc`, whose allocation, console, file and socket methods are used under the hood of the `std` library. Like
with the `vcs` folder, the root names of the ffi projects can be used as namespaces, like `libc::...`

The [ffi](../modules/FFI.md) section goes into more detail about how to structure the `ffi` folder, but typically, an
`ffi` subfolder will look like:

```
ffi
└── v8
    ├── lib
    │   ├── libv8.so
    │   ├── libv8.dylib
    │   └── libv8.dll
    └── stub.spp
```
