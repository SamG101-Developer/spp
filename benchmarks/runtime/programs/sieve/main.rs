use std::hint::black_box;

fn main() {
    let n: usize = black_box(20_000_000);
    let mut composite = vec![false; n + 1];
    let mut count: u64 = 0;
    let mut sum: u64 = 0;
    let mut i = 2;
    while i <= n {
        if !composite[i] {
            count += 1;
            sum += i as u64;
            let mut j = i * i;
            while j <= n {
                composite[j] = true;
                j += i;
            }
        }
        i += 1;
    }
    println!("{count}\n{sum}");
}
