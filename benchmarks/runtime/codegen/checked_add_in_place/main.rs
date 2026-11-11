use std::hint::black_box;

fn main() {
    let n: usize = black_box(4096);
    let mut x: Vec<i64> = vec![black_box(1); n];
    let k: i64 = black_box(3);
    let passes: i64 = black_box(50_000);
    for _ in 0..passes {
        for i in 0..x.len() {
            x[i] = x[i] + k;
        }
    }
    println!("{}", x[0] + x[4095]);
}
