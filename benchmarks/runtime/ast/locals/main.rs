use std::hint::black_box;

#[derive(Clone, Copy)]
struct Pair {
    a: u64,
    b: u64,
}

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut x: u64 = black_box(1);
    let mut y: u64 = black_box(2);
    let mut i = 0u64;
    while i < n {
        let t: u64;
        t = (x + y) % 1000003;
        let (p, q) = (t, x);
        let [r, _, s] = [q, p, y];
        let Pair { a, b } = Pair { a: r, b: s };
        let z = {
            let w = a * 3 + b;
            w % 999983
        };
        {
            x = z;
        }
        y = (y + i) % 1000003;
        i += 1;
    }
    println!("{x}\n{y}");
}
