use std::hint::black_box;

fn main() {
    let n: usize = black_box(700);
    let mut a = vec![0i64; n * n];
    let mut b = vec![0i64; n * n];
    let mut c = vec![0i64; n * n];
    let mut state: u64 = black_box(99);
    for i in 0..n * n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        a[i] = ((state >> 33) % 100) as i64;
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        b[i] = ((state >> 33) % 100) as i64;
    }
    for i in 0..n {
        for k in 0..n {
            let aik = a[i * n + k];
            for j in 0..n { c[i * n + j] = c[i * n + j] + aik * b[k * n + j]; }
        }
    }
    let total: i64 = c.iter().sum();
    println!("{total}");
}
