use std::hint::black_box;

fn find(x: u64) -> Option<u64> {
    if x % 5 == 0 { return None; }
    Some(x * 2)
}

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut acc = 0u64;
    let mut i = 0u64;
    while i < n {
        match find(i) {
            Some(val) => acc = (acc + val) % 1000000007,
            None => acc += 1,
        }
        i += 1;
    }
    println!("{acc}");
}
