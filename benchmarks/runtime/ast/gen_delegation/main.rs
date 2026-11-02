use std::hint::black_box;

fn main() {
    let n: u64 = black_box(50_000_000);
    let mut sum = 0u64;
    for x in 0..n {
        sum = (sum + x * 5) % 1000000007;
    }
    let mut count = 0u64;
    let mut it = black_box(0..n);
    while it.next().is_some() {
        count += 1;
    }
    println!("{sum}\n{count}");
}
