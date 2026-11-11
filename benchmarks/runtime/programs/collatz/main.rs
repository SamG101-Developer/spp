use std::hint::black_box;

fn main() {
    let limit: u64 = black_box(1_000_000);
    let (mut best, mut best_n, mut total) = (0u64, 0u64, 0u64);
    let mut n = 1u64;
    while n < limit {
        let mut x = n;
        let mut steps = 0u64;
        while x != 1 {
            if x % 2 == 0 { x /= 2; } else { x = x * 3 + 1; }
            steps += 1;
        }
        total += steps;
        if steps > best {
            best = steps;
            best_n = n;
        }
        n += 1;
    }
    println!("{best_n}\n{best}\n{total}");
}
