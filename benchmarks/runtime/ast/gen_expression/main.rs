use std::hint::black_box;

fn main() {
    let n: u64 = black_box(50_000_000);
    let mut sum = 0u64;
    for x in 0..n {
        sum = (sum + x * 3) % 1000000007;
    }
    println!("{sum}");
}
