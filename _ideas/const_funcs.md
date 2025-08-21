# Const functions

Functions will not be "markable" as `cmp`, the standard "compile time" keyword marker. What is going to happen, is that
every function will be assumed to be compile-time executable, in all contexts of that function being called. The
exception is if a function called a definite non-compile-time function. These definite non-compile-time functions
will **only** be ffi function calls, like `io`.

If all the arguments to a function are compile-time values (ie compile time variables or literals or results of
compile-time functions etc), then the function is automatically executed at compile time, and the result is injected
into the ASTs before codegen. Otherwise, standard IR is used to call the function. A variable is only non-compile-time
if is the result of a non-compile-time function, or it is a parameters, because there is no guarantee that the parameter
was a compile-time value or not.

A parameter can, however, be tagged as `cmp`, meaning it is always a compile time value, and giving it a non-compile
time value will throw an error.

The numeric addition functions will not have any `cmp` parameters, but can be called either as compile time executable,
like `1 + 2`, or runtime executable, like `a + b`, where `a` and `b` are variables.
