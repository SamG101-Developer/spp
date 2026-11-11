use std::hint::black_box;

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut state: u64 = black_box(3);
    let mut acc = 0u64;
    let mut i = 0u64;
    while i < n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        let v = state >> 61;
        acc = match v {
            0 => acc + 1,
            1 => acc + 3,
            2 => acc ^ 0x55,
            3 => acc + (state & 0xff),
            4 => acc - (acc >> 3),
            5 => acc + 7,
            6 => acc | 0x100,
            _ => acc + v,
        };
        i += 1;
    }
    println!("{acc}");
}
