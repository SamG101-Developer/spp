use std::hint::black_box;

fn mix(acc: &mut u64, i: u64) {
    *acc = *acc ^ (i & 255);
}

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut acc = 0u64;
    let mut i = 0u64;
    while i < n {
        {
            acc = (acc * 3 + i) % 1000003;
            mix(&mut acc, i);
        }
        i += 1;
    }
    println!("{acc}");
}
