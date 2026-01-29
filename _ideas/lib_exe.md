# Libraries vs Executables

The `spp.toml` will control whether the project is built as a library or an executable. If the project is a library,
then the `main.spp` is not required, and errors if present. Otherwise, `main.spp` is required, as executables need an
entry point.

For example, the `std` library will be built as a library, so won't have a `main.spp` file. The `src/std` inner folder
structure is still required, because of the `ffi` folder potentially being present for libraries too. So `src` and `ffi`
will be siblings inside the package.