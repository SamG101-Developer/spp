# Introduction

<img src="../spp.svg" alt="S++ Logo" width="20"/>

S++ is a modern, general-purpose systems programming language. Its syntax is a fusion of Rust and Python, designed to be
both powerful and easy to read. S++ aims to provide the performance and control of low-level languages while maintaining
the simplicity and expressiveness of high-level languages. There is a heavy focus on memory and type safety,
concurrency and modern programming paradigms.

Key features include a static and strong type system, supporting full one way type inference, generics and variant
types; advanced nested pattern matching; coroutines; and a full module system. Memory safety is enforced at compile
time, using a blend of techniques, include second-class borrows, the law of exclusivity, ownership tracking and default
move semantics.

## Philosophy

- **PERFORMANCE**: S++ is designed as a high performance language, suitable for systems programming, game development,
  and other performance-critical applications. It provides low-level control over memory and system resources, while
  still offering high-level abstractions to simplify complex tasks. This is achieved by compiling to LLVM IR, allowing
  S++ to leverage the powerful optimizations of the LLVM toolchain.


- **READABILITY**: S++ emphasizes code readability and maintainability. Its syntax is clean and expressive,
  borrowing elements from Python to make it approachable for developers of all skill levels. The language encourages
  clear and concise code, making it easier to understand and collaborate on projects. Legacy syntax from C-style
  languages is avoided where possible, in favor of more modern constructs.


- **ORTHOGONAL**: S++ is designed to be consistent in all aspects of the language. This means that similar concepts are
  expressed in similar ways, reducing the learning curve and making it easier to predict how different parts of the
  language will behave. A key example is all types acting like first class objects, including numbers, booleans, voids,
  variants, functions, arrays, tuples, etc.


- **VERSATILE**: S++ is a general-purpose language, suitable for a wide range of applications. It supports
  multiple programming paradigms, including procedural, object-oriented, and functional programming. This versatility
  allows developers to choose the best approach for their specific use case, making S++ a flexible choice for various
  projects.


- **EFFICIENT**: S++ is designed to be efficient in both development and execution. Legacy programming language features
  have been re-evaluated and modern alternatives provided where appropriate. Forcing type inference reduces boilerplate,
  and features like coroutines and pattern matching simplify complex tasks. Compile time checks prevent difficulty
  runtime memory errors, reducing debugging time.


- **SAFETY**: Memory, type and thread safety are enforced at compile time, reducing the likelihood of runtime errors and
  security vulnerabilities. S++ employs a combination of ownership tracking, borrowing rules, and type checking to
  ensure that programs are safe and reliable. This focus on safety makes S++ a suitable choice for applications where
  reliability is critical.

## Key Features

- **EXPRESSION ORIENTED**: In S++, almost every structure is an expression, rather than a statement. This includes
  conditional branching conditional looping, anonymous scopes, etc. This allows for more concise and flexible code, as
  these constructs can be used in a wider variety of contexts, such as within other expressions or as part of larger
  computations.


- **ADVANCED TYPE SYSTEM**: All types in S++ are "first-class". Every single expression can be and is type inferred,
  meaning explicit types are only requires in minimal contexts. The type system supports generics, variant types, and
  type constraints, allowing for flexible and reusable code. Flow typing is also supported in pattern matching.


- **MEMORY SAFETY**: S++ employs a unique blend of memory safety techniques, including ownership tracking,
  borrowing rules, and the law of exclusivity. This ensures that memory is managed safely and efficiently, preventing
  common issues such as null pointer dereferencing, dangling pointers, and data races. There is a Rust-like borrow
  checker, but with second-class borrows, simplifying lifetime management.


- **CLASS SYSTEM**: Like Rust, S++ splits state and behavior, using classes for data and superimposition blocks for
  behavior. However, S++ classes can be superimposed over each other, allowing for traditional inheritance whilst
  isolating each inheritance in its own block. This allows for modular and reusable code, whilst keeping maintainability
  high.


- **SUPERIMPOSITION**: Similar to Rust's `impl` block, S++ uses superimposition blocks to define either methods, or
  entire type extensions, over another type. This allows method to simply be defined over a type, or for extending
  type (traditional inheritance), and overriding base method implementations. Base methods must be marked as abstract or
  virtual to be overridden.


- **MODULE SYSTEM**: Every file in an S++ project is a module. Modules, namespaces and the file system are all aligned,
  making it easy to organize and manage code. Modules aren't imported like in Python, but are all available from the
  common namespace between that module and the current module, like in Rust. Exporting symbols is done using the access
  modifier annotations.


- **FUNCTIONS**: Functions are rich in features, supporting overloading, recursion, optional and type-safe variadic
  parameters. Closures can capture variables with parameter conventions, and support full type inference. A function is
  either defined as a `fun` subroutine, or as a `cor` coroutine.


- **ASYNC**: Rather than functions being defined as `async` capable at the prototype site, function in S++ can be
  _called_ as `async`, resolving the traditional "color problem". Calling a function as async uses coroutine backend
  logic to run the target in the background, whilst immediately creating a `Fut[T]` future type that will hold the
  result. Instead of awaiting, the `async` call modifier can just be removed.


- **GENERICS**: S++ has a powerful generics system, allowing for the creation of type-safe and reusable components. They
  can be used with classes, functions, type statements and superimpositions, and can be required, optional or variadic.
  Both type and `cmp` non-type generic are supported, such as `Arr[T, cmp n: USize]`. For non-type generics, any type
  can be used, not just number and booleans.


- **CONCURRENCY**: Coroutines form the basis of S++'s concurrency model. They are defined using the `cor` keyword, and
  can be paused and resumed, allowing for cooperative multitasking. They also support bilateral data movement (subject
  to memory safety constraints), and can yield values with parameter conventions, such as borrows, forming the basis for
  all iteration.


- **PARALLELISM**: S++ uses threads, and associated primitive, to achieve parallelism. Threads can be created using the
  `Thread` type, and communicate using channels. Shared state is managed using mutexes and atomic types, ensuring
  thread safety. Higher level parallel constructs, such as parallel for loops, are provided as postfix methods over the
  iterator types.


- **ERROR HANDLING**: S++ doesn't have any Python-/Java-like exceptions. Instead, it uses a combination of `Opt[T]` and
  `Res[T, E]` types to represent optional values and results that may fail. This encourages explicit error handling,
  making it clear when a function can fail and how to handle such failures. The postfix `?` operator has been stolen
  from Rust to simplify propagating errors.


- **PATTERN MATCHING**: S++ has a powerful pattern matching system, allowing for complex data structures to be
  deconstructed and matched against patterns. This includes support for tuples, arrays, objects, variants, nested
  patterns, and guards. Flow typing is used in conjunction with variant destructuring to refine types based on matched
  patterns. Pattern matching can be used in `case` expressions and `let` statements.


- **LOOP CONTROL FLOW**: S++ provides advanced loop control flow mechanisms, including `skip`, `exit`, which can be
  stacked to interact with nested loops. Loops can also return values, allowing for more expressive and flexible
  iteration constructs. There is only 1 loop construct (`loop`), but either a boolean or iteration expression can be
  provided, determining if it is more like a traditional `while` or `for` loop.
