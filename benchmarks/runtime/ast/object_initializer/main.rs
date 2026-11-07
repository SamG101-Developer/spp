use std::hint::black_box;

#[derive(Clone, Copy)]
struct Point {
    x: i64,
    y: i64,
}

fn step(p: Point, k: i64) -> Point {
    Point { x: (p.x * 3 + p.y + k) % 1000003, y: (p.y * 5 + p.x) % 999983 }
}

fn main() {
    let n: i64 = black_box(100_000_000);
    let mut p = Point { x: black_box(1), y: black_box(2) };
    let mut i = 0i64;
    while i < n {
        p = step(p, i % 7);
        i += 1;
    }
    println!("{}\n{}", p.x, p.y);
}
