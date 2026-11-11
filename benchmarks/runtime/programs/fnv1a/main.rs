use std::hint::black_box;

fn main() {
    let n: u64 = black_box(200_000_000);
    let mut state: u64 = black_box(12345);
    let mut hash: u64 = 0xcbf29ce484222325;
    let mut i = 0u64;
    while i < n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        hash = (hash ^ (state >> 56)).wrapping_mul(0x100000001b3);
        i += 1;
    }
    println!("{hash}");
}
