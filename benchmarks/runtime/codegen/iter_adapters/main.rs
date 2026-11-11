use std::hint::black_box;

fn main() {
    let n: usize = black_box(4096);
    let passes: u64 = black_box(50_000);
    let mut state: u64 = black_box(12345);
    let mut v: Vec<u64> = Vec::with_capacity(n);
    for _ in 0..n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        v.push(state >> 16);
    }

    let mut sum = 0u64;
    for pass in 0..passes {
        sum = v
            .iter()
            .map(|x| *x ^ pass)
            .filter(|y| y & 1 == 1)
            .fold(sum, |acc, y| acc.wrapping_add(y));
    }
    println!("{sum}");
}
