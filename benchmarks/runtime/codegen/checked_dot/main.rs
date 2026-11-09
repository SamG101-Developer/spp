use std::hint::black_box;

fn main() {
    let n: usize = black_box(4096);
    let passes: i64 = black_box(50_000);
    let mut state: u64 = black_box(12345);
    let mut a: Vec<i64> = Vec::with_capacity(n);
    for _ in 0..n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        a.push((state >> 44) as i64);
    }
    let mut b: Vec<i64> = Vec::with_capacity(n);
    for _ in 0..n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        b.push((state >> 44) as i64);
    }
    let mut sum = 0i64;
    for pass in 0..passes {
        let mut dot = 0i64;
        for i in 0..n {
            dot = dot + a[i] * b[i];
        }
        sum = (sum + dot % 1000003 + pass) % 1000000007;
    }
    println!("{sum}");
}
