use std::hint::black_box;

fn main() {
    let n: usize = black_box(4096);
    let mut data = vec![0u64; n];
    let mut state: u64 = black_box(11);
    for i in 0..n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        data[i] = state >> 54;
    }
    let mut acc = 0u64;
    for _ in 0..25000 {
        let mut lo = 0usize;
        while lo + 16 <= n {
            let window = &data[lo..lo + 16];
            let mut j = 0usize;
            while j < window.len() {
                acc = acc + window[j];
                j += 1;
            }
            lo += 1;
        }
    }
    println!("{acc}");
}
