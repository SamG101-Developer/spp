use std::hint::black_box;

#[inline(never)]
fn apply(f: fn(u64) -> u64, x: u64) -> u64 {
    f(x)
}

fn triple(x: u64) -> u64 { (x * 3 + 1) % 1000003 }
fn halve(x: u64) -> u64 { x / 2 + 7 }

trait Shape {
    fn area(&self, k: u64) -> u64 { k }
}

struct Square {
    side: u64,
}

impl Shape for Square {
    fn area(&self, k: u64) -> u64 { self.side * self.side + k }
}

fn main() {
    let n: u64 = black_box(50_000_000);
    let mut acc: u64 = black_box(1);
    let mut i = 0u64;
    while i < n {
        acc = apply(triple, acc);
        acc = apply(halve, acc);
        let sq = Square { side: i % 100 };
        acc = (acc + sq.area(3)) % 1000003;
        i += 1;
    }
    println!("{acc}");
}
