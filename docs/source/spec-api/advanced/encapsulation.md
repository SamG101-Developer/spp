# Encapsulation

S++ follows traditional encapsulation techniques, using access modifiers defined with annotations rather than with new
keywords. Should the behaviour or parameterization of these access modifiers change in the future, the function
definitions for the annotations can change, instead of the whole keyword system.

Access modifiers apply in two different areas.

- The **module** scope: classes, functions, type aliases, cmp definitions.
- The **class/superimposition** scopes: methods and attributes.

## Module scopes

- **Private**: Only the module that defines a private module member can access it. Any code within `std/string.spp` can
  reach a member marked `!private` in that module, and no other module can. Modules tie to the file system, so only
  code within the same file can access private members.

- **Protected**: The defining module and any child modules can access a protected module member. Child modules live in
  a folder sharing the name of the target module. The **[child modules](../modules/modules.md#child-modules)** section
  covers child module definitions. In short, any code inside `std/string/impl.spp` can access protected members of
  `std/string.spp`.

- **Package**: A package module member is public within the package and invisible outside it. It works like Rust's
  `pub(crate)`, or C#'s `internal`. Any module within the defining package can access it, and no module outside the
  package can.

- **Public**: Any module can access a public module member. Code in any module in any package can reach public members
  of any other module, with no restrictions.

## Class and superimposition scopes

These access modifier rules are more involved, because a superimposition can sit outside the file that contains the
class definition. To keep the access modifier control meaningful, the restrictions stay tight. Class members go by the
name fields, and superimposition members are methods, type aliases, or cmp definitions.

- **Private**: Only the defining class can access a private class member, and only from the module that defines the
  enclosing class.

- **Protected**: The defining class can access a protected class member from the same module, and so can any subtype,
  from the module that defines that subtype.

- **Package**: Any class, in any module, can access a package class member, as long as the module belongs to the same
  package as the definition. This matches Rust's `pub(crate)`, or C#'s `internal`.

- **Public**: Any module and any type can access a public class member, with no restrictions. Even modules in different
  packages can access the public members of a class.
