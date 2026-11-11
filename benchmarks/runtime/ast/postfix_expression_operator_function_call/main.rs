use std::hint::black_box;

fn fib(n: u64) -> u64 {
    if n < 2 { return n; }
    fib(n - 1) + fib(n - 2)
}

fn main() {
    println!("{}", fib(black_box(39)));
}
