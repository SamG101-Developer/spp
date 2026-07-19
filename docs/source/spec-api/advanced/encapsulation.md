# Encapsulation

S++ follows fairly traditional encapsulation techniques, using access modifiers, but defined with annotations rather
than new keyword. should the behaviour or parameterization of these access modifiers change in the future, then the
function definitions for the annotations can change, rather than having to update the entire keyword system.

There are two different areas where access modifiers can be used.

- The **module** scope: classes, functions, type aliases, cmp definitions.
- The **class/superimposition** scopes: methods and attributes.

## Module Scopes

- **Private**: A private module member can only be accessed from the same module as the member is defined in. Any module
  member marked as `!private` in the module `std/string.spp`, can be accessed from any code within `std/string.spp`, but
  not from any other module. As modules are tied to the filesystem, it means that only code within the same file can
  access private members.


- **Protected**: A protected module member can be accessed from the same module as the member is defined in, and any
  child modules. Child modules are within a folder sharing the same name as the target module. See the section on
  **[child modules](../modules/modules.md#child-modules)** for more information on child module definitions. In short,
  any code defined inside `std/string/impl.spp` can access protected members of `std/string.spp`.


- **Package**: A package module member is public to the package, but not outside of the package. It works, semantically,
  like Rusts's `pub(crate)`, or C#'s `internal`. A package module member can be accessed from any module within the same
  package as the member is defined in, but not from any module outside of the package.


- **Public**: A public module member can be accessed from any module. Any code in any module in any package can access
  public members of any other module. There are no restrictions on the access of public members.

## Class/Superimposition Scopes

These access modifier rules are slightly more complicated, because superimpositions can be done externally to the file
containing the class definition of the module. To make the access modifier control meaningful, the restrictions are
fairly tight. Class members are defined as "fields", and superimposition members are either "methods", "type aliases",
or "cmp definitions".

- **Private**: A private class member can only be accessed from the same class as the member is defined in, in the same
  module as the module that the enclosing class is defined in.


- **Protected**: A protected class member can be accessed either from the same class the member is defined in, in the
  same module, or from any subtypes, in the same module that the subtype was defined in.


- **Package**: A package class member can be accessed form any class, in any module, so long as the module belongs to
  the same package as the definition location. This is the same as Rust's `pub(crate)`, or C#'s `internal`.


- **Public**: A public class member can be accessed from any module, and any type. There are no restrictions on the
  access of public members. Even modules in different packages will be able to access public members of a class.