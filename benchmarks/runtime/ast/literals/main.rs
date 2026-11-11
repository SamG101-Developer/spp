use std::hint::black_box;

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut acc = 0u64;
    let mut f = 0.0f64;
    let mut i = 0u64;
    while i < n {
        let small = [1u64, 2, 3, 5, 8, 13, 21, 34];
        let mut window = [0u64; 4];
        window[(i % 4) as usize] = i;
        let (k, flag) = (i % 7, true);
        let c = 'x';
        let word = "benchmark";
        acc = acc + small[(i % 8) as usize] + window[0] + (k * 3) + word.len() as u64;
        if flag && c == 'x' { acc += 1; }
        f = f + 0.5;
        i += 1;
    }
    println!("{acc}\n{}", f as i64);
}
