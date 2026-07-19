# Modules

In S++, in order to maximize the simplification of a module system, the following three concepts are tied together into
teh same thing:

- Modules
- Namespaces
- Filesystem

Therefore, as seen in the stl, `std/string.spp` defines the module `std::string`, also represented by the namespace
`std::string`, where the type `Str` is located. What this means, is that by looking at the filesystem, you can determine
the namespace, and by seeing a (fully qualified namespace), you can identify teh file for the type.

## Child Modules

Child modules are important for the protected access modifier, discussed in the
**[encapsulation](../advanced/encapsulation.md)** section. To define a child module, there must be a folder matching the
filename of the parent module. For example, if there was a folder `std/string`, containing `impl.spp`, then the module
`std::string::impl` is a child module of `std::string`, defined with `std/string.spp`.
