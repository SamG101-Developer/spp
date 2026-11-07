use std::hint::black_box;

fn pop(x: u64) -> u64 {
    let a = x.wrapping_sub((x >> 1) & 0x5555555555555555);
    let b = (a & 0x3333333333333333).wrapping_add((a >> 2) & 0x3333333333333333);
    let c = (b.wrapping_add(b >> 4)) & 0x0f0f0f0f0f0f0f0f;
    c.wrapping_mul(0x0101010101010101) >> 56
}

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut state: u64 = black_box(7);
    let mut total = 0u64;
    let mut i = 0u64;
    while i < n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        total += pop(state);
        i += 1;
    }
    println!("{total}");
}
