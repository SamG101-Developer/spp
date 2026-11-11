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

    for pass in 0..passes {
        for x in v.iter_mut() {
            *x = x.wrapping_mul(3).wrapping_add(pass);
        }
    }
    let mut sum = 0u64;
    for i in 0..n {
        sum = sum.wrapping_add(v[i]);
    }
    println!("{sum}");
}
