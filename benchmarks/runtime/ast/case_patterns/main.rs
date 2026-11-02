use std::hint::black_box;

#[derive(Clone, Copy)]
struct Point {
    x: u64,
    y: u64,
}

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut state: u64 = black_box(9);
    let mut acc = 0u64;
    let mut i = 0u64;
    while i < n {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        let a = state >> 60;
        let b = (state >> 56) & 15;
        let p = Point { x: a, y: b };
        let t = match (a, b) {
            (u, v) if u == v => u * 2,
            (u, v) if u > v => u - v,
            _ => 1,
        };
        let r = match [a, b] {
            [u, v] if u < 4 => v,
            _ => 2,
        };
        let o = match p {
            Point { x, y } if x == 3 => y + 1,
            Point { x, y } => x ^ y,
        };
        if let Point { x, y } = p && x > y { acc += 1; }
        acc = acc + t + r + o;
        i += 1;
    }
    println!("{acc}");
}
