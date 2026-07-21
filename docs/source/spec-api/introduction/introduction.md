# Introduction

<img src="../spp.svg" alt="S++ Logo" width="20"/>

S++ is a modern, general-purpose systems programming language. Its syntax fuses Rust and Python, aiming to stay both
powerful and readable. S++ provides the performance and control of low-level languages while keeping the simplicity
and expressiveness of high-level languages. It focuses heavily on memory and type safety, concurrency, and modern
programming paradigms.

Key features include a static and strong type system that supports full one-way type inference, generics, and variant
types. Others include advanced nested pattern matching, coroutines, and a full module system. The compiler enforces
memory safety at compile time, using a blend of techniques including second-class borrows, the law of exclusivity,
ownership tracking and default move semantics.

## Philosophy

- **PERFORMANCE**: S++ targets high performance, suiting systems programming, game development, and other
  performance-critical applications. It gives low-level control over memory and system resources, while still offering
  high-level abstractions that simplify complex tasks. Compiling to LLVM IR achieves this, and lets S++ use the
  optimizations of the LLVM toolchain.

- **READABILITY**: S++ emphasizes code readability and maintainability. Its syntax is clean and expressive, borrowing
  elements from Python to stay approachable for developers of all skill levels. The language encourages clear and
  concise code, which helps developers understand and collaborate on projects. S++ avoids legacy syntax from C-style
  languages where possible, in favor of more modern constructs.

- **ORTHOGONAL**: S++ stays consistent across all aspects of the language. Similar concepts take similar forms, which
  reduces the learning curve and makes the behavior of each part of the language predictable. A key example: all types
  act like first class objects, including numbers, booleans, voids, variants, functions, arrays, and tuples.

- **VERSATILE**: S++ is a general-purpose language, suitable for a wide range of applications. It supports procedural,
  object-oriented, and functional programming. That versatility lets developers choose the best approach for each use
  case, and makes S++ a flexible choice across projects.

- **EFFICIENT**: S++ aims at efficiency in both development and execution. It re-evaluates legacy programming language
  features and provides modern alternatives where appropriate. Forced type inference reduces boilerplate, and features
  like coroutines and pattern matching simplify complex tasks. Compile time checks catch hard-to-find runtime memory
  errors, which cuts debugging time.

- **SAFETY**: The compiler enforces memory, type and thread safety at compile time, which reduces runtime errors and
  security vulnerabilities. S++ combines ownership tracking, borrowing rules, and type checking to keep programs safe
  and reliable. This focus on safety suits applications where reliability is critical.

## Key features

- **EXPRESSION ORIENTED**: In S++, almost every structure is an expression rather than a statement, including
  conditional branching, conditional looping, and anonymous scopes. This yields more concise and flexible code, because
  these constructs fit a wider variety of contexts, such as inside other expressions or as part of larger computations.

- **ADVANCED TYPE SYSTEM**: Every type in S++ is "first-class," and the compiler infers a type for every expression, so
  explicit types appear only in a few contexts. The type system supports generics, variant types, and type constraints,
  which makes code flexible and reusable. Pattern matching also supports flow typing.

- **MEMORY SAFETY**: S++ blends memory safety techniques, including ownership tracking, borrowing rules, and the law of
  exclusivity. Together they manage memory safely and efficiently, and prevent common issues such as null pointer
  dereferencing, dangling pointers, and data races. The borrow checker resembles Rust's, but uses second-class borrows,
  which simplifies lifetime management.

- **CLASS SYSTEM**: Like Rust, S++ splits state and behavior, using classes for data and superimposition blocks for
  behavior. S++ classes can also superimpose over each other, which gives traditional inheritance while isolating each
  inheritance in its own block. The result is modular, reusable code that stays maintainable.

- **SUPERIMPOSITION**: Like Rust's `impl` block, S++ uses superimposition blocks to define either methods or entire
  type extensions over another type. A block can define methods over a type, or extend a type in the traditional
  inheritance sense and override base method implementations. A base method must carry the abstract or virtual marker
  before anything can override it.

- **MODULE SYSTEM**: Every file in an S++ project is a module. Modules, namespaces and the file system all align, which
  keeps code straightforward to organize and manage. Modules don't need importing as in Python: each one is available
  from the common namespace between that module and the current module, as in Rust. The access modifier annotations
  export symbols.

- **FUNCTIONS**: Functions are rich in features, supporting overloading, recursion, optional and type-safe variadic
  parameters. Closures can capture variables with parameter conventions, and support full type inference. Every
  function is either a `fun` subroutine or a `cor` coroutine.

- **ASYNC**: Rather than marking functions as `async` capable at the prototype site, S++ lets the caller _call_ any
  function as `async`, which resolves the traditional "color problem." An async call uses coroutine backend logic to run
  the target in the background, and immediately creates a `Fut[T]` future type to hold the result. Dropping the `async`
  call modifier takes the place of awaiting.

- **GENERICS**: S++ has a powerful generics system for building type-safe, reusable components. Generics work with
  classes, functions, type statements and superimpositions, in required, optional or variadic form. S++ supports both
  type generics and `cmp` non-type generics, such as `Arr[T, cmp n: USize]`. Non-type generics accept any type, not just
  numbers and booleans.

- **CONCURRENCY**: Coroutines form the basis of the S++ concurrency model. The `cor` keyword defines them, and they can
  pause and resume, which gives cooperative multitasking. They also support bilateral data movement, subject to memory
  safety constraints, and can yield values with parameter conventions such as borrows, which forms the basis for all
  iteration.

- **PARALLELISM**: S++ uses threads, and the associated primitives, to achieve parallelism. The `Thread` type creates
  threads, and channels let them communicate. Mutexes and atomic types manage shared state and keep it thread safe.
  Higher level parallel constructs, such as parallel for loops, appear as postfix methods over the iterator types.

- **ERROR HANDLING**: S++ has no Python- or Java-like exceptions. It uses a combination of `Opt[T]` and `Res[T, E]`
  types to represent optional values and results that may fail. This encourages explicit error handling, and shows
  exactly when a function can fail and how to handle the failure. The postfix `?` operator, stolen from Rust, simplifies
  propagating errors.

- **PATTERN MATCHING**: S++ has a powerful pattern matching system for deconstructing complex data structures and
  matching them against patterns. It covers tuples, arrays, objects, variants, nested patterns, and guards. Flow typing
  works alongside variant destructuring to refine types from matched patterns. Pattern matching appears in `case`
  expressions and `let` statements.

- **LOOP CONTROL FLOW**: S++ provides advanced loop control flow mechanisms, including `skip` and `exit`, which stack to
  interact with nested loops. Loops can also return values, which makes iteration constructs more expressive and
  flexible. Only one loop construct exists, `loop`, and it takes either a boolean or an iteration expression, which
  decides whether it behaves like a traditional `while` or `for` loop.
