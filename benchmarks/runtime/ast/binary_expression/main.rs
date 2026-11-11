use std::hint::black_box;

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut a: u64 = black_box(1);
    let mut b: u64 = black_box(7);
    let mut c = 0u64;
    let mut i = 0u64;
    while i < n {
        a = (a * 31 + b) % 1000003;
        b = ((b ^ a) & 0xffff) | (i >> 3 & 1);
        c = c + a / (b + 1) - (c >> 7);
        if a < b { c += 1; }
        i += 1;
    }
    println!("{a}\n{b}\n{c}");
}
