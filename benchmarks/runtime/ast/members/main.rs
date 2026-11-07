use std::hint::black_box;

struct Counter {
    total: u64,
    step: u64,
}

impl Counter {
    fn make(step: u64) -> Counter { Counter { total: 0, step } }
    fn advance(&mut self, by: u64) { self.total = (self.total + by * self.step) % 1000003; }
    fn peek(&self) -> u64 { self.total }
}

fn add_into(target: &mut u64, value: u64) {
    *target = (*target + value) % 1000000007;
}

fn main() {
    let n: u64 = black_box(100_000_000);
    let mut c = Counter::make(black_box(7));
    let mut acc = 0u64;
    let mut i = 0u64;
    while i < n {
        c.advance(i);
        add_into(&mut acc, c.peek());
        i += 1;
    }
    println!("{acc}");
}
