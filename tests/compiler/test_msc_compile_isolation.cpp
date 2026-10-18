#include "test_macros.hpp"

// Two compiles in one process share nothing a program names. The language server compiles many times in one process
// (the suite runs one test per process, so it never sees this): a class named alike in a later compile is laid out
// from its own fields, not from a previous compile's same-named LLVM struct.

TEST(TestCompileIsolation, test_same_named_class_laid_out_again_in_a_later_compile) {
  build_temp_project(R"(
    cls T { }
    cls A { !public v: T }
    cls B { }
    type AB = A or B

    fun f(x: &AB) -> Void {
        case x of {
            is A(&v) { }
            else { }
        }
    }
)");

  build_temp_project(R"(
    cls B {
        !public a: std::number::S32
    }

    cls C {
        !public a: std::string::Str
    }

    cls A {
        !public a: std::boolean::Bool
    }

    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let obj = A()
        let x = obj.a
        std::mem::ops::drop(obj)
    }
)");
}
