use std::hint::black_box;

fn check(x: u64) -> Result<u64, u64> {
    if !(x % 97 != 0) { return Err(x); }
    Ok(x * 3 % 1000003)
}

fn chain(x: u64) -> Result<u64, u64> {
    let a = check(x)?;
    let b = check(a + 1)?;
    Ok(a + b)
}

fn main() {
    let n: u64 = black_box(50_000_000);
    let (mut ok, mut failed) = (0u64, 0u64);
    let mut i = 0u64;
    while i < n {
        match chain(i) {
            Ok(val) => ok = (ok + val) % 1000000007,
            Err(_) => failed += 1,
        }
        i += 1;
    }
    println!("{ok}\n{failed}");
}
