# Modules

To simplify the module system as far as possible, S++ ties the following three concepts together into one thing:

- Modules
- Namespaces
- Filesystem

As the STL shows, `std/string.spp` defines the module `std::string`, which the namespace `std::string` also represents,
and which holds the type `Str`. Reading the file system gives you the namespace, and reading a fully qualified namespace
gives you the file for the type.

## Child modules

Child modules matter for the protected access modifier, which the
**[encapsulation](../advanced/encapsulation.md)** section discusses. Defining a child module requires a folder matching
the filename of the parent module. For example, given a folder `std/string` containing `impl.spp`, the module
`std::string::impl` is a child module of `std::string`, which `std/string.spp` defines.
