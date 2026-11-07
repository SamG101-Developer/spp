use std::hint::black_box;

fn main() {
    println!("{}", black_box(0u64));
}
