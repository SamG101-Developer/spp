use std::hint::black_box;

fn main() {
    let n: u64 = black_box(250_000);
    let mut acc = 0u64;
    for i in 0..n {
        let found = 'search: {
            for j in 0..2000u64 {
                if (i + j) % 977 == 0 { break 'search j; }
                if j % 3 == 0 { continue; }
                acc += 1;
            }
            5000
        };
        acc = acc + found;
    }
    println!("{acc}");
}
