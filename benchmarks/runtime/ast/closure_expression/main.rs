use std::hint::black_box;

fn main() {
    let n: u64 = black_box(100_000_000);
    let k: u64 = black_box(17);
    let step = move |x: u64| -> u64 { (x * k + 1) % 1000003 };
    let mut acc: u64 = black_box(5);
    let mut i = 0u64;
    while i < n {
        acc = step(acc);
        i += 1;
    }
    println!("{acc}");
}
