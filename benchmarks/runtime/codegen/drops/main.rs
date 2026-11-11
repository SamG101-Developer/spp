use std::hint::black_box;

fn main() {
    let n: u64 = black_box(50_000_000);
    let mut acc = 0u64;
    let mut i = 0u64;
    while i < n {
        let mut v: Vec<u64> = Vec::with_capacity(8);
        v.push(i);
        v.push(i % 13);
        acc = (acc + v[0] + v[1]) % 1000000007;
        drop(v);
        i += 1;
    }
    println!("{acc}");
}
