use std::hint::black_box;

fn main() {
    let n: usize = black_box(4096);
    let mut a: Vec<f64> = vec![black_box(0.5); n];
    let b: Vec<f64> = vec![black_box(0.5); n];
    let passes: i64 = black_box(25_000);
    for _ in 0..passes {
        for i in 0..a.len() {
            a[i] = a[i] * b[i] + 1.0;
        }
    }
    println!("{}", a[0].to_bits() + a[4095].to_bits());
}
